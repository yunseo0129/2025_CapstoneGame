#pragma once

#include "Base.h"
#include "Client_Defines.h"

class CMainApp : public CBase
{
public:
	CMainApp();
	~CMainApp();

public:
	bool Initialize(HINSTANCE hInstance, HWND hMainWnd);
	void Update(float fTimeDelta);
	HRESULT Render();
	void FrameAdvance();
	bool Get4xMsaaState()const;
	void Set4xMsaaState(bool value);
	void OnResize();
	
protected:
	bool InitDirect3D();
	void CreateDevice();
	void CreateCommandObjects();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateSwapChain();

	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

protected:
	HINSTANCE mhAppInst = nullptr; // 핸들정보
	HWND      mhMainWnd = nullptr; // 메인윈도우 핸들정보
	bool      mAppPaused = false;  // 퍼즈인가?
	bool      mMinimized = false;  // 가장 작은 상태인가?
	bool      mMaximized = false;  // 가장 큰 상태인가?
	bool      mResizing = false;   // 드	래그로 크기조절중인가?
	bool      mFullscreenState = false;// 풀스크린 상태인가?

	//다중샘플링 앤티앨리어싱(MSAA) 설정
	bool      m4xMsaaState = false;    // 4x다중샘플링 앤티앨리어싱 사용 여부
	UINT      m4xMsaaQuality = 0;      // 퀄리티 레벨

	ComPtr<IDXGIFactory4> mdxgiFactory;
	ComPtr<IDXGISwapChain> mSwapChain;
	ComPtr<ID3D12Device> md3dDevice;

	ComPtr<ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;

	ComPtr<ID3D12CommandQueue> mCommandQueue;
	ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	ComPtr<ID3D12GraphicsCommandList> mCommandList;

	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
	ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	ComPtr<ID3D12Resource> mDepthStencilBuffer;

	ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	wstring mMainWndCaption = L"HelloDinner";
	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	int mClientWidth = 800;
	int mClientHeight = 600;
};

