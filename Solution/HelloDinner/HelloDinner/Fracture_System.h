#pragma once
#include "Base.h"

// =============================================================================
//  CFracture_System  -  파괴 벽(cell fracture) GPU 강체 물리 + 전담 렌더 (풀 버전)
//   - 조각당 단일 본(rigid skin, VTXANIMMESH) 메시를 받아 각 조각(=본)을 강체로 날린다.
//     물리=compute(Shader_Fracture_Compute.hlsl), 렌더=전용 VS/PS(Shader_Fracture.hlsl).
//   - 글로벌 RS / 본 CB 경로 무수정. 파티클처럼 독립 모듈.
//
//   [전담 렌더] CMap 은 fracture 벽을 그리지 않는다. 이 시스템이 직접 그린다.
//     - Register(model, world): 슬롯 할당 + chunk 를 bind 상태(invMass=0)로 초기화.
//         등록된 동안 매 프레임 Compute(bind면 본 매트릭스 identity) + Render → "온전한 벽".
//     - Break(model, breakPoint): 해당 슬롯 chunk 활성화(invMass>0) + 임펄스 → 부서짐.
//
//   행렬: 본(조각) 매트릭스 = T(-centerBind)·R(quat)·T(pos)  (무게중심 기준 강체)
//         centerBind = inverse(OffsetMatrix).translation  (로드 시 모델에서 추출)
//         bind(quat=identity, pos=centerBind) -> identity -> 정점 제자리(g_matWorld만 적용)
// =============================================================================

class CGameInstance;
class CModel;
class CTexture;

class CFracture_System final: public CBase
{
public:
    // C++/HLSL 공유 (80 bytes). Shader_Fracture_Compute.hlsl 의 Chunk 와 정확히 일치.
    struct CHUNK
    {
        _float3 vPos;        _float fInvMass;     // invMass==0 -> 비활성(bind, 적분 스킵)
        _float4 qRot;                             // 쿼터니언 (x,y,z,w)
        _float3 vCenterBind; _float fRestitution; // bind 무게중심(회전축, 불변) / 바닥 반발
        _float3 vLinVel;     _float fDrag;        // 선속도 / 공기저항[1/s]
        _float3 vAngVel;     _float fRadius;      // 각속도 / 바닥 충돌 반경
    };

    // [튜닝 상수] 조각 50개 + 20벽 동시 폭발 염두. 숫자만 바꿔 재빌드하면 확장됨.
    static const _uint MAX_CHUNKS = 64;   // 벽 1개의 최대 조각 수(50 + 여유)
    static const _uint MAX_FRACTURE_WALLS = 24;   // 동시 활성 벽 수(20 + 여유)
    static const _uint THREAD_GROUP_SIZE = 64;
    static const _uint MAX_COLLIDERS_PER_WALL = 16;  // [A] 벽당 충돌 검사할 근처 정적 콜라이더 상한

    // [A] 모델 공간 OBB 콜라이더 (C++/HLSL 공유, 64 bytes). Shader_Fracture_Compute.hlsl 의 Collider 와 일치.
    struct GPU_COLLIDER
    {
        _float3 vCenter;  _float fExtX;   // 16  중심 / half-extent X
        _float3 vAxisX;   _float fExtY;   // 16  정규화 축X / half-extent Y
        _float3 vAxisY;   _float fExtZ;   // 16  정규화 축Y / half-extent Z
        _float3 vAxisZ;   _float fPad;    // 16  정규화 축Z
    };

    // 파괴 튜닝(0이면 기본값). Break 시 전달.
    struct BREAK_PARAM
    {
        _float3 vBreakPoint = {};   // 파괴 지점(월드). 0 이면 무게중심 중심 폭발
        _uint   iSeed = 0;    // 결정론 난수 시드(멀티: 서버가 내려준 값 → 전 클라 동일 파괴)
        _float  fImpulse = 0.f;  // 폭발 세기
        _float  fUpBias = 0.f;  // 위로 솟구침
        _float  fSpin = 0.f;  // 각속도 세기
    };

private:
    explicit CFracture_System(EngineContext* pContext);
    virtual ~CFracture_System() = default;

public:
    HRESULT Initialize();

    void    Set_TimeDelta(_float dt) { m_fDeltaTime = dt; }
    void    Set_FloorY(_float y) { m_fFloorY = y; }

    // 로드/스폰 시: 벽을 wallId 와 함께 bind 상태로 등록 → 온전한 벽으로 렌더. 실패 시 -1.
    //  iWallId = 모든 클라에서 동일한 안정적 식별자(MapData 인스턴스 순번).
    _int    Register(CModel* pModel, _uint iWallId, const _float4x4& matWorld, _uint iMeshIndex = 0, class CCollider* pSelfCollider = nullptr);
    // [멀티 진입점] 안정적 ID 로 파괴. 서버 이벤트(wallId, breakPoint, seed) 수신 시 호출.
    void    Break_ByWallId(_uint iWallId, const BREAK_PARAM& param = {});
    // 모델 포인터로 파괴(싱글/로컬 편의). 내부적으로 슬롯을 찾아 처리.
    void    Break(CModel* pModel, const BREAK_PARAM& param = {});
    // 슬롯 인덱스를 직접 들고 있는 경우
    void    Break_BySlot(_int iSlot, const BREAK_PARAM& param = {});
    // 등록 해제(벽 제거 시). 슬롯 반환.
    void    Unregister(CModel* pModel);

    void    Compute(ID3D12GraphicsCommandList* pCmd);   // 활성 슬롯 전체: 물리 적분 + 매트릭스 합성
    void    Render(ID3D12GraphicsCommandList* pCmd);    // 활성 슬롯 전체: 조각 메시 드로우

private:
    static const _int FRAME_COUNT = 3; // = SWAP_CHAIN_BUFFER_COUNT

