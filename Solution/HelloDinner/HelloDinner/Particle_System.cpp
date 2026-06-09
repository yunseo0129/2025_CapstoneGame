#include "Particle_System.h"
#include "GameInstance.h"
#include "Texture.h"   // [7b] 파티클 외형 텍스처(단일 배열 DDS) 로드 (CTexture, TEX_ARRAY)

namespace
{
    // 입자용 간단 RNG ([a, b)). 시드 고정.
    inline float RandRange(float a, float b)
    {
        return a + (b - a) * (rand() / (float)RAND_MAX);
    }

    inline _float3 NormalizeSafe(const _float3& v)
    {
        float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
        if (len2 < 1e-10f) return {0.f, 1.f, 0.f};
        float inv = 1.f / sqrtf(len2);
        return {v.x * inv, v.y * inv, v.z * inv};
    }

    inline float Saturate01(float x) { return (x < 0.f) ? 0.f : (x > 1.f ? 1.f : x); }

    // [7b] 종류별 외형(스프라이트) 매핑. GPU 거동 테이블(b2 TYPE_BEHAVIOR)과 별개인 CPU 전용 표.
    //  외형 변형(슬라이스/회전/플립 선택)은 CPU(Emit/Fill)에서만 일어나고 uPacked 로 실린다.
    //   - base/count  : 텍스처 배열에서 이 종류가 무작위로 고르는 슬라이스 범위 [base, base+count).
    //   - bProcedural : true 면 텍스처 미사용. VS->PS 로 절차적 플래그를 실어 PS 가 소프트 원을 GPU로 그린다(흙먼지).
    //  인덱스 = PARTICLE_TYPE. 텍스처 사용 종류의 (base+count) 최댓값 = PARTICLE_TEX_SLICES(6) 와 일치해야 한다.
    struct SPRITE_RANGE { _uint base; _uint count; _bool bProcedural; };
    const SPRITE_RANGE g_SpriteRanges[] =
    {
        {0, 2, false}, // KETCHUP_SPRAY : 슬라이스 0~1 (케첩 방울 2종)
        {0, 0, true}, // WALL_DEBRIS   : 흙먼지 = GPU 절차적 드로우(텍스처 미사용)
        {2, 4, false}, // WALL_DEBRIS_2 : 슬라이스 2~5 (돌/파편 4종)
    };
    static_assert(_countof(g_SpriteRanges) == CParticle_System::TYPE_END, "SPRITE_RANGE 개수와 PARTICLE_TYPE 개수가 다릅니다");

    // [7b] uPacked 빌더 (C++ 측). HLSL 언패킹과 비트 레이아웃이 정확히 대응해야 한다.
    inline _uint PackAppearance(const SPRITE_RANGE& sr)
    {
        _uint uSprite = sr.bProcedural ? 0u
            : (sr.count ? (sr.base + (_uint)(rand() % sr.count)) : sr.base);
        _uint angleQ = (_uint)(rand() & 0xFF);          // 회전 양자화 (0~255)
        _uint flipU = (rand() & 1) ? 1u : 0u;
        _uint flipV = (rand() & 1) ? 1u : 0u;
        _uint proc = sr.bProcedural ? 1u : 0u;
        return (uSprite & 0xFFu) | (angleQ << 8) | (flipU << 16) | (flipV << 17) | (proc << 18);
    }

    // [5단계] 종류별 방출 프리셋. 색·속도패턴·크기·수명을 데이터로 분리.
    struct EMIT_PRESET
    {
        _uint   iDefaultCount;            // iCount==0 일 때 사용
        float   fSpeedMin, fSpeedMax;     // 속도 크기 범위
        bool    bRadial;                  // true=사방 폭발(방향 무시) / false=vDirection 콘 분사
        float   fConeSpread;              // 콘 퍼짐(분사형). 0=직선, 클수록 넓게
        float   fUpwardBias;              // 위로 솟구치는 추가 y속도(폭발형 splatter)
        float   fSizeMin, fSizeMax;
        float   fLifeMin, fLifeMax;
        _float3 vColor;                   // 기본색
        float   fColorJitter;             // 색 랜덤 편차
    };

    // 인덱스 = CParticle_System::PARTICLE_TYPE 순서 (KETCHUP_SPRAY, WALL_DEBRIS, WALL_DEBRIS_2)
    const EMIT_PRESET g_Presets[] =
    {
        // KETCHUP_SPRAY : 방향 콘, 빠르고 작고 짧은 방울. 색은 텍스처가 냄 -> vColor 는 거의 흰색(이중채색 방지).
        {40, 6.f, 12.f, false, 0.18f, 0.f,
        0.04f, 0.09f, 0.5f, 1.1f, {0.95f, 0.92f, 0.92f}, 0.03f},
        // WALL_DEBRIS(흙먼지, 절차적) : 사방으로 느리게 퍼지며 부풀고 오래 머묾(먼지 많이). 색은 vColor 가 직접 표현(옅은 황갈/회색).
        {40, 1.2f, 4.0f, true, 0.f, 1.4f,
        0.16f, 0.40f, 1.4f, 3.0f, {0.55f, 0.47f, 0.38f}, 0.08f},
        // WALL_DEBRIS_2(파편, 텍스처) : 무겁게 빠르게 튀어 곧 낙하(파편 적게). 작게. 색은 텍스처가 냄 -> vColor 거의 흰색.
        {18, 3.0f, 8.0f, true, 0.f, 1.4f,
        0.07f, 0.16f, 0.9f, 1.8f, {0.95f, 0.93f, 0.90f}, 0.04f},
    };
    static_assert(_countof(g_Presets) == CParticle_System::TYPE_END, "EMIT_PRESET 개수와 PARTICLE_TYPE 개수가 다릅니다");

