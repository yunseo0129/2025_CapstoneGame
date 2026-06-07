#include "Particle_System.h"
#include "GameInstance.h"

namespace
{
    // 테스트 입자용 간단 RNG ([a, b))
    inline float RandRange(float a, float b)
    {
        return a + (b - a) * (rand() / (float)RAND_MAX);
    }
}

CParticle_System::CParticle_System(ID3D12Device* pDevice)
    : m_pDevice {pDevice}
    , m_pGameInstance {CGameInstance::GetInstance()}
{
    // m_pGameInstance 는 raw 참조만 사용
}

HRESULT CParticle_System::Initialize()
{
    if (nullptr == m_pDevice || nullptr == m_pGameInstance)
        return E_FAIL;

    if (FAILED(Create_ParticleBuffer())) { MSG_BOX("Particle: Create_ParticleBuffer Failed"); return E_FAIL; }
    if (FAILED(Create_ParticleSRV())) { MSG_BOX("Particle: Create_ParticleSRV Failed");    return E_FAIL; }
    if (FAILED(Create_RootSignature())) { MSG_BOX("Particle: Create_RootSignature Failed");  return E_FAIL; }
    if (FAILED(Create_PSO())) { MSG_BOX("Particle: Create_PSO Failed");            return E_FAIL; }
    if (FAILED(Create_FrameCB())) { MSG_BOX("Particle: Create_FrameCB Failed");        return E_FAIL; }

    if (FAILED(Create_ComputeRootSignature())) { MSG_BOX("Particle: Create_ComputeRootSignature Failed"); return E_FAIL; }
    if (FAILED(Create_ComputePSO())) { MSG_BOX("Particle: Create_ComputePSO Failed");           return E_FAIL; }

    if (FAILED(Create_EmitStaging())) { MSG_BOX("Particle: Create_EmitStaging Failed");          return E_FAIL; }

    m_PendingEmits.reserve(64);

    return S_OK;
}

