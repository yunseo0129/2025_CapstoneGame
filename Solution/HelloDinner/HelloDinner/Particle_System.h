#pragma once
#include "Base.h"

// ============================================================================
//  CParticle_System
//  GPU 컴퓨트 파티클 시스템 (포트폴리오용). 기존 그래픽/인스턴싱/파괴 경로 무수정.
//
//  [1단계] 컴퓨트 없이 "GPU 버퍼 = 인스턴스 소스" 렌더 경로 확립.
//    - DEFAULT 힙 구조화버퍼(입자 풀, ALLOW_UNORDERED_ACCESS) 생성. 2단계 대비 UAV 플래그 미리 켬.
//    - 첫 프레임에 CPU 테스트 입자 N개를 UPLOAD 스테이징 -> CopyBufferRegion 으로 1회 주입.
//    - 그 버퍼의 SRV를 글로벌 셰이더 가시 힙(Texture_Manager)에 생성.
//    - 전용 그래픽 루트시그(b0 카메라 CBV + t0 StructuredBuffer SRV) + 전용 알파블렌드 PSO.
//    - 빌보드 VS(SV_VertexID 쿼드 합성 + SV_InstanceID로 입자 읽기) / 원형 페이드 PS.
//
//  [2단계 예정] 같은 버퍼에 UAV 생성 + 컴퓨트 RS/PSO + 업데이트 CS 디스패치 + 상태 전환 배리어.
//  [3단계 예정] CMap::Break() 에서 시드 입자 주입(CPU 시드 업로드 -> CopyBufferRegion).
//  [4단계 예정] ExecuteIndirect 로 alive 수만 드로우 + 압축/정렬.
// ============================================================================

class CGameInstance;
class CTexture;          // [7b] 파티클 외형 텍스처(배열 DDS) 로드용
struct EngineContext;    // [7b] device + 로딩 cmdList (Defines.h)

class CParticle_System final: public CBase
{
public:
    // C++ <-> HLSL 공유 레이아웃 (64 bytes, 16B 정렬). Shader_Particle.hlsl 의 Particle 과 정확히 동일.
    // [6단계] iType/fMaxLife/fSpawnSize 추가(48->64). fSize/vColor 는 CS가 수명 곡선으로 매 프레임 갱신.
    struct PARTICLE
    {
        _float3 vPos;   _float  fLife;       // 16
        _float3 vVel;   _float  fSize;       // 16  현재(렌더) 크기 - CS가 곡선으로 갱신
        _float4 vColor;                      // 16  현재(렌더) 색  - CS가 곡선으로 갱신
        _uint   iType;  _float  fMaxLife;    // 8   종류 / 방출 시점 수명(age = 1 - fLife/fMaxLife)
        _float  fSpawnSize; _uint  uPacked;  // 8   방출 시 기준 크기(불변, 곡선이 곱) / [7b] 외형 패킹
    };  // 합계 64 (offset 불변: iType@48, fMaxLife@52, fSpawnSize@56, uPacked@60)
    // [7b] uPacked 비트 레이아웃 (외형 다양성 - 거동 iType 과 별개로 VS/PS 에서만 사용):
    //   bit 0-7  : uSprite     (텍스처 배열 슬라이스 0-255)
    //   bit 8-15 : angleQ      (빌보드 회전 양자화. angle = angleQ/256 * 2pi)
    //   bit 16   : flipU
    //   bit 17   : flipV
    //   bit 18   : bProcedural (1=흙먼지 등 텍스처 없이 GPU 절차적 드로우. 이때 uSprite 무시)

    static const _uint MAX_PARTICLES = 131072; // 128K (256으로 나눠떨어짐 -> 512 그룹)
    static const _uint THREAD_GROUP_SIZE = 256;     // 2단계 컴퓨트 그룹 크기

    // [5단계] 입자 종류(방출 프리셋). 새 종류는 여기 + .cpp의 프리셋 테이블에만 추가하면 된다.
    enum PARTICLE_TYPE { KETCHUP_SPRAY, WALL_DEBRIS, WALL_DEBRIS_2, TYPE_END };

    // [7b] 외형 텍스처 배열(단일 DDS)의 총 슬라이스 수. CPU 외형표(.cpp g_SpriteRanges)의
    //  (base+count) 최댓값 및 DDS DepthOrArraySize 와 반드시 일치해야 한다.
    //  배분: 케첩 슬라이스 0~1(2) / 파편 슬라이스 2~5(4) / 흙먼지=텍스처 미사용(GPU 절차적).
    static const _uint PARTICLE_TEX_SLICES = 6;

    // 방출 요청. 종류 + 위치(+ 분사 방향/스폰 영역/개수). 색·속도패턴·크기·수명은 프리셋이 결정.
    struct EMIT_DESC
    {
        PARTICLE_TYPE eType = WALL_DEBRIS;
        _float3       vCenter = {};                  // 방출 중심(월드)
        _float3       vDirection = {0.f, 1.f, 0.f};   // 분사 방향(KETCHUP_SPRAY 등). 폭발형은 무시
        _float3       vExtents = {0.3f, 0.3f, 0.3f};// 스폰 영역 반경(WALL_DEBRIS). 분사형은 작게(점)
        _uint         iCount = 0;                   // 0 -> 프리셋 기본 개수
    };