    // [6단계] 종류별 거동(물리/수명 곡선). 인덱스 = PARTICLE_TYPE. g_Presets 와 짝을 이룬다.
    //  fGravityScale: 중력 배율 / fDrag: 감쇠 / fStartSize~fEndSize: 크기 곡선 배율
    //  vStartColor~vEndColor: 색 곡선(a 로 페이드). 중력 g=(0,-G,0) 에 fGravityScale 이 곱해진다.
    const CParticle_System::TYPE_BEHAVIOR g_Behaviors[] =
    {
        // KETCHUP_SPRAY(텍스처) : 약한 중력 + 큰 공기저항, 수축. 텍스처가 색을 내므로 색곡선은 거의 흰색(이중채색 방지) + 알파 페이드.
        {0.35f, 0.80f, 1.0f, 0.40f,
        {1.00f, 1.00f, 1.00f, 1.0f}, {0.65f, 0.55f, 0.55f, 0.0f}},
        // WALL_DEBRIS(흙먼지, 절차적) : 가볍게(중력 작음) 떠다니며 공기저항 큼. 부풀어 커짐(start<end). 색은 vColor 가 직접 -> 옅은 황갈->회색, 시작 알파 낮게.
        {0.30f, 0.85f, 1.0f, 1.8f,
        {0.58f, 0.49f, 0.40f, 0.55f}, {0.46f, 0.42f, 0.38f, 0.0f}},
        // WALL_DEBRIS_2(파편, 텍스처) : 무겁게(중력 큼) 푹 떨어지고 감쇠 거의 없음. 크기 유지~약간 수축. 텍스처 색 -> 색곡선 거의 흰색 + 페이드.
        {1.70f, 0.10f, 1.0f, 0.85f,
        {1.00f, 1.00f, 1.00f, 1.0f}, {0.70f, 0.68f, 0.66f, 0.0f}},
    };
    static_assert(_countof(g_Behaviors) == CParticle_System::TYPE_END, "TYPE_BEHAVIOR 개수와 PARTICLE_TYPE 개수가 다릅니다");

    // [6단계] CPU/HLSL 레이아웃 일치 컴파일 타임 검증 (64B / 16정렬 / 오프셋 / 거동 48B)
    static_assert(sizeof(CParticle_System::PARTICLE) == 64, "PARTICLE 은 64바이트여야 합니다");
    static_assert(sizeof(CParticle_System::PARTICLE) % 16 == 0, "PARTICLE 크기는 16의 배수여야 합니다");
    static_assert(offsetof(CParticle_System::PARTICLE, iType) == 48, "iType 오프셋은 48(16*3)이어야 합니다");
    static_assert(offsetof(CParticle_System::PARTICLE, fMaxLife) == 52, "fMaxLife 오프셋은 52여야 합니다");
    static_assert(offsetof(CParticle_System::PARTICLE, fSpawnSize) == 56, "fSpawnSize 오프셋은 56이어야 합니다");
    static_assert(offsetof(CParticle_System::PARTICLE, uPacked) == 60, "uPacked 오프셋은 60이어야 합니다(이전 _pad0 자리)");
    static_assert(sizeof(CParticle_System::TYPE_BEHAVIOR) == 48, "TYPE_BEHAVIOR 는 48바이트여야 합니다");
}

CParticle_System::CParticle_System(EngineContext* pContext)
    : m_pContext {pContext}
    , m_pDevice {pContext ? pContext->device : nullptr}
    , m_pGameInstance {CGameInstance::GetInstance()}
{
    // m_pGameInstance 는 raw 참조만 사용(AddRef 안 함). 본 객체의 소유자는 GameInstance.
    // m_pContext 는 device + 로딩 cmdList. 텍스처 로딩(Load_ParticleTexture)에서만 cmdList 를 쓴다.
}

HRESULT CParticle_System::Initialize()
{
    if (nullptr == m_pDevice || nullptr == m_pGameInstance || nullptr == m_pContext)
        return E_FAIL;

    if (FAILED(Create_ParticleBuffer())) { MSG_BOX("Particle: Create_ParticleBuffer Failed"); return E_FAIL; }
    if (FAILED(Create_ParticleSRV())) { MSG_BOX("Particle: Create_ParticleSRV Failed");    return E_FAIL; }
    if (FAILED(Create_RootSignature())) { MSG_BOX("Particle: Create_RootSignature Failed");  return E_FAIL; }
    if (FAILED(Create_PSO())) { MSG_BOX("Particle: Create_PSO Failed");            return E_FAIL; }
    if (FAILED(Create_FrameCB())) { MSG_BOX("Particle: Create_FrameCB Failed");        return E_FAIL; }

    if (FAILED(Create_ComputeRootSignature())) { MSG_BOX("Particle: Create_ComputeRootSignature Failed"); return E_FAIL; }
    if (FAILED(Create_ComputePSO())) { MSG_BOX("Particle: Create_ComputePSO Failed");           return E_FAIL; }
    if (FAILED(Create_TypeCB())) { MSG_BOX("Particle: Create_TypeCB Failed");              return E_FAIL; }
    // [7b] 외형 텍스처(단일 배열 DDS)는 여기서 만들지 않는다.
    //  Initialize_Engine 시점의 cmdList 는 Close 상태이고, 텍스처 업로드는 엔진 로딩 cmdList 구간
    //  (ResetCmdList~CloseCmdList)에서 일어나야 펜스로 완료 보장되기 때문.
    //  -> Loader::Loading_Level_GamePlay 가 Load_ParticleTexture() 를 호출한다.

    if (FAILED(Create_EmitStaging())) { MSG_BOX("Particle: Create_EmitStaging Failed");          return E_FAIL; }

    if (FAILED(Create_AliveList())) { MSG_BOX("Particle: Create_AliveList Failed");            return E_FAIL; }
    if (FAILED(Create_IndirectArgs())) { MSG_BOX("Particle: Create_IndirectArgs Failed");         return E_FAIL; }

    m_PendingEmits.reserve(64);

    // [6단계] 레이아웃/정렬 런타임 확인 로그 (출력 창)
    {
        string s = "[Particle] sizeof(PARTICLE)=" + to_string(sizeof(PARTICLE))
            + " (expect 64), align16=" + (string)((sizeof(PARTICLE) % 16 == 0) ? "OK" : "FAIL")
            + ", sizeof(TYPE_BEHAVIOR)=" + to_string(sizeof(TYPE_BEHAVIOR)) + " (expect 48)"
            + ", TYPE_END=" + to_string((_uint)TYPE_END) + "\n";
        OutputDebugStringA(s.c_str());
    }

    return S_OK;
}