    // 벽 1개분 자원 + 상태
    struct INSTANCE
    {
        ComPtr<ID3D12Resource> pChunkBuffer;            // DEFAULT, UAV  (CHUNK[MAX_CHUNKS])
        ComPtr<ID3D12Resource> pChunkUpload;            // UPLOAD        (시드)
        CHUNK* pUploadMapped = nullptr;
        ComPtr<ID3D12Resource> pMatrixBuffer;           // DEFAULT, UAV->SRV (float4x4[MAX_CHUNKS])
        _uint                  iMatrixSrvIndex = 0;      // 글로벌 힙 SRV 인덱스

        CModel* pModel = nullptr;         // 참조(소유는 CMap)
        _uint                  iWallId = 0;              // 안정적 식별자(MapData 인스턴스 순번)
        _float4x4              matWorld = {};
        _uint                  iMeshIndex = 0;
        _uint                  iChunkCount = 0;

        _bool                  bUsed = false;            // 슬롯 점유(Register~Unregister)
        _bool                  bBroken = false;          // 파괴 시작됨(물리 활성)
        _bool                  bSeedDirty = false;       // 시드 업로드 필요(Register/Break 직후)
        _float                 fLife = 0.f;              // 파괴 후 경과(소멸 타이머)
        _float                 fModelRadius = 1.f;      // 모델 공간 반경(임펄스/중력 스케일 기준 → 한 프레임 소멸 방지)

        // [A] 근처 정적 콜라이더(모델 공간 OBB). Break 시 1회 채움. 컴퓨트가 루트 SRV(t0)로 읽음.
        ComPtr<ID3D12Resource> pColliderBuffer;          // UPLOAD, StructuredBuffer<Collider>
        GPU_COLLIDER* pColliderMapped = nullptr;
        _uint                  iColliderCount = 0;
        class CCollider* pSelfCollider = nullptr;  // 자기 벽 콜라이더(충돌 제외)
    };
    INSTANCE m_Instances[MAX_FRACTURE_WALLS];

    // 공용 파이프라인
    ComPtr<ID3D12RootSignature> m_pComputeRS;
    ComPtr<ID3D12PipelineState> m_pComputePSO;
    ComPtr<ID3D12RootSignature> m_pGraphicRS;
    ComPtr<ID3D12PipelineState> m_pGraphicPSO;

    // 렌더 per-frame CB (Shader_Fracture.hlsl 의 FractureFrameCB). 슬롯마다 world 가 달라
    //  프레임 내 여러 인스턴스를 그리므로 [FRAME_COUNT] x [MAX_FRACTURE_WALLS] 링 버퍼.
    struct FRAME_CB
    {
        _float4x4 matWorld;   // 64
        _float4x4 matView;    // 64
        _float4x4 matProj;    // 64
        _float3   vLightDir;  _float fAmbient;  // 16
    };
    ComPtr<ID3D12Resource> m_pFrameCB[FRAME_COUNT];
    _byte* m_pFrameCBMapped[FRAME_COUNT] = {};  // 링 베이스(바이트 단위 오프셋)
    _uint                  m_iFrameCBStride = 0;                // 256 정렬 1개 슬롯 크기

    // compute 루트 상수 (Shader_Fracture_Compute.hlsl 의 FractureComputeCB)
    struct COMPUTE_CB
    {
        _float  fDeltaTime; _uint iChunkCount; _float fFloorY; _float fFriction;
        _float3 vGravity;   _uint  iColliderCount;   // [A] 활성 콜라이더 수(였던 fPad)
    };

    EngineContext* m_pContext = {nullptr};
    ID3D12Device* m_pDevice = {nullptr};
    CGameInstance* m_pGameInstance = {nullptr};

    // 생성 헬퍼
    HRESULT Create_InstanceBuffers(INSTANCE& inst);   // 슬롯 1개분 버퍼 + SRV
    HRESULT Create_ComputeRS_PSO();
    HRESULT Create_GraphicRS_PSO();
    HRESULT Create_FrameCB();
    ID3DBlob* Compile_Shader(const wstring& strPath, const char* strEntry, const char* strTarget);

    _int    Find_FreeSlot();
    _int    Find_SlotByModel(CModel* pModel);
    _int    Find_SlotByWallId(_uint iWallId);
    // 모델 메시 OffsetMatrix -> centerBind 추출 + chunk 를 bind 상태로 시드. 반환=chunk 수.
    _uint   Seed_BindState(CModel* pModel, _uint iMeshIndex, CHUNK* pOut);
    // 활성화 시 임펄스 부여(bind 시드된 chunk 에 속도/회전 추가).
    void    Apply_Impulse(CHUNK* pChunks, _uint iCount, const BREAK_PARAM& param, _float fModelRadius);
    void    Collect_Colliders(INSTANCE& inst);   // [A] 근처 정적 콜라이더 수집(브로드페이즈→모델 OBB)
    void    Update_FrameCB(_uint iFrame, _uint iRingSlot, const _float4x4& matWorld);

    // 튜닝
    _float  m_fDeltaTime = 0.f;
    _float  m_fFloorY = 0.f;
    _float3 m_vGravity = {0.f, -9.8f, 0.f};
    _float  m_fFriction = 0.85f;
    _float  m_fLifeMax = 8.0f;     // 파괴 후 이 시간 지나면 슬롯 반환
    _float  m_fDefImpulse = 3.5f;
    _float  m_fDefUpBias = 2.0f;
    _float  m_fDefSpin = 6.0f;
    _float  m_fChunkRadius = 0.15f;

public:
    static CFracture_System* Create(EngineContext* pContext);
    virtual void Free() override;
};