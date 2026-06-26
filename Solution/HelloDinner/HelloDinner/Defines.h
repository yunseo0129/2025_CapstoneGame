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

//-------------------------------------------------------------
// 대기방(Room)에서 고른 팀/번호를 게임플레이로 넘기기 위한 전역 캐리어.
//  - Room 은 엔진이 켜지기 전(별도 Win32 창)에 도므로, 싱글톤 대신
//    이 작은 전역 구조체에 선택값을 담아 둔다.
//  - CGame_Manager::Start_Match() 가 이 값을 읽어 본인 슬롯을 배치한다.
//  - iTeam : 0 = RED, 1 = BLUE  /  iNumber : 1~3 (팀 내 번호)
//  - 선택 지점 월드 좌표:  RED = (100, 100, 0),  BLUE = (-100, 100, 0)
//  - 두 팀 모두 원점(0,0,0)을 바라보고 선다.
//-------------------------------------------------------------
struct MATCH_SETUP
{
    int iTeam = 0;     // 0: RED, 1: BLUE
    int iNumber = 1;   // 1, 2, 3

    // 선택 지점 월드 좌표를 계산해 돌려준다(팀 → 위치 규칙을 한 곳에 둠).
    //   RED  : x = 100,  y = 100, z = 0
    //   BLUE : x = -100, y = 100, z = 0
    XMFLOAT3 Get_SpawnSpot() const
    {
        return XMFLOAT3(5.f * iNumber - 5.f,
            25.f, iTeam == 0 ? 50.f : -60.f
            );
    }

    // 스폰 지점에서 원점(0,0,0)을 바라보는 Y축 회전(yaw, 라디안)을 돌려준다.
    //  - 기본 룩이 +Z 라는 전제(Game_Manager 에서 XM_PI 로 카메라 보도록 뒤집는 것과 동일 규칙).
    //  - 수평 방향(dx, dz)을 바라보는 yaw = atan2(dx, dz).
    float Get_SpawnYaw() const
    {
        const XMFLOAT3 vSpot = Get_SpawnSpot();
        const float dx = 0.f - vSpot.x;   // 원점 - 스폰
        const float dz = 0.f - vSpot.z;
        return atan2f(dx, dz);
    }

    // ── 싱크대 위 스테이징 좌표 (CHARSELECT / SCOREBOARD 단계 배치용) ──────────
    // Washbasin OBB 윗면 월드 (MapData.json에서 확인):
    //   중심 XZ = (-53.04, 9.85),  윗면 Y = 33.2
    //   반치수 x=11.9  z=20.0  (footprint x∈[-65,-41] z∈[-10,30])
    // 팀A(0) → z = +24.85 (싱크 +z 끝), 팀B(1) → z = -5.15 (싱크 -z 끝)
    // 슬롯 번호(1~3) → x = SINK_X + (number-2)*8   (8 단위 1열 정렬)
    // 서버 RoomPhaseManager::ApplyTeamSpawnPositions 와 수치 완전 일치 유지 필요.
    XMFLOAT3 Get_SinkSpot() const
    {
        constexpr float SINK_X         = -53.04f;
        constexpr float SINK_TOP_Y     =  33.2f;
        constexpr float SINK_Z         =   9.85f;
        constexpr float SLOT_SPACING_X =   8.0f;
        constexpr float TEAM_Z_OFFSET  =  15.0f;

        const float x = SINK_X + (iNumber - 2) * SLOT_SPACING_X;
        const float y = SINK_TOP_Y;
        const float z = SINK_Z + (iTeam == 0 ? +TEAM_Z_OFFSET : -TEAM_Z_OFFSET);
        return XMFLOAT3(x, y, z);
    }

    // 싱크대 위에서 바라볼 방향: 팀A → 상대(−z) 방향, 팀B → 상대(+z) 방향
    float Get_SinkYaw() const
    {
        // 팀A: dz=-1(-z를 바라봄), 팀B: dz=+1(+z를 바라봄)
        const float dz = (iTeam == 0) ? -1.f : +1.f;
        return atan2f(0.f, dz);
    }
};
extern MATCH_SETUP g_MatchSetup;

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
enum LEVELID { LEVEL_STATIC, LEVEL_LOADING, LEVEL_LOGO, LEVEL_GAMEPLAY, LEVEL_MAPLOADING, LEVEL_END };

// Texture data 저장하는 srv 만들 때 VIEW_DIMENSION 설정용
enum TEXTURE_TYPE {
    TEX_2D,
    TEX_CUBE,
    TEX_ARRAY,
};

// Camera
enum CAMERA_TYPE
{
    CAMERA_FPV,
    CAMERA_TPV,
    CAMERA_UI,
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
    UIColor,
    MapUV,      // [방식 가] 팔레트 crop UV 재매핑 (b5: float2 offset + float2 scale)
    // 후에 Material, BoneMatrix 등등 추가할 수 있음
    End
};

// Input Layout과 PSO 설정용
enum PSO_TYPE
{
    DEFAULT,        // 일반 물체 (Static Mesh / Opaque)
    SKYBOX,         // 스카이박스 (TextureCube / DepthFunc LessEqual)
    ANIM,           // 캐릭터 (Skeletal Mesh / Opaque)
    ALPHA_BLEND,    // 반투명 이펙트 (Static Mesh / Transparent)
    UI,             // 2D UI (UI Mesh / Transparent / No Depth)
    SHADOW_STATIC,  // 그림자 생성용 (Static Mesh / Depth Only)
    SHADOW_ANIM,    // 그림자 생성용 (Skeletal Mesh / Depth Only)
    DEFAULT_INSTANCED, // 인스턴싱용
    SHADOW_STATIC_INSTANCED,
    MAPRT_INSTANCED,   // [맵 RTT] 탑다운 맵 렌더용 (DEFAULT_INSTANCED + CullMode None)
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

    tagKeyState(): bPress(false), bDown(false), bUp(false) {}
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