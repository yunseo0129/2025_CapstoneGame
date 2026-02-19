#pragma once

#include "Base.h"

class CGraphic_Device : public CBase
{
public:
	CGraphic_Device();
	~CGraphic_Device();

	void ResetCmdList ();

public:
	void BeforeRender(const _float4& vClearColor);
	void AfterRender();

	void CloseCmdList ();
	
	const ComPtr<ID3D12GraphicsCommandList>& GetCommandList() const { return m_pCommandList; }
private:
	bool InitDirect3D(HWND& _hwnd, EngineContext* _pcontext);
	bool Get4xMsaaState()const;
	void Set4xMsaaState(bool _value);
	void CreateRenderTargetViews ();
	void CreateDepthStencilView ();
	void CreateDevice();
	void CreateCommandObjects();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateSwapChain();
	void WaitForGpuComplete();
	void MoveToNextFrame();

	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* _adapter);
	void LogOutputDisplayModes(IDXGIOutput* _output, DXGI_FORMAT _format);

private:
	// 활성화 상태 플래그 (아직 사용 X)
	// _bool m_isActivate;

	// 메인윈도우 핸들정보 (swapchain생성시 필요)
	HWND      m_hMainWnd = nullptr; 

	// 드래그로 크기조절 관련 플래그 (아직 사용 X)
	_bool      m_isAppPaused = false;  // 퍼즈인가?
	_bool      m_isMinimized = false;  // 가장 작은 상태인가?
	_bool      m_isMaximized = false;  // 가장 큰 상태인가?
	_bool      m_isResizing = false;   // 드	래그로 크기조절중인가?
	_bool      m_isFullscreenState = false;// 풀스크린 상태인가?

	//다중샘플링 앤티앨리어싱(MSAA) 설정
	_bool      m_is4xMsaaState = false;    // 4x다중샘플링 앤티앨리어싱 사용 여부
	_uint      m_i4xMsaaQuality = 0;      // 퀄리티 레벨


	// Graphics
	ComPtr<IDXGIFactory4> m_pDxgiFactory;
	ComPtr<IDXGISwapChain3> m_pSwapChain;
	ComPtr<ID3D12Device> m_pD3dDevice;

	ComPtr<ID3D12CommandQueue> m_pCommandQueue;
	ComPtr<ID3D12CommandAllocator> m_pDirectCmdListAlloc;
	ComPtr<ID3D12GraphicsCommandList> m_pCommandList;

	static const _int m_iSwapChainBufferCount = 2;
	_int			m_iCurrBackBuffer = 0;
	ComPtr<ID3D12Resource> m_pSwapChainBuffer[m_iSwapChainBufferCount];
	ComPtr<ID3D12Resource> m_pDepthStencilBuffer;

	ComPtr<ID3D12DescriptorHeap> m_pRtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_pDsvHeap;
	ComPtr<ID3D12RootSignature>	 m_pRootSignature;

	ComPtr<ID3D12Fence> m_pFence;
	UINT64				m_nFenceValues[m_iSwapChainBufferCount];
	HANDLE				m_hFenceEvent = nullptr;

	D3D12_VIEWPORT m_ScreenViewport;
	D3D12_RECT m_ScissorRect;

	_uint m_iRtvDescriptorSize = 0;
	_uint m_iDsvDescriptorSize = 0;
	_uint m_iCbvSrvUavDescriptorSize = 0;

	D3D_DRIVER_TYPE m_d3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT m_BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

public:
	static CGraphic_Device* Create(HWND _hwnd, EngineContext* _pcontext);
	void Free();
};