// ----------------------------------------------------------------------------
// DEFAULT 힙 구조화버퍼(입자 풀) + UPLOAD 스테이징
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_ParticleBuffer()
{
    const _uint bufferSize = MAX_PARTICLES * sizeof(PARTICLE);

    // --- DEFAULT 풀 버퍼
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
// 전용 그래픽 루트시그: [0] b0 카메라 CBV(VS), [1] t0 StructuredBuffer SRV table(VS)
//  - 입력레이아웃/텍스처/샘플러 없음. PS는 루트 리소스 미사용(접근 거부).
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_RootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE srvRange[1];
    srvRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0); // t0, space0

    CD3DX12_ROOT_PARAMETER params[2];
    params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);          // b0
    params[1].InitAsDescriptorTable(1, &srvRange[0], D3D12_SHADER_VISIBILITY_VERTEX);  // t0

    D3D12_ROOT_SIGNATURE_FLAGS flags =
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(_countof(params), params, 0, nullptr, flags);

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
// 2단계 컴퓨트 루트시그: [0] b1 32비트 상수(5개), [1] u0 루트 UAV
//  - 디스크립터 테이블 없음(구조화버퍼라 루트 UAV로 직접 바인딩 가능).
// ----------------------------------------------------------------------------
HRESULT CParticle_System::Create_ComputeRootSignature()
{
    CD3DX12_ROOT_PARAMETER params[2];
    params[0].InitAsConstants(5, 1, 0);             // b1: dt, maxParticles, gx, gy, gz
    params[1].InitAsUnorderedAccessView(0, 0);      // u0: RWStructuredBuffer (루트 UAV)

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
// [3단계] 방출: 시드 입자를 스테이징에 쓰고 펜딩 복사로 기록. 실제 CopyBufferRegion은 Compute가 기록.
//  - 게임 로직(CMap::Break 등)에서 호출. 같은 프레임의 Compute에서 풀의 링 영역으로 복사된다.
// ----------------------------------------------------------------------------
void CParticle_System::Emit(const _float3& vCenter, const _float3& vExtents, _uint iCount)
{
    if (!m_bUploaded || 0 == iCount)
        return;

    // 이번 프레임 스테이징 용량으로 클램프
    if (m_iStagingCursor + iCount > EMIT_STAGING_MAX)
        iCount = EMIT_STAGING_MAX - m_iStagingCursor;
    if (0 == iCount)
        return;

    // 풀 링: 끝을 넘으면 0으로 래핑(분할 복사 회피)
    if (m_iEmitCursor + iCount > MAX_PARTICLES)
        m_iEmitCursor = 0;

    // 안전한 양의 분출 범위
    _float ex = (vExtents.x > 0.01f) ? vExtents.x : 0.5f;
    _float ey = (vExtents.y > 0.01f) ? vExtents.y : 0.5f;
    _float ez = (vExtents.z > 0.01f) ? vExtents.z : 0.5f;

    for (_uint k = 0; k < iCount; ++k)
    {
        PARTICLE p;
        // 위치: 벽 중심 ± 범위 내 랜덤
        p.vPos = {vCenter.x + RandRange(-ex, ex),
            vCenter.y + RandRange(-ey, ey),
            vCenter.z + RandRange(-ez, ez)};
        // 속도: 사방으로 튀고 위로 솟구치는 폭발(이후 컴퓨트의 중력으로 낙하 -> 케첩 분사 느낌)
        p.vVel = {RandRange(-3.f, 3.f),
            RandRange(2.f, 6.f),
            RandRange(-3.f, 3.f)};
        p.fLife = RandRange(0.8f, 2.0f);
        p.fSize = RandRange(0.06f, 0.14f);
        p.vColor = {RandRange(0.7f, 1.0f), // 케첩 레드
            RandRange(0.05f, 0.2f),
            RandRange(0.05f, 0.15f),
            1.f};
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
    // 컴퓨트 업데이트: NON_PIXEL_SHADER_RESOURCE -> UAV -> Dispatch -> 다시 SRV 상태
    // ------------------------------------------------------------------

    // (1) 이번 프레임 방출 시드 복사 후 UAV로 전환. 버퍼는 매 ExecuteCommandLists 후 COMMON으로 decay됨.
    if (!m_PendingEmits.empty())
    {
        // 복사 사용 시 COMMON -> COPY_DEST 자동 승격
        for (const auto& e : m_PendingEmits)
        {
            pCmd->CopyBufferRegion(
                m_pParticleBuffer.Get(), (UINT64)e.iPoolStart * sizeof(PARTICLE),
                m_pEmitStaging.Get(), (UINT64)e.iStagingStart * sizeof(PARTICLE),
                (UINT64)e.iCount * sizeof(PARTICLE));
        }

        D3D12_RESOURCE_BARRIER toUav = {};
        toUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toUav.Transition.pResource = m_pParticleBuffer.Get();
        toUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toUav.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;   // 복사로 승격된 상태
        toUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        pCmd->ResourceBarrier(1, &toUav);
    }
    else
    {
        D3D12_RESOURCE_BARRIER toUav = {};
        toUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toUav.Transition.pResource = m_pParticleBuffer.Get();
        toUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toUav.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;      // decay된 상태
        toUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        pCmd->ResourceBarrier(1, &toUav);
    }

    // (2) 컴퓨트 RS/PSO 바인딩 (그래픽 RS와 별도 바인드 포인트라 메인 패스에 영향 없음)
    pCmd->SetComputeRootSignature(m_pComputeRootSig.Get());
    pCmd->SetPipelineState(m_pComputePSO.Get());

    // (3) 루트 32비트 상수(b1) + 루트 UAV(u0) 바인딩
    CS_CONSTANTS cb;
    cb.fDeltaTime = (m_fDeltaTime > 0.05f) ? 0.05f : m_fDeltaTime; // dt 클램프(50ms): 폭발 방지
    cb.iMaxParticles = MAX_PARTICLES;
    cb.fGravityX = 0.f;
    cb.fGravityY = m_fGravityY;
    cb.fGravityZ = 0.f;
    pCmd->SetComputeRoot32BitConstants(0, 5, &cb, 0);
    pCmd->SetComputeRootUnorderedAccessView(1, m_pParticleBuffer->GetGPUVirtualAddress());

    // (4) 디스패치 (풀 전체를 256 스레드 그룹으로)
    _uint iGroups = (MAX_PARTICLES + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
    pCmd->Dispatch(iGroups, 1, 1);

    // (5) UAV 쓰기 상태 -> SRV 읽기 상태 (Render 의 VS 가 읽을 수 있도록)
    {
        D3D12_RESOURCE_BARRIER toSrv = {};
        toSrv.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toSrv.Transition.pResource = m_pParticleBuffer.Get();
        toSrv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        toSrv.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        pCmd->ResourceBarrier(1, &toSrv);
    }

    // (6) 이번 프레임 방출 처리 완료 -> 펜딩/스테이징 커서 리셋
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
    if (nullptr == pCmd || !m_bUploaded)
        return;

    _uint iFrame = (_uint)m_pGameInstance->GetCurrentFrameIndex();
    if (iFrame >= (_uint)FRAME_COUNT)
        iFrame = 0;

    Update_FrameCB(iFrame);

    pCmd->SetGraphicsRootSignature(m_pRootSig.Get());
    pCmd->SetPipelineState(m_pPSO.Get());

    pCmd->SetGraphicsRootConstantBufferView(0, m_pFrameCB[iFrame]->GetGPUVirtualAddress());
    pCmd->SetGraphicsRootDescriptorTable(1, m_pGameInstance->Get_GPUHandle(m_iSrvIndex));

    pCmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCmd->IASetVertexBuffers(0, 0, nullptr);
    pCmd->IASetIndexBuffer(nullptr);

    // 3단계: 풀 전체를 인스턴스로 드로우. 죽은(life<=0)/빈 슬롯은 VS에서 size 0 디제너릿으로 사라짐.
    // (4단계 ExecuteIndirect 에서 alive 수만 그리도록 최적화 예정)
    pCmd->DrawInstanced(6, MAX_PARTICLES, 0, 0);
}

CParticle_System* CParticle_System::Create(ID3D12Device* pDevice)
{
    CParticle_System* pInstance = new CParticle_System(pDevice);
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

    m_pParticleBuffer.Reset();

    m_pPSO.Reset();
    m_pRootSig.Reset();

    m_pComputePSO.Reset();
    m_pComputeRootSig.Reset();

    // m_pGameInstance 는 raw 참조 -> 해제하지 않음.
    m_pDevice = nullptr;

    __super::Free();
}