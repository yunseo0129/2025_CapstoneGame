#include "Graphic_Device.h"

CGraphic_Device::CGraphic_Device()
{
}


CGraphic_Device::~CGraphic_Device()
{
	// 디바이스가 종료되기 전에 GPU가 모든 명령을 완료하도록 대기
	FlushCommandQueue();
}

void CGraphic_Device::BeforeRender()
{
	// 명령 레코드와 연결된 메모리 재사용
	// 연결된 명령 목록이 GPU에 완료되었을 때만 재설정 가능.
	ThrowIfFailed(m_pDirectCmdListAlloc->Reset());

	// 명령 목록은 ExecuteCommandList를 통해 명령 대기열에 추가된 후에 재설정 가능
	// 명령 목록을 재사용하면 메모리가 재사용 됨
	ThrowIfFailed(m_pCommandList->Reset(m_pDirectCmdListAlloc.Get(), nullptr));

	// 자원 사용에 대한 상태 전환 (Present -> RenderTarget)
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = CurrentBackBuffer();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	m_pCommandList->ResourceBarrier(1, &barrier);

	// 뷰포트와 ScissorRects 재설정
	m_pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
	m_pCommandList->RSSetViewports(1, &m_ScreenViewport);
	m_pCommandList->RSSetScissorRects(1, &m_ScissorRect);

	// 백 버퍼와 깊이 버퍼 초기화.
	m_pCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	m_pCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// 렌더링 버퍼 지정
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentBackBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DepthStencilView();
	m_pCommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);
}

void CGraphic_Device::AfterRender()
{
	// 자원 사용에 대한 상태 전환 (RenderTarget -> Present)
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = CurrentBackBuffer();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	m_pCommandList->ResourceBarrier(1, &barrier);

	// command list에 기록 완료
	ThrowIfFailed(m_pCommandList->Close());

	// 명령 대기열에 명령 목록 추가
	ID3D12CommandList* cmdsLists[] = { m_pCommandList.Get() };
	m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 스왑체인 프로젝트
	ThrowIfFailed(m_pSwapChain->Present(0, 0));
	m_iCurrBackBuffer = (m_iCurrBackBuffer + 1) % m_iSwapChainBufferCount;

	// 동기화 코드 -> 추후 수정 필요
	FlushCommandQueue();
}


bool CGraphic_Device::InitDirect3D(HWND _hwnd, EngineContext* _pcontext)
{
	m_hMainWnd = _hwnd;
	CreateDevice();

#ifdef _DEBUG
	LogAdapters();
#endif

	CreateCommandObjects();
	CreateSwapChain();
	CreateRtvAndDsvDescriptorHeaps();
	// CreateRootSignature(); level에서 처리

	_pcontext->device = m_pD3dDevice.Get();
	_pcontext->cmdList = m_pCommandList.Get();
	_pcontext->cmdQueue = m_pCommandQueue.Get();
	_pcontext->rtvHeap = m_pRtvHeap.Get();
	_pcontext->dsvHeap = m_pDsvHeap.Get();
	// srvHeap는 다른 곳에서 처리

	Safe_AddRef(m_pD3dDevice);
	Safe_AddRef(m_pCommandList);
	Safe_AddRef(m_pCommandQueue);
	Safe_AddRef(m_pRtvHeap);
	Safe_AddRef(m_pDsvHeap);

	return true;
}

void CGraphic_Device::CreateDevice()
{
#if defined(DEBUG) || defined(_DEBUG) 
	// 디버그 레이어 활성화
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_pDxgiFactory)));

// 하드웨어 디바이스 생성
HRESULT hardwareResult = D3D12CreateDevice(
	nullptr,             // default adapter
	D3D_FEATURE_LEVEL_11_0,
	IID_PPV_ARGS(&m_pD3dDevice));

// 하드웨어 디바이스 생성 실패 시 WARP 디바이스 생성
if (FAILED(hardwareResult))
{
	ComPtr<IDXGIAdapter> pWarpAdapter;
	ThrowIfFailed(m_pDxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

	ThrowIfFailed(D3D12CreateDevice(
		pWarpAdapter.Get(),
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&m_pD3dDevice)));
}

ThrowIfFailed(m_pD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
	IID_PPV_ARGS(&m_pFence)));

