#include "Graphic_Device.h"

CGraphic_Device::CGraphic_Device()
{
	for ( int i = 0; i < m_iSwapChainBufferCount; i++ ) m_nFenceValues[i] = 0;
	m_ScreenViewport = { 0, 0, Client::g_iWinSizeX, Client::g_iWinSizeY, 0.0f, 1.0f };
	m_ScissorRect = { 0, 0, Client::g_iWinSizeX, Client::g_iWinSizeY };
}

CGraphic_Device::~CGraphic_Device()
{

}

void CGraphic_Device::ResetCmdList ()
{
	// 명령 목록은 ExecuteCommandList를 통해 명령 대기열에 추가된 후에 재설정 가능
	// 명령 목록을 재사용하면 메모리가 재사용 됨
	ThrowIfFailed ( m_pCommandList.Get ()->Reset ( m_pDirectCmdListAlloc.Get () , nullptr ) );
	//MSG_BOX ( "CmdList Reset" );
}

void CGraphic_Device::BeforeRender(const _float4& vClearColor)
{
	// 명령 레코드와 연결된 메모리 재사용
	// 연결된 명령 목록이 GPU에 완료되었을 때만 재설정 가능.
	ThrowIfFailed ( m_pDirectCmdListAlloc->Reset () );

	ResetCmdList ();
	
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
	m_pCommandList->RSSetViewports(1, &m_ScreenViewport);
	m_pCommandList->RSSetScissorRects(1, &m_ScissorRect);

	// 백 버퍼와 깊이 버퍼 초기화.
	m_pCommandList->ClearRenderTargetView(CurrentBackBufferView(), (_float*)&vClearColor, 0, nullptr);
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

	m_pCommandList->ResourceBarrier (1 , &barrier);

	CloseCmdList();


#ifdef _WITH_PRESENT_PARAMETERS
	DXGI_PRESENT_PARAMETERS dxgiPresentParameters;
	dxgiPresentParameters.DirtyRectsCount = 0;
	dxgiPresentParameters.pDirtyRects = NULL;
	dxgiPresentParameters.pScrollRect = NULL;
	dxgiPresentParameters.pScrollOffset = NULL;
	m_pSwapChain->Present1(1, 0, &dxgiPresentParameters);
#else
#ifdef _WITH_SYNCH_SWAPCHAIN
	m_pSwapChain->Present(1, 0);
#else
	HRESULT hr = m_pSwapChain->Present ( 0 , 0 );
	if ( FAILED ( hr ) )
	{
		if ( hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET )
		{
			HRESULT removedReason = m_pD3dDevice->GetDeviceRemovedReason ();
			wchar_t buf[128];
			swprintf_s ( buf , L"Device Removed! Reason: 0x%08X\n" , ( unsigned int )removedReason );
			OutputDebugString ( buf );

			// DRED 데이터 조회
			ComPtr<ID3D12DeviceRemovedExtendedData> pDred;
			if ( SUCCEEDED ( m_pD3dDevice->QueryInterface ( IID_PPV_ARGS ( &pDred ) ) ) )
			{
				D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
				if ( SUCCEEDED ( pDred->GetAutoBreadcrumbsOutput ( &DredAutoBreadcrumbsOutput ) ) )
				{
					OutputDebugString ( L"[DRED] Auto Breadcrumbs:\n" );
					for ( auto pNode = DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode; pNode; pNode = pNode->pNext )
					{
						swprintf_s ( buf , L"  Command List: %s, Count: %u, Completed: %u\n" ,
							pNode->pCommandListDebugNameW ? pNode->pCommandListDebugNameW : L"(unnamed)" ,
							pNode->BreadcrumbCount ,
							pNode->pLastBreadcrumbValue ? *pNode->pLastBreadcrumbValue : 0 );
						OutputDebugString ( buf );
					}
				}
				D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
				if ( SUCCEEDED ( pDred->GetPageFaultAllocationOutput ( &DredPageFaultOutput ) ) )
				{
					swprintf_s ( buf , L"[DRED] Page Fault VA: 0x%llX\n" , DredPageFaultOutput.PageFaultVA );
					OutputDebugString ( buf );
				}
			}

			ThrowIfFailed ( removedReason );
		}
		ThrowIfFailed ( hr );
	}
#endif
#endif

	MoveToNextFrame();
}