// ----------------------------------------------------------------------------
// DEFAULT 힙 구조화버퍼(입자 풀) + UPLOAD 스테이징
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_ParticleBuffer()
{
    const _uint bufferSize = MAX_PARTICLES * sizeof(PARTICLE);

    // --- DEFAULT 풀 버퍼 (ALLOW_UNORDERED_ACCESS: 2단계 컴퓨트 대비 미리 켬) ---
    D3D12_HEAP_PROPERTIES heapDefault = {};
    heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bufferSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (FAILED(m_pDevice->CreateCommittedResource(
        &heapDefault, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COMMON,       // 버퍼는 항상 COMMON으로 생성됨(InitialState 무시 경고 방지)
        nullptr, IID_PPV_ARGS(&m_pParticleBuffer))))
        return E_FAIL;

    // --- UPLOAD 스테이징 (테스트 입자 수만큼; 0이면 생략) ---
    if (m_iTestCount > 0)
    {
        const _uint stagingSize = m_iTestCount * sizeof(PARTICLE);

        D3D12_HEAP_PROPERTIES heapUpload = {};
        heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC udesc = desc;
        udesc.Width = stagingSize;
        udesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        if (FAILED(m_pDevice->CreateCommittedResource(
            &heapUpload, D3D12_HEAP_FLAG_NONE, &udesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&m_pUploadBuffer))))
            return E_FAIL;

        if (FAILED(m_pUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_pUploadMapped))))
            return E_FAIL;
    }

    return S_OK;
}

// ----------------------------------------------------------------------------
// 글로벌 셰이더 가시 힙(Texture_Manager)에 StructuredBuffer SRV 생성
//  - CTexture 와 동일 패턴: Get_CPUHandle -> 뷰 생성 -> Get_CurrentIndex 기록 -> Offset(1)
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_ParticleSRV()
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpu = m_pGameInstance->Get_CPUHandle();
    m_iSrvIndex = m_pGameInstance->Get_CurrentIndex();

    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = DXGI_FORMAT_UNKNOWN; // 구조화버퍼
    srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Buffer.FirstElement = 0;
    srv.Buffer.NumElements = MAX_PARTICLES;
    srv.Buffer.StructureByteStride = sizeof(PARTICLE);
    srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    m_pDevice->CreateShaderResourceView(m_pParticleBuffer.Get(), &srv, hCpu);

    m_pGameInstance->Offset_DescriptorHandle(1);

    // 2단계 UAV는 디스크립터를 만들지 않고 SetComputeRootUnorderedAccessView(루트 UAV)로 직접 바인딩한다.

    return S_OK;
}

// ----------------------------------------------------------------------------
// 전용 그래픽 루트시그: [0] b0 카메라 CBV(VS), [1] t0 입자 SRV table(VS), [2] t1 alive SRV(VS),
//  [3] t2 텍스처배열 table(PS) + s0 정적 샘플러(PS).  [7a] PS 가 텍스처를 읽으므로 DENY_PIXEL 제거.
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_RootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE srvRange[2];
    srvRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0); // t0, space0 (VS: 입자)
    srvRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0); // t2, space0 (PS: 종류별 텍스처 배열) [7a]

    CD3DX12_ROOT_PARAMETER params[4];
    params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);          // b0 카메라 CB
    params[1].InitAsDescriptorTable(1, &srvRange[0], D3D12_SHADER_VISIBILITY_VERTEX);  // t0 입자(테이블)
    params[2].InitAsShaderResourceView(1, 0, D3D12_SHADER_VISIBILITY_VERTEX);          // t1 alive 목록(루트 SRV)
    params[3].InitAsDescriptorTable(1, &srvRange[1], D3D12_SHADER_VISIBILITY_PIXEL);   // t2 텍스처 배열(테이블) [7a]

    // [7a] s0 정적 샘플러(PS). linear + clamp. 빌보드 UV [0,1] 라 clamp 가 가장자리 번짐 방지.
    CD3DX12_STATIC_SAMPLER_DESC samp(
        0,                                          // s0
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // [7a] PS 가 t2/s0 를 읽으므로 DENY_PIXEL 제거(HULL/DOMAIN/GEOMETRY 만 거부 유지).
    D3D12_ROOT_SIGNATURE_FLAGS flags =
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(_countof(params), params, 1, &samp, flags);

    ID3DBlob* pSig = nullptr;
    ID3DBlob* pErr = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSig, &pErr)))
    {
        if (pErr) { OutputDebugStringA((char*)pErr->GetBufferPointer()); pErr->Release(); }
        return E_FAIL;
    }

    HRESULT hr = m_pDevice->CreateRootSignature(
        0, pSig->GetBufferPointer(), pSig->GetBufferSize(), IID_PPV_ARGS(&m_pRootSig));

    Safe_Release(pSig);
    Safe_Release(pErr);

    return hr;
}

// ----------------------------------------------------------------------------
// 전용 PSO: 입력레이아웃 없음, 알파블렌드, 깊이 테스트 O / 기록 X, Cull None, TRIANGLE
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_PSO()
{
    ComPtr<ID3DBlob> vs;
    ComPtr<ID3DBlob> ps;
    vs.Attach(Compile_Shader(L"Shader_Particle.hlsl", "VS_Particle", "vs_5_1")); // +1 참조 소유권 이전
    ps.Attach(Compile_Shader(L"Shader_Particle.hlsl", "PS_Particle", "ps_5_1"));
    if (!vs || !ps)
        return E_FAIL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC d = {};
    d.pRootSignature = m_pRootSig.Get();
    d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    d.InputLayout = {nullptr, 0};                       // IA 입력 없음

    d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;    // 빌보드는 양면

    // 알파 블렌드
    d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    {
        D3D12_RENDER_TARGET_BLEND_DESC& b = d.BlendState.RenderTarget[0];
        b.BlendEnable = TRUE;
        b.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        b.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        b.BlendOp = D3D12_BLEND_OP_ADD;
        b.SrcBlendAlpha = D3D12_BLEND_ONE;
        b.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        b.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    // 깊이: 테스트 O, 기록 X (반투명이 불투명 뒤로 가려지되 서로 덮어쓰지 않게)
    d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    d.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    d.SampleMask = UINT_MAX;
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    d.SampleDesc.Count = 1;

    return m_pDevice->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&m_pPSO));
}