    static const _uint PARTICLE_TYPE_COUNT = TYPE_END; // Shader_Particle.hlsl 의 PARTICLE_TYPE_COUNT 와 동일해야 함

    // [6단계] 종류별 거동(물리/수명 곡선) 테이블. Shader_Particle.hlsl 의 TypeBehavior 와 정확히 동일(48 bytes).
    //  방출 모양(.cpp의 g_Presets)과 거동(.cpp의 g_Behaviors)의 단일 출처 = PARTICLE_TYPE enum.
    struct TYPE_BEHAVIOR
    {
        _float  fGravityScale;            // 중력 배율 (돌=큼, 케첩=작음)
        _float  fDrag;                    // 속도 감쇠(공기저항) 계수 [1/s]
        _float  fStartSize, fEndSize;     // fSpawnSize 에 곱할 크기 곡선 배율(age 0->1)
        _float4 vStartColor, vEndColor;   // vColor 보간색(alpha 로 페이드)
    };  // 16 + 16 + 16 = 48

private:
    explicit CParticle_System(EngineContext* pContext);
    virtual ~CParticle_System() = default;

public:
    HRESULT Initialize();

    // Update_Engine(CPU) 에서 호출 - dt 저장(2단계 컴퓨트용). GPU 명령 없음.
    void    Set_TimeDelta(_float fDeltaTime) { m_fDeltaTime = fDeltaTime; }

    // Draw() 시작부에서 호출 - 1단계: 1회 테스트 입자 업로드 + 배리어. 2단계: 컴퓨트 디스패치.
    void    Compute(ID3D12GraphicsCommandList* pCmd);

    // Draw() 의 블렌드(반투명) 이후에 호출 - 카메라 CB 갱신 + 빌보드 인스턴스 드로우.
    void    Render(ID3D12GraphicsCommandList* pCmd);

    _uint   Get_AliveCount() const { return m_iAliveCount; }

    // [3단계+] 방출. 게임 로직(CMap::Break, 케첩건 발사 등)에서 호출. 시드 입자를 스테이징에 쌓고
    //          다음 Compute 에서 풀의 링 영역으로 복사된다(GPU 명령은 Compute가 기록).
    void    Emit(const EMIT_DESC& desc);

    // [7b] 파티클 외형 텍스처(단일 배열 DDS) 로드. CTexture(TEX_ARRAY)가 내부에서 EngineContext->cmdList 로
    //  업로드를 기록하므로, 엔진 로딩 cmdList 구간(MainApp: ResetCmdList~CloseCmdList) 안에서 호출해야
    //  업로드가 로딩 펜스(WaitForGpuComplete)로 완료 보장된다. (Loader::Loading_Level_GamePlay 에서 호출)
    HRESULT Load_ParticleTexture();

private:
    HRESULT Create_ParticleBuffer();   // DEFAULT 힙 구조화버퍼(입자 풀)
    HRESULT Create_ParticleSRV();      // 글로벌 힙에 SRV (인덱스 기록)
    HRESULT Create_RootSignature();    // 전용 그래픽 RS (b0 CBV + t0 SRV table)
    HRESULT Create_PSO();              // 전용 알파블렌드 PSO (입력레이아웃 없음)
    HRESULT Create_FrameCB();          // per-frame 카메라/상수 CB (UPLOAD x FRAME_COUNT)

    HRESULT Create_ComputeRootSignature(); // 2단계: 컴퓨트 RS (b1 32비트상수 + u0 루트 UAV)
    HRESULT Create_ComputePSO();           // 2단계: 컴퓨트 PSO (CS_Update)
    HRESULT Create_TypeCB();               // 6단계: 종류별 거동 상수버퍼(b2, UPLOAD/영속매핑) + 1회 채움

    HRESULT Create_EmitStaging();          // 3단계: 방출 시드용 UPLOAD 스테이징
    _bool   Is_CameraReady();              // 카메라 뷰가 유효(비특이행렬)한가 -> 빌보드 축 NaN 방지

    HRESULT Create_AliveList();            // 4단계: alive 인덱스 목록 버퍼(UAV/SRV 루트 디스크립터)
    HRESULT Create_IndirectArgs();         // 4단계: 간접 인자 버퍼 + 리셋 업로드 + 커맨드 시그니처

    ID3DBlob* Compile_Shader(const wstring& strPath, const char* strEntry, const char* strTarget);

    void    Update_FrameCB(_uint iFrame); // 현재 카메라 -> 매핑된 CB

private:
    static const _int FRAME_COUNT = 3; // = CGraphic_Device::SWAP_CHAIN_BUFFER_COUNT

    // per-frame 상수 (Shader_Particle.hlsl 의 ParticleFrameCB 와 동일 레이아웃, 160B -> 256 정렬)
    struct FRAME_CB
    {
        _float4x4 matView;   // 64
        _float4x4 matProj;   // 64
        _float3   vCamRight;  _float fDeltaTime;    // 16
        _float3   vCamUp;     _uint  iMaxParticles; // 16
    };