void CGraphic_Device::CloseCmdList ()
{
	// command list에 기록 완료
	ThrowIfFailed(m_pCommandList->Close ());

	// 명령 대기열에 명령 목록 추가
	ID3D12CommandList* cmdsLists[] = { m_pCommandList.Get () };
	m_pCommandQueue->ExecuteCommandLists ( _countof ( cmdsLists ) , cmdsLists );

	//MSG_BOX ( "CmdList Close" );
	WaitForGpuComplete ();
}

bool CGraphic_Device::InitDirect3D(HWND& _hwnd, EngineContext* _pcontext)
{
	m_hMainWnd = _hwnd;
	CreateDevice();

#ifdef _DEBUG
	LogAdapters();
#endif

	CreateCommandObjects();
	CreateRtvAndDsvDescriptorHeaps();
	CreateSwapChain ();
	CreateRenderTargetViews ();
	CreateDepthStencilView ();

	_pcontext->device = m_pD3dDevice.Get();
	_pcontext->cmdList = m_pCommandList.Get();
	_pcontext->cmdQueue = m_pCommandQueue.Get();
	_pcontext->rtvHeap = m_pRtvHeap.Get();
	_pcontext->dsvHeap = m_pDsvHeap.Get();
	// srvHeap는 다른 곳에서 처리

	return true;
}

void CGraphic_Device::CreateDevice()
{
	UINT nDXGIFactoryFlags = 0;
#if defined(DEBUG) || defined(_DEBUG) 
	// 디버그 레이어 활성화
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();

		ComPtr<ID3D12DeviceRemovedExtendedDataSettings> pDredSettings;
		if ( SUCCEEDED ( D3D12GetDebugInterface ( IID_PPV_ARGS ( &pDredSettings ) ) ) )
		{
			pDredSettings->SetAutoBreadcrumbsEnablement ( D3D12_DRED_ENABLEMENT_FORCED_ON );
			pDredSettings->SetPageFaultEnablement ( D3D12_DRED_ENABLEMENT_FORCED_ON );
		}
	}
nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

ThrowIfFailed(CreateDXGIFactory2( nDXGIFactoryFlags, IID_PPV_ARGS(&m_pDxgiFactory)));

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

for ( UINT i = 0; i < m_iSwapChainBufferCount; i++ ) m_nFenceValues[i] = 0;
m_hFenceEvent = ::CreateEvent ( NULL , FALSE , FALSE , NULL );
}