// ----------------------------------------------------------------------------
// 컴퓨트 루트시그: [0] b1 32비트 상수(5), [1] u0 입자, [2] u1 간접인자, [3] u2 alive목록
//  - 모두 루트 디스크립터(구조화/raw 버퍼라 가능). 디스크립터 테이블 없음.
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_ComputeRootSignature()
{
    CD3DX12_ROOT_PARAMETER params[5];
    params[0].InitAsConstants(5, 1, 0);             // b1: dt, maxParticles, gx, gy, gz
    params[1].InitAsUnorderedAccessView(0, 0);      // u0: RWStructuredBuffer<Particle>
    params[2].InitAsUnorderedAccessView(1, 0);      // u1: RWByteAddressBuffer (간접 인자/카운터)
    params[3].InitAsUnorderedAccessView(2, 0);      // u2: RWStructuredBuffer<uint> (alive 목록)
    params[4].InitAsConstantBufferView(2, 0);       // b2: TypeBehavior[] 종류별 거동 (루트 CBV)

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ID3DBlob* pSig = nullptr;
    ID3DBlob* pErr = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSig, &pErr)))
    {
        if (pErr) { OutputDebugStringA((char*)pErr->GetBufferPointer()); pErr->Release(); }
        return E_FAIL;
    }

    HRESULT hr = m_pDevice->CreateRootSignature(
        0, pSig->GetBufferPointer(), pSig->GetBufferSize(), IID_PPV_ARGS(&m_pComputeRootSig));

    Safe_Release(pSig);
    Safe_Release(pErr);

    return hr;
}

// ----------------------------------------------------------------------------
// 2단계 컴퓨트 PSO (CS_Update)
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_ComputePSO()
{
    ComPtr<ID3DBlob> cs;
    cs.Attach(Compile_Shader(L"Shader_Particle.hlsl", "CS_Update", "cs_5_1")); // 반환된 +1 참조 소유권 이전
    if (!cs)
        return E_FAIL;

    D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
    d.pRootSignature = m_pComputeRootSig.Get();
    d.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};

    return m_pDevice->CreateComputePipelineState(&d, IID_PPV_ARGS(&m_pComputePSO));
}

// ----------------------------------------------------------------------------
// [6단계] 종류별 거동 상수버퍼 (UPLOAD, 영속 매핑) - b2 루트 CBV 로 컴퓨트에 바인딩.
//  UPLOAD 힙/GENERIC_READ 이므로 COMMON-decay 대상이 아니다 -> 배리어 불필요(FrameCB 와 동일 패턴).
//  값은 1회 채운다(수치 조정 시 m_pTypeCBMapped 를 갱신하면 됨).
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_TypeCB()
{
    const _uint cbSize = ((_uint)sizeof(TYPE_BEHAVIOR) * TYPE_END + 255) & ~255u; // 256 정렬

    D3D12_HEAP_PROPERTIES heapUpload = {};
    heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = cbSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(m_pDevice->CreateCommittedResource(
        &heapUpload, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&m_pTypeCB))))
        return E_FAIL;

    if (FAILED(m_pTypeCB->Map(0, nullptr, reinterpret_cast<void**>(&m_pTypeCBMapped))))
        return E_FAIL;

    // 거동 테이블 1회 업로드 (g_Behaviors: 익명 네임스페이스, PARTICLE_TYPE 인덱스 순서)
    memcpy(m_pTypeCBMapped, g_Behaviors, sizeof(TYPE_BEHAVIOR) * TYPE_END);

    return S_OK;
}

// ----------------------------------------------------------------------------
// [7b] 파티클 외형 텍스처(단일 배열 DDS) 로드.
//  - CTexture(TEX_ARRAY)가 device 로 리소스 생성 + EngineContext->cmdList 로 업로드 기록 + 글로벌 힙에 SRV 등록.
//  - 반드시 엔진 로딩 cmdList 구간(MainApp: ResetCmdList~CloseCmdList)에서 호출되어야 업로드가 로딩 펜스로 완료된다.
//  - 단일 배열 DDS 이므로 iNumTextures=1. 슬라이스 수는 DDS DepthOrArraySize -> CTexture::m_iArraySize -> SRV ArraySize.
//  - mip/포맷/배리어/업로드버퍼는 CTexture 가 처리. 파티클은 Render 에서 Bind_ShaderResource(pCmd, 3) 로 t2 에 바인딩만 한다.
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Load_ParticleTexture()
{
    if (nullptr == m_pContext || nullptr == m_pContext->cmdList)
    {
        MSG_BOX("Particle: Load_ParticleTexture - EngineContext/cmdList 없음(엔진 로딩 구간에서 호출되어야 함)");
        return E_FAIL;
    }
    if (m_pParticleTex)   // 중복 로드 방지
        return S_OK;

    // 단일 배열 DDS (슬라이스 = 케첩 0~1 / 파편 2~5). 실제 파일명으로 교체할 것.
    //  경로 컨벤션: 작업 디렉토리 기준 상대경로 (다른 텍스처 로딩과 동일).
    const _tchar* szPath = L"Resources/NonAnim/Map/dds/Particle_Array.dds";

    m_pParticleTex = CTexture::Create(m_pContext, szPath, 1, TEXTURE_TYPE::TEX_ARRAY);
    if (nullptr == m_pParticleTex)
    {
        MSG_BOX("Particle: Load_ParticleTexture - DDS 로드 실패(경로/파일/포맷 확인)");
        return E_FAIL;
    }

    // 슬라이스 수 / mip 검증 로그 (DDS DepthOrArraySize == PARTICLE_TEX_SLICES 여야 슬라이스 인덱싱이 맞는다)
    {
        _uint iArray = m_pParticleTex->Get_ArraySize();
        string s = "[Particle] ParticleTex ArraySize=" + to_string(iArray)
            + " (expect " + to_string((_uint)PARTICLE_TEX_SLICES) + "), Mips="
            + to_string(m_pParticleTex->Get_MipLevels()) + "\n";
        OutputDebugStringA(s.c_str());
        if (iArray < PARTICLE_TEX_SLICES)
            OutputDebugStringA("[Particle][WARN] DDS 슬라이스 수가 PARTICLE_TEX_SLICES 보다 적습니다 - 상위 슬라이스는 클램프됩니다\n");
    }

    return S_OK;
}

// ----------------------------------------------------------------------------
// per-frame 카메라/상수 CB (UPLOAD x FRAME_COUNT, 영속 매핑) - CPU/GPU 경합 방지용 더블/트리플버퍼
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_FrameCB()
{
    m_iFrameCBStride = (sizeof(FRAME_CB) + 255) & ~255u;

    D3D12_HEAP_PROPERTIES heapUpload = {};
    heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC cdesc = {};
    cdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cdesc.Width = m_iFrameCBStride;
    cdesc.Height = 1;
    cdesc.DepthOrArraySize = 1;
    cdesc.MipLevels = 1;
    cdesc.SampleDesc.Count = 1;
    cdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    for (_int i = 0; i < FRAME_COUNT; ++i)
    {
        if (FAILED(m_pDevice->CreateCommittedResource(
            &heapUpload, D3D12_HEAP_FLAG_NONE, &cdesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&m_pFrameCB[i]))))
            return E_FAIL;

        if (FAILED(m_pFrameCB[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_pFrameCBMapped[i]))))
            return E_FAIL;
    }

    return S_OK;
}

