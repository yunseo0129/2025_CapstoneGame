#pragma once

#include "Base.h"
#include "Timer.h"
#include "Scene.h"


class CMainApp : public CBase
{
public:
	CMainApp();
	~CMainApp();

public:
	
	void MouseEvent(HWND _hWnd, FLOAT _timeElapsed);
	void KeyboardEvent(FLOAT _timeElapsed);
	void MouseEvent(UINT _message, LPARAM _lParam);
	void KeyboardEvent(HWND _hWnd, UINT _message, WPARAM _wParam, LPARAM _lParam);
	void SetActive(BOOL _isActive);


	void Update();
	HRESULT Render();
	void BuildObjects();
	void FrameAdvance();

	void BeforeRender();
	void AfterRender();

	bool Initialize(HINSTANCE _hInstance, HWND _hMainWnd);
	bool Get4xMsaaState()const;
	void Set4xMsaaState(bool _value);
	void OnResize();
	
protected:
	bool InitDirect3D();
	void CreateDevice();
	void CreateCommandObjects();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateSwapChain();
	void CreateRootSignature();

	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* _adapter);
	void LogOutputDisplayModes(IDXGIOutput* _output, DXGI_FORMAT _format);

protected:

	bool m_isActivate;

	HINSTANCE m_hAppInst = nullptr; // 핸들정보
	HWND      m_hMainWnd = nullptr; // 메인윈도우 핸들정보
	bool      m_isAppPaused = false;  // 퍼즈인가?
	bool      m_isMinimized = false;  // 가장 작은 상태인가?
	bool      m_isMaximized = false;  // 가장 큰 상태인가?
	bool      m_isResizing = false;   // 드	래그로 크기조절중인가?
	bool      m_isFullscreenState = false;// 풀스크린 상태인가?

	//다중샘플링 앤티앨리어싱(MSAA) 설정
	bool      m_is4xMsaaState = false;    // 4x다중샘플링 앤티앨리어싱 사용 여부
	UINT      m_i4xMsaaQuality = 0;      // 퀄리티 레벨

	ComPtr<IDXGIFactory4> m_pDxgiFactory;
	ComPtr<IDXGISwapChain> m_pSwapChain;
	ComPtr<ID3D12Device> m_pD3dDevice;

	ComPtr<ID3D12Fence> m_pFence;
	UINT64 m_iCurrentFence = 0;

	ComPtr<ID3D12CommandQueue> m_pCommandQueue;
	ComPtr<ID3D12CommandAllocator> m_pDirectCmdListAlloc;
	ComPtr<ID3D12GraphicsCommandList> m_pCommandList;

	static const int m_iSwapChainBufferCount = 2;
	int m_iCurrBackBuffer = 0;
	ComPtr<ID3D12Resource> m_pSwapChainBuffer[m_iSwapChainBufferCount];
	ComPtr<ID3D12Resource> m_pDepthStencilBuffer;

	ComPtr<ID3D12DescriptorHeap> m_pRtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_pDsvHeap;
	ComPtr<ID3D12RootSignature>	 m_pRootSignature;

	D3D12_VIEWPORT m_ScreenViewport;
	D3D12_RECT m_ScissorRect;

	UINT m_iRtvDescriptorSize = 0;
	UINT m_iDsvDescriptorSize = 0;
	UINT m_iCbvSrvUavDescriptorSize = 0;

	wstring m_strMainWndCaption = L"HelloDinner";
	D3D_DRIVER_TYPE m_d3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT m_BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	int m_iClientHeight = 600;

	CTimer								m_CTimer;

	unique_ptr<CScene>					m_pScene;
};