void CGraphic_Device::CreateSwapChain()
{
	// 스왑체인 생성 전에 기존 스왑체인 해제
	m_pSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferDesc.Width = Client::g_iWinSizeX;
	sd.BufferDesc.Height = Client::g_iWinSizeY;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = m_BackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = m_iSwapChainBufferCount;
	sd.OutputWindow = m_hMainWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// 스왑체인 생성
	ComPtr<IDXGISwapChain> swapChain;
	ThrowIfFailed ( m_pDxgiFactory->CreateSwapChain (
		m_pCommandQueue.Get () ,
		&sd ,
		swapChain.GetAddressOf () ) );

	ThrowIfFailed ( swapChain.As ( &m_pSwapChain ) );

	m_pDxgiFactory->MakeWindowAssociation ( m_hMainWnd , DXGI_MWA_NO_ALT_ENTER );
	m_iCurrBackBuffer = m_pSwapChain->GetCurrentBackBufferIndex ();
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

void CGraphic_Device::WaitForGpuComplete()
{
	const UINT64 nFenceValue = ++m_nFenceValues[m_iCurrBackBuffer];
	HRESULT hResult = m_pCommandQueue->Signal(m_pFence.Get(), nFenceValue);

	if (m_pFence->GetCompletedValue() < nFenceValue)
	{
		hResult = m_pFence->SetEventOnCompletion(nFenceValue, m_hFenceEvent);
		::WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}

void CGraphic_Device::MoveToNextFrame()
{
	m_iCurrBackBuffer = m_pSwapChain.Get()->GetCurrentBackBufferIndex();
	UINT64 nFenceValue = ++m_nFenceValues[m_iCurrBackBuffer];
	HRESULT hResult = m_pCommandQueue->Signal(m_pFence.Get(), nFenceValue);

	if (m_pFence->GetCompletedValue() < nFenceValue)
	{
		hResult = m_pFence->SetEventOnCompletion(nFenceValue, m_hFenceEvent);
		::WaitForSingleObject(m_hFenceEvent, INFINITE);
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
		CreateRenderTargetViews ();
		CreateDepthStencilView ();
	}
}

void CGraphic_Device::CreateRenderTargetViews ()
{
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart ();
	for ( UINT i = 0; i < m_iSwapChainBufferCount; i++ )
	{
		ThrowIfFailed ( m_pSwapChain->GetBuffer ( i , __uuidof( ID3D12Resource ) , ( void** )&m_pSwapChainBuffer[i] ) );
		m_pD3dDevice->CreateRenderTargetView ( m_pSwapChainBuffer[i].Get() , NULL , d3dRtvCPUDescriptorHandle);
		d3dRtvCPUDescriptorHandle.ptr += m_iRtvDescriptorSize;
	}
}

void CGraphic_Device::CreateDepthStencilView ()
{
	D3D12_RESOURCE_DESC d3dResourceDesc;
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = Client::g_iWinSizeX;
	d3dResourceDesc.Height = Client::g_iWinSizeY;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dResourceDesc.SampleDesc.Count = 1;
	d3dResourceDesc.SampleDesc.Quality = 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES d3dHeapProperties;
	::ZeroMemory ( &d3dHeapProperties , sizeof ( D3D12_HEAP_PROPERTIES ) );
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapProperties.CreationNodeMask = 1;
	d3dHeapProperties.VisibleNodeMask = 1;

	D3D12_CLEAR_VALUE d3dClearValue;
	d3dClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dClearValue.DepthStencil.Depth = 1.0f;
	d3dClearValue.DepthStencil.Stencil = 0;

	m_pD3dDevice->CreateCommittedResource ( &d3dHeapProperties , D3D12_HEAP_FLAG_NONE , &d3dResourceDesc , D3D12_RESOURCE_STATE_DEPTH_WRITE , &d3dClearValue , __uuidof( ID3D12Resource ) , ( void** )&m_pDepthStencilBuffer );

	D3D12_DEPTH_STENCIL_VIEW_DESC d3dDepthStencilViewDesc;
	::ZeroMemory ( &d3dDepthStencilViewDesc , sizeof ( D3D12_DEPTH_STENCIL_VIEW_DESC ) );
	d3dDepthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dDepthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	d3dDepthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle = m_pDsvHeap->GetCPUDescriptorHandleForHeapStart ();
	m_pD3dDevice->CreateDepthStencilView ( m_pDepthStencilBuffer.Get() , &d3dDepthStencilViewDesc , d3dDsvCPUDescriptorHandle);
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
	// 디바이스가 종료되기 전에 GPU가 모든 명령을 완료하도록 대기
	WaitForGpuComplete();
	if (m_hFenceEvent)
	{
		::CloseHandle(m_hFenceEvent);
		m_hFenceEvent = nullptr;
	}
	// ComPtr은 Reset() 또는 소멸자에서 자동 Release되지만,
	// 명시적으로 해제 순서를 지정하는 것이 안전합니다.
	m_pDepthStencilBuffer.Reset();
	for (int i = 0; i < m_iSwapChainBufferCount; ++i)
		m_pSwapChainBuffer[i].Reset();

	m_pRootSignature.Reset();
	m_pFence.Reset();
	m_pCommandList.Reset();
	m_pDirectCmdListAlloc.Reset();
	m_pCommandQueue.Reset();

	m_pDsvHeap.Reset();
	m_pRtvHeap.Reset();
	m_pSwapChain.Reset();
	// Device Reset 전에 남은 객체 확인
#ifdef _DEBUG
	{
		ComPtr<ID3D12DebugDevice> debugDevice;
		if (SUCCEEDED(m_pD3dDevice->QueryInterface(IID_PPV_ARGS(&debugDevice))))
		{
			m_pD3dDevice.Reset(); // Device 먼저 해제
			debugDevice->ReportLiveDeviceObjects(
				D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
			// debugDevice가 스코프 종료 시 자동 해제
		}
	}
#else
	m_pD3dDevice.Reset();
#endif

	m_pDxgiFactory.Reset();
	__super::Free();

}