m_iRtvDescriptorSize = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
m_iDsvDescriptorSize = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
m_iCbvSrvUavDescriptorSize = m_pD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

// 다중샘플링 앤티앨리어싱(MSAA) 품질 수준 확인
// 샘플 카운트는 1,2,4,8,16 중 하나여야 함
// 품질 수준은 0에서 시작하여 최대값은 CheckFeatureSupport로 조회한 값 -1

D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
msQualityLevels.Format = m_BackBufferFormat;
msQualityLevels.SampleCount = 4;
msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
msQualityLevels.NumQualityLevels = 0;
ThrowIfFailed(m_pD3dDevice->CheckFeatureSupport(
	D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
	&msQualityLevels,
	sizeof(msQualityLevels)));

m_i4xMsaaQuality = msQualityLevels.NumQualityLevels;
assert(m_i4xMsaaQuality > 0 && "Unexpected MSAA quality level.");
}

void CGraphic_Device::CreateSwapChain()
{
	// 스왑체인 생성 전에 기존 스왑체인 해제
	m_pSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = Client::g_iWinSizeX;
	sd.BufferDesc.Height = Client::g_iWinSizeY;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = m_BackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = m_is4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m_is4xMsaaState ? (m_i4xMsaaQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = m_iSwapChainBufferCount;
	sd.OutputWindow = m_hMainWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// 스왑체인 생성
	ThrowIfFailed(m_pDxgiFactory->CreateSwapChain(
		m_pCommandQueue.Get(),
		&sd,
		m_pSwapChain.GetAddressOf()));
}

void CGraphic_Device::CreateRtvAndDsvDescriptorHeaps()
{
	// RTV와 DSV 디스크립터 힙 생성

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = m_iSwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(m_pD3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(m_pRtvHeap.GetAddressOf())));


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(m_pD3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(m_pDsvHeap.GetAddressOf())));
}




void CGraphic_Device::CreateCommandObjects()
{
	// 커맨드 큐, 커맨드 얼로케이터, 커맨드 리스트 생성
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(m_pD3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCommandQueue)));

	ThrowIfFailed(m_pD3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_pDirectCmdListAlloc.GetAddressOf())));

	ThrowIfFailed(m_pD3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_pDirectCmdListAlloc.Get(), // 명령 할당자
		nullptr,                   // 초기 파이프라인 상태 객체
		IID_PPV_ARGS(m_pCommandList.GetAddressOf())));

	// 커맨드 리스트는 생성과 동시에 기록 상태이므로, GPU에서 실행하기 전에 반드시 닫아주어야 한다.
	m_pCommandList->Close();
}