// ----------------------------------------------------------------------------
// 셰이더 컴파일 헬퍼 (Shader_Manager 와 동일 규약)
// ----------------------------------------------------------------------------
ID3DBlob* CParticle_System::Compile_Shader(const wstring& strPath, const char* strEntry, const char* strTarget)
{
    ID3DBlob* pBlob = nullptr;
    ID3DBlob* pError = nullptr;

    UINT iFlag = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    iFlag |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    if (FAILED(D3DCompileFromFile(strPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        strEntry, strTarget, iFlag, 0, &pBlob, &pError)))
    {
        if (pError)
        {
            OutputDebugStringA((char*)pError->GetBufferPointer());
            MSG_BOX("Particle: Compile_Shader Failed");
            pError->Release();
        }
        return nullptr;
    }
    Safe_Release(pError);
    return pBlob;
}

// ----------------------------------------------------------------------------
// 첫 Compute 에서 스테이징 채움: 카메라 정면에 입자 구름 생성(어떤 씬에서도 보이게)
// ----------------------------------------------------------------------------
_bool CParticle_System::Fill_TestParticles_CPU()
{
    // 카메라 월드 행렬 = inverse(view) 에서 정면/위치 추출
    _float3 vCenter = {0.f, 0.f, 8.f};

    if (m_bSpawnInFront)
    {
        XMFLOAT4X4 v = m_pGameInstance->Get_CurrentCameraView();
        XMMATRIX matView = XMLoadFloat4x4(&v);

        XMVECTOR det;
        XMMATRIX matInvView = XMMatrixInverse(&det, matView);

        // 로딩 직후 첫 프레임엔 FPV 뷰가 0행렬(특이행렬)이라 역행렬이 NaN/Inf가 된다.
        // 그 경우 false -> Compute 가 이번 프레임 업로드를 건너뛰고 다음 프레임에 재시도.
        float fDet = XMVectorGetX(det);
        if (fDet != fDet || (fDet > -1e-8f && fDet < 1e-8f)) // NaN(자기비교 실패) 또는 0
            return false;

        XMFLOAT4X4 iv; XMStoreFloat4x4(&iv, matInvView);
        XMFLOAT3 camPos = {iv._41, iv._42, iv._43};
        XMFLOAT3 fwd = {iv._31, iv._32, iv._33}; // 카메라 전방(LH +Z)

        if (camPos.x != camPos.x) // 안전장치: 그래도 NaN이면 재시도
            return false;

        XMVECTOR vf = XMVector3Normalize(XMLoadFloat3(&fwd));
        XMStoreFloat3(&fwd, vf);

        vCenter = {camPos.x + fwd.x * 8.f,
            camPos.y + fwd.y * 8.f,
            camPos.z + fwd.z * 8.f};
    }

    for (_uint i = 0; i < m_iTestCount; ++i)
    {
        PARTICLE p;
        p.vPos = {vCenter.x + RandRange(-2.5f, 2.5f),
            vCenter.y + RandRange(-2.5f, 2.5f),
            vCenter.z + RandRange(-2.5f, 2.5f)};
        p.vVel = {0.f, 0.f, 0.f};
        p.fLife = RandRange(3.f, 6.f); // 2단계 낙하 관찰용 수명(3~6초). 다 되면 사라짐
        p.fSize = 0.12f;   // 빌보드 반경
        p.vColor = {RandRange(0.7f, 1.0f),  // 케첩 느낌 붉은 톤
            RandRange(0.1f, 0.3f),
            RandRange(0.1f, 0.2f),
            1.f};

        // [6단계] 테스트 구름도 종류/곡선 기준값 필요(미설정 시 거동 테이블 OOB/쓰레기값)
        p.iType = KETCHUP_SPRAY;
        p.fMaxLife = p.fLife;
        p.fSpawnSize = p.fSize;
        // [7b] 외형 패킹(슬라이스/회전/플립). 테스트 구름은 케첩 외형.
        p.uPacked = PackAppearance(g_SpriteRanges[KETCHUP_SPRAY]);

        m_pUploadMapped[i] = p;
    }

    return true;
}

// ----------------------------------------------------------------------------
// 3단계 방출 시드용 UPLOAD 스테이징 (영속 매핑)
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_EmitStaging()
{
    const _uint stagingSize = EMIT_STAGING_MAX * sizeof(PARTICLE);

    D3D12_HEAP_PROPERTIES heapUpload = {};
    heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = stagingSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(m_pDevice->CreateCommittedResource(
        &heapUpload, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&m_pEmitStaging))))
        return E_FAIL;

    if (FAILED(m_pEmitStaging->Map(0, nullptr, reinterpret_cast<void**>(&m_pEmitStagingMapped))))
        return E_FAIL;

    return S_OK;
}

// ----------------------------------------------------------------------------
// 4단계 alive 인덱스 목록 버퍼 (DEFAULT, UAV/SRV는 루트 디스크립터로 바인딩)
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_AliveList()
{
    const _uint bufferSize = MAX_PARTICLES * sizeof(_uint);

    D3D12_HEAP_PROPERTIES heapDefault = {};
    heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bufferSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (FAILED(m_pDevice->CreateCommittedResource(
        &heapDefault, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(&m_pAliveListBuffer))))
        return E_FAIL;

    return S_OK;
}