    // 2단계 컴퓨트 루트 32비트 상수 (HLSL ParticleComputeCB(b1) 와 동일 레이아웃, 5 x 4B)
    struct CS_CONSTANTS
    {
        _float fDeltaTime;     // b1 offset 0
        _uint  iMaxParticles;  //         4
        _float fGravityX;      //         8
        _float fGravityY;      //         12
        _float fGravityZ;      //         16
    };

    // 3단계: 이번 프레임 방출 복사 1건 (스테이징 -> 풀)
    struct EMIT_COPY
    {
        _uint iPoolStart;     // 풀 버퍼 시작 인덱스(링)
        _uint iStagingStart;  // 스테이징 시작 인덱스
        _uint iCount;         // 입자 수
    };

    static const _uint EMIT_STAGING_MAX = 8192; // 한 프레임 방출 가능한 최대 입자 수

    EngineContext* m_pContext = {nullptr};      // [7b] device + 로딩 cmdList (파티클 텍스처 로딩에 사용)
    ID3D12Device* m_pDevice = {nullptr};
    CGameInstance* m_pGameInstance = {nullptr}; // raw 참조(AddRef 안 함: GameInstance가 본 객체를 소유)

    // 입자 풀(영속 단일 버퍼)
    ComPtr<ID3D12Resource> m_pParticleBuffer;            // DEFAULT, ALLOW_UNORDERED_ACCESS

    _uint m_iSrvIndex = {0};  // 글로벌 SRV 힙에서의 인덱스
    // 2단계: _uint m_iUavIndex = { 0 };

    ComPtr<ID3D12RootSignature> m_pRootSig; // 전용 그래픽 RS
    ComPtr<ID3D12PipelineState> m_pPSO;     // 전용 알파블렌드 PSO

    ComPtr<ID3D12RootSignature> m_pComputeRootSig; // 2단계: 컴퓨트 RS
    ComPtr<ID3D12PipelineState> m_pComputePSO;     // 2단계: 컴퓨트 PSO

    // 6단계: 종류별 거동 테이블 상수버퍼 (UPLOAD, 영속 매핑, b2 루트 CBV)
    ComPtr<ID3D12Resource> m_pTypeCB;
    TYPE_BEHAVIOR* m_pTypeCBMapped = {nullptr};

    // [7b] 파티클 외형 텍스처(단일 배열 DDS). CTexture(TEX_ARRAY)가 리소스/SRV(글로벌 힙)를 소유한다.
    //  Create 가 돌려준 +1 참조를 그대로 보유하고 Free 에서 Safe_Release. 모양은 텍스처 알파, 색/페이드는 vColor(6단계 곡선).
    CTexture* m_pParticleTex = {nullptr};

    ComPtr<ID3D12Resource> m_pFrameCB[FRAME_COUNT];
    FRAME_CB* m_pFrameCBMapped[FRAME_COUNT] = {};
    _uint                 m_iFrameCBStride = {0}; // 256 정렬 크기

    // 3단계: 방출(emission)
    ComPtr<ID3D12Resource> m_pEmitStaging;                 // UPLOAD 스테이징(방출 시드)
    PARTICLE* m_pEmitStagingMapped = {nullptr};
    _uint                 m_iEmitCursor = {0};         // 풀 링 커서(프레임 넘어 유지)
    _uint                 m_iStagingCursor = {0};        // 이번 프레임 스테이징 커서(매 Compute 리셋)
    vector<EMIT_COPY>     m_PendingEmits;                  // 이번 프레임 방출 복사 목록

    // 4단계: GPU 컬링 + ExecuteIndirect
    ComPtr<ID3D12Resource>        m_pAliveListBuffer;      // DEFAULT, ALLOW_UAV (uint x MAX) - alive 인덱스
    ComPtr<ID3D12Resource>        m_pArgsBuffer;           // DEFAULT, ALLOW_UAV - D3D12_DRAW_ARGUMENTS(간접 인자 + 카운터)
    ComPtr<ID3D12Resource>        m_pArgsResetBuffer;      // UPLOAD - {6,0,0,0} 매 프레임 리셋용
    void* m_pArgsResetMapped = {nullptr};
    ComPtr<ID3D12CommandSignature> m_pCommandSignature;    // DrawInstanced 간접 시그니처
    _bool                         m_bArgsValid = {false}; // 첫 컴퓨트 패스 완료(ExecuteIndirect 안전) 여부

    _uint   m_iAliveCount = {0};     // 현재 살아있는 입자 수
    _float  m_fDeltaTime = {0.f};
    _float  m_fGravityY = {-5.f};  // 2단계: 중력(케첩이 아래로 떨어지는 느낌). 게임 스케일에 맞춰 조절
    _bool   m_bUploaded = {false}; // 첫 프레임 준비(카메라 유효) 완료 여부

public:
    static CParticle_System* Create(EngineContext* pContext);
    virtual void Free() override;
};