void CGraphic_Device::FlushCommandQueue()
{
	// 현재 펜스 값 증가
	m_iCurrentFence++;

	// 커맨드 큐에 신호를 보낸다.
	// 이 신호는 GPU가 커맨드 큐에 제출된 모든 명령을 실행한 후에야 처리됨
	// 즉, 이 신호가 처리되었다는 것은 GPU가 모든 명령을 완료했다는 의미
	ThrowIfFailed(m_pCommandQueue->Signal(m_pFence.Get(), m_iCurrentFence));

	// GPU가 현재 펜스 값에 도달했는지 확인
	if (m_pFence->GetCompletedValue() < m_iCurrentFence)
	{
		HANDLE eventHandle = CreateEvent(nullptr, false, false, NULL);

		// 펜스가 현재 펜스 값에 도달하면 이벤트가 신호됨 
		ThrowIfFailed(m_pFence->SetEventOnCompletion(m_iCurrentFence, eventHandle));

		// 이벤트가 신호될 때까지 대기
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

bool CGraphic_Device::Get4xMsaaState()const
{
	return m_is4xMsaaState;
}

void CGraphic_Device::Set4xMsaaState(bool _value)
{
	if (m_is4xMsaaState != _value)
	{
		m_is4xMsaaState = _value;

		// 다중샘플링 앤티앨리어싱 설정이 바뀌면 스왑체인과 관련된 자원을 모두 다시 만들어야 한다.
		CreateSwapChain();
		OnResize();
	}
}

void CGraphic_Device::OnResize()
{
	// 기존 디바이스와 스왑체인, 커맨드 얼로케이터가 유효한지 확인
	assert(m_pD3dDevice);
	assert(m_pSwapChain);
	assert(m_pDirectCmdListAlloc);

	// 리소스 변경전에 Flush
	FlushCommandQueue();

	ThrowIfFailed(m_pCommandList->Reset(m_pDirectCmdListAlloc.Get(), nullptr));

	// 이전 리소스 해제
	for (int i = 0; i < m_iSwapChainBufferCount; ++i)
		m_pSwapChainBuffer[i].Reset();
	m_pDepthStencilBuffer.Reset();

	// 스왑체인 버퍼 크기 조절
	ThrowIfFailed(m_pSwapChain->ResizeBuffers(
		m_iSwapChainBufferCount,
		Client::g_iWinSizeX, Client::g_iWinSizeY,
		m_BackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	m_iCurrBackBuffer = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_pRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < m_iSwapChainBufferCount; i++)
	{
		ThrowIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pSwapChainBuffer[i])));
		m_pD3dDevice->CreateRenderTargetView(m_pSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.ptr += m_iRtvDescriptorSize;
	}

	// 깊이/스텐실 버퍼 생성
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = Client::g_iWinSizeX;
	depthStencilDesc.Height = Client::g_iWinSizeY;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;

	// 1. SRV 형식: DXGI_FORMAT_R24_UNORM_X8_TYPEless
	// 2. DSV 형식: DXGI_FORMAT_D24_UNORM_S8_UINT
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

	depthStencilDesc.SampleDesc.Count = m_is4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m_is4xMsaaState ? (m_i4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = m_DepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES heapProps;
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	ThrowIfFailed(m_pD3dDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(m_pDepthStencilBuffer.GetAddressOf())));

	// 리소스 형식 설정
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = m_DepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	m_pD3dDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	// 리소스 상태 전환 (COMMON -> DEPTH_WRITE)
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_pDepthStencilBuffer.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;


	m_pCommandList->ResourceBarrier(1, &barrier);

	// 명령 목록 닫기 및 실행
	ThrowIfFailed(m_pCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_pCommandList.Get() };
	m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 동기화
	FlushCommandQueue();

	// client 영역에 맞게 뷰포트와 시저렉트 설정
	m_ScreenViewport.TopLeftX = 0;
	m_ScreenViewport.TopLeftY = 0;
	m_ScreenViewport.Width = static_cast<float>(Client::g_iWinSizeX);
	m_ScreenViewport.Height = static_cast<float>(Client::g_iWinSizeY);
	m_ScreenViewport.MinDepth = 0.0f;
	m_ScreenViewport.MaxDepth = 1.0f;

	m_ScissorRect = { 0, 0, Client::g_iWinSizeX, Client::g_iWinSizeY };
}

ID3D12Resource* CGraphic_Device::CurrentBackBuffer()const
{
	// 현재 백 버퍼 리소스 반환
	return m_pSwapChainBuffer[m_iCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE CGraphic_Device::CurrentBackBufferView()const
{
	// 현재 백 버퍼 뷰 반환
	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += m_iCurrBackBuffer * m_iRtvDescriptorSize;
	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE CGraphic_Device::DepthStencilView()const
{
	// 깊이/스텐실 뷰 반환
	return m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void CGraphic_Device::LogAdapters()
{
	// 어댑터 열거
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (m_pDxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void CGraphic_Device::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	// 어댑터의 출력 열거
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, m_BackBufferFormat);

		ReleaseCom(output);

		++i;
	}
}

void CGraphic_Device::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	// 출력의 디스플레이 모드 열거
	UINT count = 0;
	UINT flags = 0;

	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";

		::OutputDebugString(text.c_str());
	}
}

CGraphic_Device* CGraphic_Device::Create(HWND _hwnd, EngineContext* _pcontext)
{
	CGraphic_Device* pInstance = new CGraphic_Device();
	if (!pInstance->InitDirect3D(_hwnd, _pcontext))
	{
		Safe_Release(pInstance);
		assert(!"CGraphic_Device::Create - Failed to Create CGraphic_Device");
	}
	return pInstance;
}

void CGraphic_Device::Free()
{

	Safe_Release(m_pD3dDevice);
	Safe_Release(m_pCommandQueue);
	Safe_Release(m_pCommandList);
	Safe_Release(m_pRtvHeap);
	Safe_Release(m_pDsvHeap);
	
}