// ----------------------------------------------------------------------------
// 4단계 간접 인자 버퍼 + 리셋 업로드({6,0,0,0}) + DrawInstanced 커맨드 시그니처
//  - 간접 인자 버퍼는 컴퓨트에서 InstanceCount(byte 4)를 InterlockedAdd로 채우고,
//    ExecuteIndirect 가 그걸 읽어 alive 수만 그린다.
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_IndirectArgs()
{
    D3D12_HEAP_PROPERTIES heapDefault = {};
    heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeof(D3D12_DRAW_ARGUMENTS);   // 16
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (FAILED(m_pDevice->CreateCommittedResource(
        &heapDefault, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(&m_pArgsBuffer))))
        return E_FAIL;

    // 리셋 업로드 버퍼
    D3D12_HEAP_PROPERTIES heapUpload = {};
    heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC udesc = desc;
    udesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (FAILED(m_pDevice->CreateCommittedResource(
        &heapUpload, D3D12_HEAP_FLAG_NONE, &udesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&m_pArgsResetBuffer))))
        return E_FAIL;

    if (FAILED(m_pArgsResetBuffer->Map(0, nullptr, &m_pArgsResetMapped)))
        return E_FAIL;

    D3D12_DRAW_ARGUMENTS init = {};
    init.VertexCountPerInstance = 6;   // 빌보드 쿼드 6 정점
    init.InstanceCount = 0;   // 매 프레임 컴퓨트가 채움
    init.StartVertexLocation = 0;
    init.StartInstanceLocation = 0;
    memcpy(m_pArgsResetMapped, &init, sizeof(init));

    // 커맨드 시그니처 (DrawInstanced 1건; 루트 변경 인자 없음 -> pRootSignature = nullptr)
    D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

    D3D12_COMMAND_SIGNATURE_DESC csDesc = {};
    csDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
    csDesc.NumArgumentDescs = 1;
    csDesc.pArgumentDescs = &argDesc;

    if (FAILED(m_pDevice->CreateCommandSignature(&csDesc, nullptr, IID_PPV_ARGS(&m_pCommandSignature))))
        return E_FAIL;

    return S_OK;
}

// ----------------------------------------------------------------------------
// 카메라 뷰가 유효(비특이행렬)한가 -> 빌보드 축/배치의 NaN 방지
// ----------------------------------------------------------------------------
_bool CParticle_System::Is_CameraReady()
{
    XMFLOAT4X4 v = m_pGameInstance->Get_CurrentCameraView();
    XMMATRIX matView = XMLoadFloat4x4(&v);
    float fDet = XMVectorGetX(XMMatrixDeterminant(matView));
    if (fDet != fDet || (fDet > -1e-8f && fDet < 1e-8f)) // NaN 또는 0
        return false;
    return true;
}

// ----------------------------------------------------------------------------
// [5단계] 방출: 종류 프리셋으로 시드 입자를 채워 스테이징에 쓰고 펜딩 복사로 기록.
//  실제 CopyBufferRegion 은 같은 프레임의 Compute 가 풀의 링 영역으로 기록한다.
// ----------------------------------------------------------------------------
void CParticle_System::Emit(const EMIT_DESC& desc)
{
    if (!m_bUploaded)
        return;
    if ((int)desc.eType < 0 || (int)desc.eType >= (int)TYPE_END)
        return;

    const EMIT_PRESET& preset = g_Presets[desc.eType];

    _uint iCount = (desc.iCount > 0) ? desc.iCount : preset.iDefaultCount;
    if (0 == iCount)
        return;

    // 이번 프레임 스테이징 용량으로 클램프
    if (m_iStagingCursor + iCount > EMIT_STAGING_MAX)
        iCount = EMIT_STAGING_MAX - m_iStagingCursor;
    if (0 == iCount)
        return;

    // 풀 링: 끝을 넘으면 0으로 래핑(분할 복사 회피)
    if (m_iEmitCursor + iCount > MAX_PARTICLES)
        m_iEmitCursor = 0;

    const _float3 dir = NormalizeSafe(desc.vDirection);
    const _float  ex = (desc.vExtents.x > 0.f) ? desc.vExtents.x : 0.f;
    const _float  ey = (desc.vExtents.y > 0.f) ? desc.vExtents.y : 0.f;
    const _float  ez = (desc.vExtents.z > 0.f) ? desc.vExtents.z : 0.f;

    for (_uint k = 0; k < iCount; ++k)
    {
        PARTICLE p;

        // 위치: 중심 ± 스폰 영역
        p.vPos = {desc.vCenter.x + RandRange(-ex, ex),
            desc.vCenter.y + RandRange(-ey, ey),
            desc.vCenter.z + RandRange(-ez, ez)};

        // 속도: 프리셋의 패턴(사방 폭발 / 방향 콘)
        _float  spd = RandRange(preset.fSpeedMin, preset.fSpeedMax);
        _float3 vdir;
        if (preset.bRadial)
        {
            vdir = NormalizeSafe({RandRange(-1.f, 1.f), RandRange(-1.f, 1.f), RandRange(-1.f, 1.f)});
        }
        else
        {
            vdir = NormalizeSafe({dir.x + RandRange(-1.f, 1.f) * preset.fConeSpread,
                dir.y + RandRange(-1.f, 1.f) * preset.fConeSpread,
                dir.z + RandRange(-1.f, 1.f) * preset.fConeSpread});
        }
        p.vVel = {vdir.x * spd,
            vdir.y * spd + preset.fUpwardBias,
            vdir.z * spd};

        p.fLife = RandRange(preset.fLifeMin, preset.fLifeMax);
        p.fSize = RandRange(preset.fSizeMin, preset.fSizeMax);

        const float j = preset.fColorJitter;
        p.vColor = {Saturate01(preset.vColor.x + RandRange(-j, j)),
            Saturate01(preset.vColor.y + RandRange(-j, j)),
            Saturate01(preset.vColor.z + RandRange(-j, j)),
            1.f};

        // [6단계] 종류 / 수명 곡선 기준값 기록 (age = 1 - fLife/fMaxLife, 크기 곡선은 fSpawnSize 에 곱)
        p.iType = (_uint)desc.eType;
        p.fMaxLife = p.fLife;
        p.fSpawnSize = p.fSize;
        // [7b] 외형 패킹: 거동 타입의 스프라이트 범위에서 슬라이스 무작위 + 회전/플립 무작위.
        //  흙먼지(WALL_DEBRIS)는 g_SpriteRanges 가 bProcedural=true 라 proc 비트만 세팅(텍스처 미사용).
        //  -> 같은 종류라도 입자마다 슬라이스·회전이 달라 반복돼 보이지 않는다.
        p.uPacked = PackAppearance(g_SpriteRanges[desc.eType]);

        m_pEmitStagingMapped[m_iStagingCursor + k] = p;
    }

    m_PendingEmits.push_back({m_iEmitCursor, m_iStagingCursor, iCount});

    m_iEmitCursor += iCount;
    if (m_iEmitCursor >= MAX_PARTICLES) m_iEmitCursor = 0;
    m_iStagingCursor += iCount;
}

