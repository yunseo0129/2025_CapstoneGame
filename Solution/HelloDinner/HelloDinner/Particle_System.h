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

class CParticle_System final: public CBase
{
public:
    // C++ <-> HLSL 공유 레이아웃 (48 bytes, 16B 정렬). Shader_Particle.hlsl 의 Particle 과 동일.
    struct PARTICLE
    {
        _float3 vPos;   _float  fLife;    // 16
        _float3 vVel;   _float  fSize;    // 16
        _float4 vColor;                   // 16
    };

    static const _uint MAX_PARTICLES = 131072; // 128K (256으로 나눠떨어짐 -> 512 그룹)
    static const _uint THREAD_GROUP_SIZE = 256;     // 2단계 컴퓨트 그룹 크기

private:
    explicit CParticle_System(ID3D12Device* pDevice);
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

    // [3단계] 방출. 게임 로직(CMap::Break 등)에서 호출. 시드 입자를 스테이징에 쌓고
    //         다음 Compute 에서 풀의 링 영역으로 복사된다(GPU 명령은 Compute가 기록).
    void    Emit(const _float3& vCenter, const _float3& vExtents, _uint iCount);

    // 테스트 입자 배치 방식: true = 첫 프레임에 카메라 정면에 구름 생성(어떤 씬에서도 보이게).
    void    Set_TestSpawnInFront(_bool b) { m_bSpawnInFront = b; }
    void    Set_TestCount(_uint n) { m_iTestCount = (n > MAX_PARTICLES) ? MAX_PARTICLES : n; }

private:
    HRESULT Create_ParticleBuffer();   // DEFAULT 구조화버퍼 + UPLOAD 스테이징
    HRESULT Create_ParticleSRV();      // 글로벌 힙에 SRV (인덱스 기록)
    HRESULT Create_RootSignature();    // 전용 그래픽 RS (b0 CBV + t0 SRV table)
    HRESULT Create_PSO();              // 전용 알파블렌드 PSO (입력레이아웃 없음)
    HRESULT Create_FrameCB();          // per-frame 카메라/상수 CB (UPLOAD x FRAME_COUNT)

    HRESULT Create_ComputeRootSignature(); // 2단계: 컴퓨트 RS (b1 32비트상수 + u0 루트 UAV)
    HRESULT Create_ComputePSO();           // 2단계: 컴퓨트 PSO (CS_Update)

    HRESULT Create_EmitStaging();          // 3단계: 방출 시드용 UPLOAD 스테이징
    _bool   Is_CameraReady();              // 카메라 뷰가 유효(비특이행렬)한가 -> 빌보드 축 NaN 방지

    ID3DBlob* Compile_Shader(const wstring& strPath, const char* strEntry, const char* strTarget);

    _bool   Fill_TestParticles_CPU();  // 스테이징 채움. 카메라 유효 시 true / 아직 미준비(특이행렬)면 false
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

    ID3D12Device* m_pDevice = {nullptr};
    CGameInstance* m_pGameInstance = {nullptr}; // raw 참조(AddRef 안 함: GameInstance가 본 객체를 소유)

    // 입자 풀(영속 단일 버퍼) + 초기 업로드 스테이징
    ComPtr<ID3D12Resource> m_pParticleBuffer;            // DEFAULT, ALLOW_UNORDERED_ACCESS
    ComPtr<ID3D12Resource> m_pUploadBuffer;              // UPLOAD 스테이징(테스트 입자)
    PARTICLE* m_pUploadMapped = {nullptr};

    _uint m_iSrvIndex = {0};  // 글로벌 SRV 힙에서의 인덱스
    // 2단계: _uint m_iUavIndex = { 0 };

    ComPtr<ID3D12RootSignature> m_pRootSig; // 전용 그래픽 RS
    ComPtr<ID3D12PipelineState> m_pPSO;     // 전용 알파블렌드 PSO

    ComPtr<ID3D12RootSignature> m_pComputeRootSig; // 2단계: 컴퓨트 RS
    ComPtr<ID3D12PipelineState> m_pComputePSO;     // 2단계: 컴퓨트 PSO

    ComPtr<ID3D12Resource> m_pFrameCB[FRAME_COUNT];
    FRAME_CB* m_pFrameCBMapped[FRAME_COUNT] = {};
    _uint                 m_iFrameCBStride = {0}; // 256 정렬 크기

    // 3단계: 방출(emission)
    ComPtr<ID3D12Resource> m_pEmitStaging;                 // UPLOAD 스테이징(방출 시드)
    PARTICLE* m_pEmitStagingMapped = {nullptr};
    _uint                 m_iEmitCursor = {0};         // 풀 링 커서(프레임 넘어 유지)
    _uint                 m_iStagingCursor = {0};        // 이번 프레임 스테이징 커서(매 Compute 리셋)
    vector<EMIT_COPY>     m_PendingEmits;                  // 이번 프레임 방출 복사 목록

    _uint   m_iAliveCount = {0};     // 1단계: 테스트 입자 수
    _uint   m_iTestCount = {4096};  // 초기 테스트 입자 개수
    _float  m_fDeltaTime = {0.f};
    _float  m_fGravityY = {-5.f};  // 2단계: 중력(케첩이 아래로 떨어지는 느낌). 게임 스케일에 맞춰 조절
    _bool   m_bUploaded = {false}; // 1회 업로드 완료 여부
    _bool   m_bSpawnInFront = {true};  // 카메라 정면 구름 배치

public:
    static CParticle_System* Create(ID3D12Device* pDevice);
    virtual void Free() override;
};