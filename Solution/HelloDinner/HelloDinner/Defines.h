#pragma once

//-------------------------------------------------------------
// namespace 
//-------------------------------------------------------------
namespace Client
{
	constexpr int g_iWinSizeX = 1280;
	constexpr int g_iWinSizeY = 720;
}

extern HINSTANCE g_hInst;
extern HWND g_hWnd;

namespace RootParameter
{
    constexpr UINT GameObject = 0;
    constexpr UINT Camera = 1;
    constexpr UINT TextureCube = 2;
    constexpr UINT Texture2D = 3;
	constexpr UINT TextureArray = 4;
}

namespace DescriptorRange
{
    constexpr UINT Texture = 0;
    constexpr UINT TextureCube = 1;
}

namespace Engine
{
    enum class PROTOTYPE { PROTO_GAMEOBJ, PROTO_COMPONENT };

    enum MOUSEKEYSTATE { DIM_LB, DIM_RB, DIM_MB, DIM_END };
    enum MOUSEMOVESTATE { DIMS_X, DIMS_Y, DIMS_Z, DIMS_END };
}
//-------------------------------------------------------------
//-------------------------------------------------------------

//-------------------------------------------------------------
// struct 
//-------------------------------------------------------------
struct EngineContext
{
    ID3D12Device* device;
    ID3D12GraphicsCommandList* cmdList;
    ID3D12CommandQueue* cmdQueue;

    ID3D12DescriptorHeap* srvHeap;
    ID3D12DescriptorHeap* rtvHeap;
    ID3D12DescriptorHeap* dsvHeap;
};

typedef struct
{
	HINSTANCE		hInstance;
	HWND			hWnd;
	bool			isWindowed;
	unsigned int	iNumLevels;
	unsigned int	iViewportWidth;
	unsigned int	iViewportHeight;
}ENGINE_DESC;

typedef struct tagKeyState
{
	bool bPress = false;
	bool bDown = false;
	bool bUp = false;

	tagKeyState() : bPress(false), bDown(false), bUp(false) {}
}KEYSTATE, * PKEYSTATE;


// InputLayout에서 사용할 정점 구조체
typedef struct
{
	XMFLOAT3		vPosition;		// 12bytes
	XMFLOAT3		vNormal;		// 12bytes
	XMFLOAT2		vTexcoord;		// 8bytes
	XMFLOAT3		vTangent;		// 12bytes
}VTXMESH;

//-------------------------------------------------------------
//-------------------------------------------------------------

#define NO_COPY(CLASSNAME)											\
			private:												\
			CLASSNAME(const CLASSNAME&) = delete;					\
			CLASSNAME& operator = (const CLASSNAME&) = delete;		

#define DECLARE_SINGLETON(CLASSNAME)								\
			NO_COPY(CLASSNAME)										\
			private:												\
			static CLASSNAME*	m_pInstance;						\
			public:													\
			static CLASSNAME*	GetInstance( void );				\
			static unsigned int DestroyInstance( void );			

#define IMPLEMENT_SINGLETON(CLASSNAME)								\
			CLASSNAME*	CLASSNAME::m_pInstance = nullptr;			\
			CLASSNAME*	CLASSNAME::GetInstance( void )	{			\
			if(nullptr == m_pInstance) {							\
				m_pInstance = new CLASSNAME;						\
				}													\
				return m_pInstance;									\
			}														\
			unsigned int CLASSNAME::DestroyInstance( void ) {		\
				unsigned int iRefCnt = {0};							\
				if (nullptr != m_pInstance) {						\
				iRefCnt = m_pInstance->Release();					\
				if(0 == iRefCnt)									\
					m_pInstance = nullptr;							\
				}													\
				return iRefCnt;										\
			}

#ifndef			MSG_BOX
#define			MSG_BOX(_message)			MessageBox(NULL, TEXT(_message), L"System Message", MB_OK)
#endif