// ----------------------------------------------------------------------------
// 현재 카메라 -> 매핑된 per-frame CB
// ----------------------------------------------------------------------------
void CParticle_System::Update_FrameCB(_uint iFrame)
{
    XMFLOAT4X4 v = m_pGameInstance->Get_CurrentCameraView();
    XMFLOAT4X4 p = m_pGameInstance->Get_CurrentCameraProjection();

    XMMATRIX matView = XMLoadFloat4x4(&v);
    XMVECTOR det;
    XMMATRIX matInvView = XMMatrixInverse(&det, matView);

    // 안전장치: 뷰가 특이행렬이면 역행렬 NaN -> 빌보드 축을 기본값으로(0*NaN 전파 방지)
    _float3 vRight = {1.f, 0.f, 0.f};
    _float3 vUp = {0.f, 1.f, 0.f};
    float fDet = XMVectorGetX(det);
    if (!(fDet != fDet || (fDet > -1e-8f && fDet < 1e-8f)))
    {
        XMFLOAT4X4 iv; XMStoreFloat4x4(&iv, matInvView);
        vRight = {iv._11, iv._12, iv._13};  // 카메라 우측(월드)
        vUp = {iv._21, iv._22, iv._23};  // 카메라 상단(월드)
    }

    FRAME_CB cb;
    cb.matView = v;
    cb.matProj = p;
    cb.vCamRight = vRight;
    cb.vCamUp = vUp;
    cb.fDeltaTime = m_fDeltaTime;
    cb.iMaxParticles = MAX_PARTICLES;

    memcpy(m_pFrameCBMapped[iFrame], &cb, sizeof(FRAME_CB));
}

