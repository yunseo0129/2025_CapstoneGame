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

struct KEYFRAME
{
	XMFLOAT3		vScale;
	XMFLOAT4		vRotation;
	XMFLOAT3		vPosition;
	float			fKeyFramePosition;
};

// 수정 필요
enum LEVELID { LEVEL_STATIC, LEVEL_LOADING, LEVEL_LOGO, LEVEL_GAMEPLAY, LEVEL_MAPLOADING ,LEVEL_END };

// Texture data 저장하는 srv 만들 때 VIEW_DIMENSION 설정용
enum TEXTURE_TYPE {
	TEX_2D ,
	TEX_CUBE ,
	TEX_ARRAY ,
};

// Camera
enum CAMERA_TYPE
{
	CAMERA_FPV ,
	CAMERA_TPV ,
	CAMERA_UI  ,
	CAMERA_END
};

// Root Signature의 Root Parameter 슬롯 설정용
enum RootParameterIndex
{
	Camera,
	GameObject,
	TEXTURE_Diffuse,
	TEXTURE_Normal,
	BoneMatrix,
	Light,
	ShadowMap,
	// 후에 Material, BoneMatrix 등등 추가할 수 있음
	End
};

// Input Layout과 PSO 설정용
enum PSO_TYPE
{
	DEFAULT ,        // 일반 물체 (Static Mesh / Opaque)
	SKYBOX ,         // 스카이박스 (TextureCube / DepthFunc LessEqual)
	ANIM ,           // 캐릭터 (Skeletal Mesh / Opaque)
	ALPHA_BLEND ,    // 반투명 이펙트 (Static Mesh / Transparent)
	UI ,             // 2D UI (UI Mesh / Transparent / No Depth)
	SHADOW_STATIC ,  // 그림자 생성용 (Static Mesh / Depth Only)
	SHADOW_ANIM ,    // 그림자 생성용 (Skeletal Mesh / Depth Only)
    DEFAULT_INSTANCED, // 인스턴싱용
    SHADOW_STATIC_INSTANCED,
	END
};

//Texture Type
enum TextureType
{
	TextureType_NONE = 0,
	TextureType_DIFFUSE = 1,
	TextureType_SPECULAR = 2,
	TextureType_AMBIENT = 3,
	TextureType_EMISSIVE = 4,
	TextureType_HEIGHT = 5,
	TextureType_NORMALS = 6,
	TextureType_SHININESS = 7,
	TextureType_OPACITY = 8,
	TextureType_DISPLACEMENT = 9,
	TextureType_LIGHTMAP = 10,
	TextureType_REFLECTION = 11,
	TextureType_BASE_COLOR = 12,
	TextureType_NORMAL_CAMERA = 13,
	TextureType_EMISSION_COLOR = 14,
	TextureType_METALNESS = 15,
	TextureType_DIFFUSE_ROUGHNESS = 16,
	TextureType_AMBIENT_OCCLUSION = 17,
	TextureType_UNKNOWN = 18,

#ifndef SWIG
	TextureType_Force32Bit = INT_MAX
#endif
};
#define AI_TEXTURE_TYPE_MAX  TextureType_UNKNOWN


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


// CB

// Camera에서 사용할 카메라 정보 구조체
typedef struct
{
	XMFLOAT4X4						m_xmf4x4View;
	XMFLOAT4X4						m_xmf4x4Proj;
	XMFLOAT3						m_xmf3Position;
	float							m_fPadding; // 16바이트 정렬을 위한 패딩
}CB_VS_CAMERA;

// BoneMatrix에서 사용할 본 매트릭스 상수 버퍼 구조체
typedef struct
{
	XMFLOAT4X4						m_BoneMatrices[512];
}CB_BONE_MATRICES;

// Light
typedef struct
{
	XMFLOAT4 vDirection;
	XMFLOAT4 vPosition;
	XMFLOAT4 vDiffuse;
	XMFLOAT4 vAmbient;
	XMFLOAT4 vSpecular;
	float    fRange;
	XMFLOAT3 vPadding;

	XMFLOAT4X4 matLightTransform;
}CB_LIGHT;

// InputLayout에서 사용할 정점 구조체
typedef struct
{
	XMFLOAT3		vPosition;		// 12bytes
	XMFLOAT3		vNormal;		// 12bytes
	XMFLOAT2		vTexcoord;		// 8bytes
	XMFLOAT3		vTangent;		// 12bytes
}VTXMESH;

typedef struct
{
	XMFLOAT3		vPosition;		// 12bytes
	XMFLOAT3		vNormal;		// 12bytes
	XMFLOAT2		vTexcoord;		// 8bytes
	XMFLOAT3		vTangent;		// 12bytes
	XMUINT4			vBlendIndices;	// 16bytes
	XMFLOAT4		vBlendWeights;	// 16bytes
}VTXANIMMESH;



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