// ----------------------------------------------------------------------------
// Compute: 1단계는 1회 테스트 입자 업로드 + 상태 전환(COPY_DEST -> NON_PIXEL_SHADER_RESOURCE)
//          2단계는 여기서 UAV 전환 + 업데이트 CS 디스패치 + SRV 복귀를 추가.
// ----------------------------------------------------------------------------
void CParticle_System::Compute(ID3D12GraphicsCommandList* pCmd)
{
    if (nullptr == pCmd)
        return;

    // [7b] 외형 텍스처는 Load_ParticleTexture(엔진 로딩 cmdList 구간)에서 이미 업로드 + PIXEL_SHADER_RESOURCE 로
    //  올라와 있다(텍스처는 명시 전이 후 decay 없음). 따라서 Compute 시점의 1회 업로드 로직은 불필요.

    // 첫 준비: 카메라 뷰가 유효(비특이행렬)해질 때까지 대기(빌보드 축/배치 NaN 방지).
    // 유효한 첫 프레임에 (옵션) 테스트 구름을 1회 업로드하고 준비 완료로 표시한다.
    if (!m_bUploaded)
    {
        if (m_iTestCount > 0)
        {
            if (!Fill_TestParticles_CPU())   // 카메라 미준비면 false -> 다음 프레임 재시도
                return;

            m_iAliveCount = m_iTestCount;

            pCmd->CopyBufferRegion(           // COMMON -> COPY_DEST (버퍼 자동 승격)
                m_pParticleBuffer.Get(), 0,
                m_pUploadBuffer.Get(), 0,
                (UINT64)m_iTestCount * sizeof(PARTICLE));

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_pParticleBuffer.Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            pCmd->ResourceBarrier(1, &barrier);
        }
        else
        {
            if (!Is_CameraReady())            // 테스트 구름이 없어도 카메라 준비는 기다림
                return;
        }

        m_bUploaded = true;
        return;   // 준비 프레임엔 디스패치 생략 -> 다음 프레임부터 컴퓨트로 갱신
    }

    // ------------------------------------------------------------------
    // 매 프레임: (방출 복사 + 간접인자 리셋) -> UAV -> 업데이트/컴팩션 디스패치 -> 렌더 상태로
    //  버퍼들은 매 ExecuteCommandLists 후 COMMON으로 decay됨.
    // ------------------------------------------------------------------
    const _bool bHasEmits = !m_PendingEmits.empty();

    // (1) 방출 시드 복사 (COMMON -> COPY_DEST 자동 승격)
    if (bHasEmits)
    {
        for (const auto& e : m_PendingEmits)
        {
            pCmd->CopyBufferRegion(
                m_pParticleBuffer.Get(), (UINT64)e.iPoolStart * sizeof(PARTICLE),
                m_pEmitStaging.Get(), (UINT64)e.iStagingStart * sizeof(PARTICLE),
                (UINT64)e.iCount * sizeof(PARTICLE));
        }
    }

    // (2) 간접 인자 리셋: {VertexCountPerInstance=6, InstanceCount=0, 0, 0} (COMMON -> COPY_DEST)
    pCmd->CopyBufferRegion(
        m_pArgsBuffer.Get(), 0,
        m_pArgsResetBuffer.Get(), 0,
        sizeof(D3D12_DRAW_ARGUMENTS));

    // (3) UAV 쓰기 상태로 전환 (입자 / 간접인자 / alive목록 한꺼번에)
    {
        D3D12_RESOURCE_BARRIER toUav[3] = {};
        for (int i = 0; i < 3; ++i)
        {
            toUav[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            toUav[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            toUav[i].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        toUav[0].Transition.pResource = m_pParticleBuffer.Get();
        toUav[0].Transition.StateBefore = bHasEmits ? D3D12_RESOURCE_STATE_COPY_DEST  // 방출 복사로 승격됨
            : D3D12_RESOURCE_STATE_COMMON;     // decay 상태
        toUav[1].Transition.pResource = m_pArgsBuffer.Get();
        toUav[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;              // 리셋 복사로 승격됨
        toUav[2].Transition.pResource = m_pAliveListBuffer.Get();
        toUav[2].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;                 // decay 상태
        pCmd->ResourceBarrier(3, toUav);
    }

    // (4) 컴퓨트 RS/PSO + 상수/UAV 바인딩 (그래픽 RS와 별도 바인드 포인트)
    pCmd->SetComputeRootSignature(m_pComputeRootSig.Get());
    pCmd->SetPipelineState(m_pComputePSO.Get());

    CS_CONSTANTS cb;
    cb.fDeltaTime = (m_fDeltaTime > 0.05f) ? 0.05f : m_fDeltaTime; // dt 클램프(50ms): 폭발 방지
    cb.iMaxParticles = MAX_PARTICLES;
    cb.fGravityX = 0.f;
    cb.fGravityY = m_fGravityY;
    cb.fGravityZ = 0.f;
    pCmd->SetComputeRoot32BitConstants(0, 5, &cb, 0);
    pCmd->SetComputeRootUnorderedAccessView(1, m_pParticleBuffer->GetGPUVirtualAddress());   // u0 입자
    pCmd->SetComputeRootUnorderedAccessView(2, m_pArgsBuffer->GetGPUVirtualAddress());        // u1 간접인자(카운터)
    pCmd->SetComputeRootUnorderedAccessView(3, m_pAliveListBuffer->GetGPUVirtualAddress());   // u2 alive목록
    pCmd->SetComputeRootConstantBufferView(4, m_pTypeCB->GetGPUVirtualAddress());             // b2 종류별 거동 테이블

    // (5) 디스패치: 풀 전체를 갱신하며 alive 인덱스를 목록에 추가하고 InstanceCount 집계
    _uint iGroups = (MAX_PARTICLES + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
    pCmd->Dispatch(iGroups, 1, 1);

    // (6) 렌더용 상태로 전환: 입자/alive목록 -> SRV(VS 읽기), 간접인자 -> INDIRECT_ARGUMENT
    {
        D3D12_RESOURCE_BARRIER toRead[3] = {};
        for (int i = 0; i < 3; ++i)
        {
            toRead[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            toRead[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            toRead[i].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        toRead[0].Transition.pResource = m_pParticleBuffer.Get();
        toRead[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        toRead[1].Transition.pResource = m_pAliveListBuffer.Get();
        toRead[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        toRead[2].Transition.pResource = m_pArgsBuffer.Get();
        toRead[2].Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        pCmd->ResourceBarrier(3, toRead);
    }

    m_bArgsValid = true;  // 이제 ExecuteIndirect 안전

    // (7) 이번 프레임 방출 처리 완료 -> 펜딩/스테이징 커서 리셋
    m_PendingEmits.clear();
    m_iStagingCursor = 0;
}

// ----------------------------------------------------------------------------
// Render: 카메라 CB 갱신 후 빌보드 인스턴스 드로우 (블렌드 패스 이후 호출)
//  - 글로벌 SRV 힙은 Render_Begin(Bind_GlobalHeap)에서 이미 바인딩됨.
//  - 전용 RS로 전환해도 바인딩된 디스크립터 힙은 유지됨.
// ----------------------------------------------------------------------------
void CParticle_System::Render(ID3D12GraphicsCommandList* pCmd)
{
    if (nullptr == pCmd || !m_bArgsValid)   // 첫 컴퓨트 패스 완료 전엔 그리지 않음(간접 인자 미준비)
        return;

    _uint iFrame = (_uint)m_pGameInstance->GetCurrentFrameIndex();
    if (iFrame >= (_uint)FRAME_COUNT)
        iFrame = 0;

    Update_FrameCB(iFrame);

    pCmd->SetGraphicsRootSignature(m_pRootSig.Get());
    pCmd->SetPipelineState(m_pPSO.Get());

    pCmd->SetGraphicsRootConstantBufferView(0, m_pFrameCB[iFrame]->GetGPUVirtualAddress());   // b0 카메라
    pCmd->SetGraphicsRootDescriptorTable(1, m_pGameInstance->Get_GPUHandle(m_iSrvIndex));     // t0 입자(테이블)
    pCmd->SetGraphicsRootShaderResourceView(2, m_pAliveListBuffer->GetGPUVirtualAddress());   // t1 alive목록(루트 SRV)
    // [7b] t2 외형 텍스처 배열. CTexture 가 글로벌 힙의 자기 SRV(m_iSRVIndex)를 root param 3 에 바인딩.
    //  단일 배열 DDS 라 SRV 1개 -> Bind_ShaderResource(pCmd, 3) 한 번이면 충분(iTextureIndex 기본 0).
    if (m_pParticleTex)
        m_pParticleTex->Bind_ShaderResource(pCmd, (RootParameterIndex)3);

    pCmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCmd->IASetVertexBuffers(0, 0, nullptr);
    pCmd->IASetIndexBuffer(nullptr);

    // 4단계: 살아있는 입자 수(InstanceCount)만큼만 ExecuteIndirect 로 드로우. VS가 alive목록으로 인덱싱.
    pCmd->ExecuteIndirect(m_pCommandSignature.Get(), 1, m_pArgsBuffer.Get(), 0, nullptr, 0);
}

CParticle_System* CParticle_System::Create(EngineContext* pContext)
{
    CParticle_System* pInstance = new CParticle_System(pContext);
    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : CParticle_System");
        Safe_Release(pInstance);
    }
    return pInstance;
}

void CParticle_System::Free()
{
    for (_int i = 0; i < FRAME_COUNT; ++i)
    {
        if (m_pFrameCB[i] && m_pFrameCBMapped[i])
        {
            m_pFrameCB[i]->Unmap(0, nullptr);
            m_pFrameCBMapped[i] = nullptr;
        }
        m_pFrameCB[i].Reset();
    }

    if (m_pUploadBuffer && m_pUploadMapped)
    {
        m_pUploadBuffer->Unmap(0, nullptr);
        m_pUploadMapped = nullptr;
    }
    m_pUploadBuffer.Reset();

    if (m_pEmitStaging && m_pEmitStagingMapped)
    {
        m_pEmitStaging->Unmap(0, nullptr);
        m_pEmitStagingMapped = nullptr;
    }
    m_pEmitStaging.Reset();

    if (m_pArgsResetBuffer && m_pArgsResetMapped)
    {
        m_pArgsResetBuffer->Unmap(0, nullptr);
        m_pArgsResetMapped = nullptr;
    }
    m_pArgsResetBuffer.Reset();

    m_pCommandSignature.Reset();
    m_pArgsBuffer.Reset();
    m_pAliveListBuffer.Reset();

    m_pParticleBuffer.Reset();

    m_pPSO.Reset();
    m_pRootSig.Reset();

    m_pComputePSO.Reset();
    m_pComputeRootSig.Reset();

    if (m_pTypeCB && m_pTypeCBMapped)
    {
        m_pTypeCB->Unmap(0, nullptr);
        m_pTypeCBMapped = nullptr;
    }
    m_pTypeCB.Reset();

    // [7b] 외형 텍스처(CTexture): Create 가 반환한 +1 참조를 본 객체가 단독 소유 -> 여기서 Release.
    //  내부 GPU 리소스/SRV 디스크립터는 CTexture::Free 가 정리한다.
    Safe_Release(m_pParticleTex);

    // m_pGameInstance 는 raw 참조 -> 해제하지 않음.
    m_pDevice = nullptr;

    __super::Free();
}