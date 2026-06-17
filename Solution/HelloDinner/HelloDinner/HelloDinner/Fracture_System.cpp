#include "Fracture_System.h"
#include "GameInstance.h"
#include "Model.h"
#include "Texture.h"
#include "Collider.h"
#include "Bounding.h"

namespace
{
    // 결정론 난수: 시드 기반 xorshift32. 같은 seed → 같은 수열 → 모든 클라 동일 파괴.
    //  (rand() 는 클라마다 달라 멀티에서 조각이 어긋나므로 사용 금지)
    struct DetRand
    {
        _uint s;
        explicit DetRand(_uint seed): s(seed ? seed : 0x9E3779B9u) {}
        _uint next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        float unit() { return (next() & 0xFFFFFFu) / (float)0x1000000; } // [0,1)
        float sym(float a) { return (unit() * 2.f - 1.f) * a; }          // [-a,a)
    };

    inline _float3 NormalizeSafe(const _float3& v, const _float3& fb)
    {
        float l2 = v.x * v.x + v.y * v.y + v.z * v.z;
        if (l2 < 1e-8f) return fb;
        float inv = 1.f / sqrtf(l2);
        return {v.x * inv, v.y * inv, v.z * inv};
    }
    static_assert(sizeof(CFracture_System::CHUNK) == 80, "CHUNK must be 80 bytes (matches HLSL Chunk)");
    static_assert(sizeof(CFracture_System::CHUNK) % 16 == 0, "CHUNK must be 16-aligned");

    // 공통 전이 배리어 헬퍼
    inline void Transition(ID3D12GraphicsCommandList* cmd, ID3D12Resource* res,
        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        D3D12_RESOURCE_BARRIER b = {};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = res;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = before;
        b.Transition.StateAfter = after;
        cmd->ResourceBarrier(1, &b);
    }
}

CFracture_System::CFracture_System(EngineContext* pContext)
    : m_pContext {pContext}
    , m_pDevice {pContext ? pContext->device : nullptr}
    , m_pGameInstance {CGameInstance::GetInstance()}
{
}

HRESULT CFracture_System::Initialize()
{
    if (nullptr == m_pDevice || nullptr == m_pGameInstance || nullptr == m_pContext)
        return E_FAIL;

    if (FAILED(Create_ComputeRS_PSO())) { MSG_BOX("Fracture: Compute RS/PSO Failed"); return E_FAIL; }
    if (FAILED(Create_GraphicRS_PSO())) { MSG_BOX("Fracture: Graphic RS/PSO Failed"); return E_FAIL; }
    if (FAILED(Create_FrameCB())) { MSG_BOX("Fracture: FrameCB Failed");        return E_FAIL; }

    for (_uint i = 0; i < MAX_FRACTURE_WALLS; ++i)
        if (FAILED(Create_InstanceBuffers(m_Instances[i]))) { MSG_BOX("Fracture: Instance Buffers Failed"); return E_FAIL; }

    return S_OK;
}

// ----------------------------------------------------------------------------
//  슬롯 1개분 자원: chunk(DEFAULT UAV) + upload(UPLOAD) + matrix(DEFAULT UAV) + matrix SRV
// ----------------------------------------------------------------------------
HRESULT CFracture_System::Create_InstanceBuffers(INSTANCE& inst)
{
    D3D12_HEAP_PROPERTIES heapDefault = {}; heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_HEAP_PROPERTIES heapUpload = {}; heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

    auto MakeBuf = [](UINT64 size, _bool uav) {
        D3D12_RESOURCE_DESC d = {};
        d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        d.Width = size; d.Height = 1; d.DepthOrArraySize = 1; d.MipLevels = 1;
        d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        d.Flags = uav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
        return d;
        };

    { // chunk (DEFAULT, UAV)
        D3D12_RESOURCE_DESC d = MakeBuf((UINT64)MAX_CHUNKS * sizeof(CHUNK), true);
        if (FAILED(m_pDevice->CreateCommittedResource(&heapDefault, D3D12_HEAP_FLAG_NONE, &d,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&inst.pChunkBuffer)))) return E_FAIL;
    }
    { // upload (UPLOAD, 영속 매핑)
        D3D12_RESOURCE_DESC d = MakeBuf((UINT64)MAX_CHUNKS * sizeof(CHUNK), false);
        if (FAILED(m_pDevice->CreateCommittedResource(&heapUpload, D3D12_HEAP_FLAG_NONE, &d,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&inst.pChunkUpload)))) return E_FAIL;
        if (FAILED(inst.pChunkUpload->Map(0, nullptr, reinterpret_cast<void**>(&inst.pUploadMapped)))) return E_FAIL;
    }
    { // matrix (DEFAULT, UAV -> SRV)
        D3D12_RESOURCE_DESC d = MakeBuf((UINT64)MAX_CHUNKS * sizeof(_float4x4), true);
        if (FAILED(m_pDevice->CreateCommittedResource(&heapDefault, D3D12_HEAP_FLAG_NONE, &d,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&inst.pMatrixBuffer)))) return E_FAIL;
    }
    { // matrix SRV → 글로벌 셰이더 가시 힙
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpu = m_pGameInstance->Get_CPUHandle();
        inst.iMatrixSrvIndex = m_pGameInstance->Get_CurrentIndex();

        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = DXGI_FORMAT_UNKNOWN;
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Buffer.NumElements = MAX_CHUNKS;
        srv.Buffer.StructureByteStride = sizeof(_float4x4);
        m_pDevice->CreateShaderResourceView(inst.pMatrixBuffer.Get(), &srv, hCpu);
        m_pGameInstance->Offset_DescriptorHandle(1);
    }

    { // [A] collider (UPLOAD, 영속 매핑) — Break 시 근처 OBB 채움, 컴퓨트가 t0 로 읽음
        D3D12_RESOURCE_DESC d = MakeBuf((UINT64)MAX_COLLIDERS_PER_WALL * sizeof(GPU_COLLIDER), false);
        if (FAILED(m_pDevice->CreateCommittedResource(&heapUpload, D3D12_HEAP_FLAG_NONE, &d,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&inst.pColliderBuffer)))) return E_FAIL;
        if (FAILED(inst.pColliderBuffer->Map(0, nullptr, reinterpret_cast<void**>(&inst.pColliderMapped)))) return E_FAIL;
        inst.iColliderCount = 0;
    }

    return S_OK;
}

// ----------------------------------------------------------------------------
//  컴퓨트 RS: [0] b0 32비트상수(8) / [1] u0 chunk / [2] u1 matrix
// ----------------------------------------------------------------------------
HRESULT CFracture_System::Create_ComputeRS_PSO()
{
    CD3DX12_ROOT_PARAMETER p[4];
    p[0].InitAsConstants(8, 0, 0);
    p[1].InitAsUnorderedAccessView(0, 0);
    p[2].InitAsUnorderedAccessView(1, 0);
    p[3].InitAsShaderResourceView(0, 0);   // [A] t0: 콜라이더 버퍼

    CD3DX12_ROOT_SIGNATURE_DESC rs;
    rs.Init(_countof(p), p, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ID3DBlob* pSig = nullptr; ID3DBlob* pErr = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &pSig, &pErr)))
    {
        if (pErr) { OutputDebugStringA((char*)pErr->GetBufferPointer()); pErr->Release(); } return E_FAIL;
    }
    HRESULT hr = m_pDevice->CreateRootSignature(0, pSig->GetBufferPointer(), pSig->GetBufferSize(), IID_PPV_ARGS(&m_pComputeRS));
    Safe_Release(pSig); Safe_Release(pErr);
    if (FAILED(hr)) return hr;

    ComPtr<ID3DBlob> cs;
    cs.Attach(Compile_Shader(L"Shader_Fracture_Compute.hlsl", "CS_Fracture", "cs_5_1"));
    if (!cs) return E_FAIL;

    D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
    d.pRootSignature = m_pComputeRS.Get();
    d.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};
    return m_pDevice->CreateComputePipelineState(&d, IID_PPV_ARGS(&m_pComputePSO));
}

// ----------------------------------------------------------------------------
//  그래픽 RS: [0] b0 CBV / [1] t0 matrix SRV(VS) / [2] t1 diffuse(PS) + s0
// ----------------------------------------------------------------------------
HRESULT CFracture_System::Create_GraphicRS_PSO()
{
    CD3DX12_DESCRIPTOR_RANGE rMtx, rTex;
    rMtx.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0); // t0
    rTex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0); // t1

    CD3DX12_ROOT_PARAMETER p[3];
    p[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
    p[1].InitAsDescriptorTable(1, &rMtx, D3D12_SHADER_VISIBILITY_VERTEX);
    p[2].InitAsDescriptorTable(1, &rTex, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC samp(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_FLAGS flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    CD3DX12_ROOT_SIGNATURE_DESC rs;
    rs.Init(_countof(p), p, 1, &samp, flags);

    ID3DBlob* pSig = nullptr; ID3DBlob* pErr = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &pSig, &pErr)))
    {
        if (pErr) { OutputDebugStringA((char*)pErr->GetBufferPointer()); pErr->Release(); } return E_FAIL;
    }
    HRESULT hr = m_pDevice->CreateRootSignature(0, pSig->GetBufferPointer(), pSig->GetBufferSize(), IID_PPV_ARGS(&m_pGraphicRS));
    Safe_Release(pSig); Safe_Release(pErr);
    if (FAILED(hr)) return hr;

    ComPtr<ID3DBlob> vs, ps;
    vs.Attach(Compile_Shader(L"Shader_Fracture.hlsl", "VS_Fracture", "vs_5_1"));
    ps.Attach(Compile_Shader(L"Shader_Fracture.hlsl", "PS_Fracture", "ps_5_1"));
    if (!vs || !ps) return E_FAIL;

    // VTXANIMMESH 입력 레이아웃 (엔진 정의와 오프셋 일치 확인됨)
    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC d = {};
    d.pRootSignature = m_pGraphicRS.Get();
    d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    d.InputLayout = {layout, _countof(layout)};
    d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    d.SampleMask = UINT_MAX;
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    d.SampleDesc.Count = 1;

    return m_pDevice->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&m_pGraphicPSO));
}

HRESULT CFracture_System::Create_FrameCB()
{
    m_iFrameCBStride = (sizeof(FRAME_CB) + 255) & ~255u;
    const UINT64 ringBytes = (UINT64)m_iFrameCBStride * MAX_FRACTURE_WALLS;

    D3D12_HEAP_PROPERTIES heapUpload = {}; heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC d = {};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = ringBytes; d.Height = 1; d.DepthOrArraySize = 1; d.MipLevels = 1;
    d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    for (_int i = 0; i < FRAME_COUNT; ++i)
    {
        if (FAILED(m_pDevice->CreateCommittedResource(&heapUpload, D3D12_HEAP_FLAG_NONE, &d,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pFrameCB[i])))) return E_FAIL;
        if (FAILED(m_pFrameCB[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_pFrameCBMapped[i])))) return E_FAIL;
    }
    return S_OK;
}

ID3DBlob* CFracture_System::Compile_Shader(const wstring& strPath, const char* strEntry, const char* strTarget)
{
    ID3DBlob* pBlob = nullptr; ID3DBlob* pError = nullptr;
    UINT iFlag = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    iFlag |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    if (FAILED(D3DCompileFromFile(strPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        strEntry, strTarget, iFlag, 0, &pBlob, &pError)))
    {
        if (pError) { OutputDebugStringA((char*)pError->GetBufferPointer()); MSG_BOX("Fracture: Compile_Shader Failed"); pError->Release(); }
        return nullptr;
    }
    Safe_Release(pError);
    return pBlob;
}

// ----------------------------------------------------------------------------
//  슬롯 검색
// ----------------------------------------------------------------------------
_int CFracture_System::Find_FreeSlot()
{
    for (_uint i = 0; i < MAX_FRACTURE_WALLS; ++i)
        if (!m_Instances[i].bUsed) return (_int)i;
    return -1;
}
_int CFracture_System::Find_SlotByModel(CModel* pModel)
{
    for (_uint i = 0; i < MAX_FRACTURE_WALLS; ++i)
        if (m_Instances[i].bUsed && m_Instances[i].pModel == pModel) return (_int)i;
    return -1;
}
_int CFracture_System::Find_SlotByWallId(_uint iWallId)
{
    for (_uint i = 0; i < MAX_FRACTURE_WALLS; ++i)
        if (m_Instances[i].bUsed && m_Instances[i].iWallId == iWallId) return (_int)i;
    return -1;
}

// ----------------------------------------------------------------------------
//  centerBind 추출 + chunk 를 bind 상태(invMass=0, 정지)로 시드. 반환=chunk 수.
// ----------------------------------------------------------------------------
_uint CFracture_System::Seed_BindState(CModel* pModel, _uint iMeshIndex, CHUNK* pOut)
{
    _uint iCount = pModel->Get_MeshNumBones(iMeshIndex);
    const _float4x4* pOffsets = pModel->Get_MeshOffsetMatrices(iMeshIndex);
    if (0 == iCount || nullptr == pOffsets) return 0;
    if (iCount > MAX_CHUNKS)
    {
        char s[160]; sprintf_s(s, "[Fracture][WARN] chunk %u > MAX_CHUNKS %u -> 초과분 누락\n", iCount, MAX_CHUNKS);
        OutputDebugStringA(s);
        iCount = MAX_CHUNKS;
    }

    for (_uint i = 0; i < iCount; ++i)
    {
        XMMATRIX offset = XMLoadFloat4x4(&pOffsets[i]);
        XMVECTOR det;
        XMMATRIX bindWorld = XMMatrixInverse(&det, offset);
        XMFLOAT4X4 bw; XMStoreFloat4x4(&bw, bindWorld);

        CHUNK& c = pOut[i];
        c.vCenterBind = {bw._41, bw._42, bw._43};
        c.vPos = c.vCenterBind;
        c.qRot = {0.f, 0.f, 0.f, 1.f};   // identity
        c.fInvMass = 0.f;                      // 비활성(bind)
        c.fRestitution = 0.30f;
        c.fDrag = 0.20f;
        c.fRadius = m_fChunkRadius;
        c.vLinVel = {0.f, 0.f, 0.f};
        c.vAngVel = {0.f, 0.f, 0.f};
    }
    return iCount;
}

// 활성화 시 임펄스(중심에서 바깥 + 위 + 결정론 난수) 부여
void CFracture_System::Apply_Impulse(CHUNK* pChunks, _uint iCount, const BREAK_PARAM& param, _float fModelRadius)
{
    const float fImpulse = (param.fImpulse > 0.f) ? param.fImpulse : m_fDefImpulse;
    const float fUpBias = (param.fUpBias > 0.f) ? param.fUpBias : m_fDefUpBias;
    const float fSpin = (param.fSpin > 0.f) ? param.fSpin : m_fDefSpin;

    _float3 centroid = {0.f, 0.f, 0.f};
    for (_uint i = 0; i < iCount; ++i)
    {
        centroid.x += pChunks[i].vCenterBind.x; centroid.y += pChunks[i].vCenterBind.y; centroid.z += pChunks[i].vCenterBind.z;
    }
    centroid.x /= iCount; centroid.y /= iCount; centroid.z /= iCount;

    // [스케일] 속도/지터/바닥반경을 모델 공간 반경에 비례 → 모델이 작아도(배치 스케일 큼) 한 프레임에
    //   날아가 사라지지 않는다. 각속도(fSpin)는 크기 무관이라 스케일하지 않는다.
    const float R = (fModelRadius > 1e-6f) ? fModelRadius : 1.f;
    const float fJitter = 0.6f * R;

    // 시드 기반 결정론 난수. chunk 순서가 모든 클라에서 동일(본 인덱스)하므로 결과도 동일.
    DetRand rng(param.iSeed);

    for (_uint i = 0; i < iCount; ++i)
    {
        CHUNK& c = pChunks[i];
        _float3 dir = {c.vCenterBind.x - centroid.x, c.vCenterBind.y - centroid.y, c.vCenterBind.z - centroid.z};
        dir = NormalizeSafe(dir, {0.f, 1.f, 0.f});
        c.fInvMass = 1.f;   // 활성
        c.vLinVel = {dir.x * fImpulse * R + rng.sym(fJitter),
            dir.y * fImpulse * R + fUpBias * R + rng.sym(fJitter),
            dir.z * fImpulse * R + rng.sym(fJitter)};
        c.fRadius = m_fChunkRadius * R;   // [스케일] 바닥 충돌 반경도 모델 크기 비례
        c.vAngVel = {rng.sym(fSpin), rng.sym(fSpin), rng.sym(fSpin)};
    }
}

// ----------------------------------------------------------------------------
//  Register / Break / Unregister
// ----------------------------------------------------------------------------
_int CFracture_System::Register(CModel* pModel, _uint iWallId, const _float4x4& matWorld, _uint iMeshIndex, class CCollider* pSelfCollider)
{
    if (nullptr == pModel) return -1;

    _int s = Find_SlotByWallId(iWallId);
    if (s >= 0) { m_Instances[s].matWorld = matWorld; return s; }   // 이미 등록(같은 wallId) → world 갱신

    s = Find_FreeSlot();
    if (s < 0) { OutputDebugStringA("[Fracture][WARN] 빈 슬롯 없음(MAX_FRACTURE_WALLS 초과)\n"); return -1; }

    INSTANCE& inst = m_Instances[s];
    _uint cnt = Seed_BindState(pModel, iMeshIndex, inst.pUploadMapped);
    if (0 == cnt) return -1;

    // [스케일] 모델 공간 반경 = 조각 중심들의 무게중심 평균 거리. 임펄스/중력 스케일 기준.
    {
        _float3 cen = {0.f, 0.f, 0.f};
        for (_uint i = 0; i < cnt; ++i)
        {
            cen.x += inst.pUploadMapped[i].vCenterBind.x; cen.y += inst.pUploadMapped[i].vCenterBind.y; cen.z += inst.pUploadMapped[i].vCenterBind.z;
        }
        cen.x /= cnt; cen.y /= cnt; cen.z /= cnt;
        float rsum = 0.f;
        for (_uint i = 0; i < cnt; ++i)
        {
            const _float3& p = inst.pUploadMapped[i].vCenterBind;
            float dx = p.x - cen.x, dy = p.y - cen.y, dz = p.z - cen.z;
            rsum += sqrtf(dx * dx + dy * dy + dz * dz);
        }
        inst.fModelRadius = (rsum > 1e-6f) ? (rsum / cnt) : 1.f;
    }

    inst.pModel = pModel; inst.iWallId = iWallId; inst.matWorld = matWorld; inst.iMeshIndex = iMeshIndex;
    inst.pSelfCollider = pSelfCollider;
    inst.iChunkCount = cnt;
    inst.bUsed = true; inst.bBroken = false; inst.bSeedDirty = true; inst.fLife = 0.f;
    return s;
}

void CFracture_System::Break_ByWallId(_uint iWallId, const BREAK_PARAM& param)
{
    _int s = Find_SlotByWallId(iWallId);
    if (s < 0) { OutputDebugStringA("[Fracture][WARN] 미등록 wallId Break 시도\n"); return; }
    Break_BySlot(s, param);
}

void CFracture_System::Break(CModel* pModel, const BREAK_PARAM& param)
{
    _int s = Find_SlotByModel(pModel);
    if (s < 0) { OutputDebugStringA("[Fracture][WARN] 미등록 모델 Break 시도(먼저 Register 필요)\n"); return; }
    Break_BySlot(s, param);
}

void CFracture_System::Break_BySlot(_int iSlot, const BREAK_PARAM& param)
{
    if (iSlot < 0 || iSlot >= (_int)MAX_FRACTURE_WALLS) return;
    INSTANCE& inst = m_Instances[iSlot];
    if (!inst.bUsed || inst.bBroken) return;

    // bind 로 다시 시드 후 임펄스 부여 → 업로드 재전송
    Seed_BindState(inst.pModel, inst.iMeshIndex, inst.pUploadMapped);
    Apply_Impulse(inst.pUploadMapped, inst.iChunkCount, param, inst.fModelRadius);
    inst.bBroken = true; inst.bSeedDirty = true; inst.fLife = 0.f;

    // [A] 근처 정적 콜라이더 수집(정적이라 1회). 2단계에서 GPU 충돌에 사용.
    Collect_Colliders(inst);
}

// [A-1단계] 근처 정적 콜라이더를 BVH로 질의 → 모델 공간 OBB로 변환 → 로그(검증).
//  2단계에서 이 결과를 콜라이더 버퍼(루트 SRV)로 올려 컴퓨트 셰이더 sphere-vs-OBB 충돌에 사용한다.
void CFracture_System::Collect_Colliders(INSTANCE& inst)
{
    if (nullptr == m_pGameInstance || 0 == inst.iChunkCount) return;

    // 1) 벽 월드 중심 = 조각 무게중심(모델) 변환.
    _float3 mc = {0.f, 0.f, 0.f};
    for (_uint i = 0; i < inst.iChunkCount; ++i)
    {
        mc.x += inst.pUploadMapped[i].vCenterBind.x; mc.y += inst.pUploadMapped[i].vCenterBind.y; mc.z += inst.pUploadMapped[i].vCenterBind.z;
    }
    mc.x /= inst.iChunkCount; mc.y /= inst.iChunkCount; mc.z /= inst.iChunkCount;

    XMMATRIX W = XMLoadFloat4x4(&inst.matWorld);
    _float3 worldCenter; XMStoreFloat3(&worldCenter, XMVector3Transform(XMLoadFloat3(&mc), W));

    // 2) 월드 스케일(행 길이 평균) → 질의 반경.
    XMFLOAT4X4 w; XMStoreFloat4x4(&w, W);
    float sx = sqrtf(w._11 * w._11 + w._12 * w._12 + w._13 * w._13);
    float sy = sqrtf(w._21 * w._21 + w._22 * w._22 + w._23 * w._23);
    float sz = sqrtf(w._31 * w._31 + w._32 * w._32 + w._33 * w._33);
    float scaleAvg = (sx + sy + sz) / 3.f;
    float queryR = inst.fModelRadius * scaleAvg * 2.5f + 1.0f;

    // [진단] 모델 X/Y/Z 축이 월드에서 어디를 향하는지 — 중력 축 어긋남 확인용.
    {
        char sb[220];
        sprintf_s(sb, "[Fracture] wall %u matWorld basis  X->(%.2f,%.2f,%.2f)  Y->(%.2f,%.2f,%.2f)  Z->(%.2f,%.2f,%.2f)\n",
            inst.iWallId, w._11, w._12, w._13, w._21, w._22, w._23, w._31, w._32, w._33);
        OutputDebugStringA(sb);
    }

    // 3) BVH 브로드페이즈(정적이라 1회).
    vector<CCollider*> hits;
    m_pGameInstance->Query_StaticColliders(worldCenter, queryR, hits);

    // 4) 각 콜라이더의 월드 AABB → 모델 공간 OBB.  (Step1: 로그만 / Step2: 버퍼 패킹 + 자기 벽 제외)
    //    행렬은 row-major: 월드축은 invW 의 '행'으로 매핑되므로 행 길이로 extent 를 스케일.
    XMMATRIX invW = XMMatrixInverse(nullptr, W);
    XMFLOAT4X4 iw; XMStoreFloat4x4(&iw, invW);
    float lx = sqrtf(iw._11 * iw._11 + iw._12 * iw._12 + iw._13 * iw._13);
    float ly = sqrtf(iw._21 * iw._21 + iw._22 * iw._22 + iw._23 * iw._23);
    float lz = sqrtf(iw._31 * iw._31 + iw._32 * iw._32 + iw._33 * iw._33);

    // 모델 공간 OBB 축 = invW 의 정규화된 '행'(row-major).
    _float3 axX = {iw._11 / lx, iw._12 / lx, iw._13 / lx};
    _float3 axY = {iw._21 / ly, iw._22 / ly, iw._23 / ly};
    _float3 axZ = {iw._31 / lz, iw._32 / lz, iw._33 / lz};

    _uint n = 0;
    for (CCollider* pCol : hits)
    {
        if (n >= MAX_COLLIDERS_PER_WALL) break;
        if (nullptr == pCol || pCol == inst.pSelfCollider) continue;   // 자기 벽 제외
        CBounding* pB = pCol->Get_Bounding();
        if (nullptr == pB) continue;

        _float3 wc, we;
        pB->Get_AABBBound(wc, we);                 // 월드 AABB(center, half-extents)

        GPU_COLLIDER& g = inst.pColliderMapped[n];
        XMStoreFloat3(&g.vCenter, XMVector3Transform(XMLoadFloat3(&wc), invW));
        g.vAxisX = axX; g.vAxisY = axY; g.vAxisZ = axZ;
        g.fExtX = we.x * lx; g.fExtY = we.y * ly; g.fExtZ = we.z * lz;
        g.fPad = 0.f;
        ++n;
    }
    inst.iColliderCount = n;

    char s[200];
    sprintf_s(s, "[Fracture] wall %u: packed colliders = %u / found %d  (worldC=%.1f,%.1f,%.1f, queryR=%.2f)\n",
        inst.iWallId, n, (int)hits.size(), worldCenter.x, worldCenter.y, worldCenter.z, queryR);
    OutputDebugStringA(s);
}

void CFracture_System::Unregister(CModel* pModel)
{
    _int s = Find_SlotByModel(pModel);
    if (s < 0) return;
    INSTANCE& inst = m_Instances[s];
    inst.bUsed = false; inst.bBroken = false; inst.bSeedDirty = false;
    inst.pModel = nullptr; inst.iChunkCount = 0; inst.fLife = 0.f;
}

// ----------------------------------------------------------------------------
//  Compute: 활성 슬롯 순회. bind 완료 슬롯은 skip(1회만), 파괴 슬롯만 매 프레임 적분.
// ----------------------------------------------------------------------------
void CFracture_System::Compute(ID3D12GraphicsCommandList* pCmd)
{
    if (nullptr == pCmd) return;

    for (_uint i = 0; i < MAX_FRACTURE_WALLS; ++i)
    {
        INSTANCE& inst = m_Instances[i];
        if (!inst.bUsed) continue;
        if (!inst.bSeedDirty && !inst.bBroken) continue;   // bind 상태 + 시드 완료 → 계산 불필요

        const _bool bFirst = inst.bSeedDirty;
        if (bFirst)
        {
            pCmd->CopyBufferRegion(inst.pChunkBuffer.Get(), 0, inst.pChunkUpload.Get(), 0,
                (UINT64)inst.iChunkCount * sizeof(CHUNK));
            inst.bSeedDirty = false;
        }

        Transition(pCmd, inst.pChunkBuffer.Get(),
            bFirst ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Transition(pCmd, inst.pMatrixBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        pCmd->SetComputeRootSignature(m_pComputeRS.Get());
        pCmd->SetPipelineState(m_pComputePSO.Get());

        COMPUTE_CB cb;
        cb.fDeltaTime = (m_fDeltaTime > 0.05f) ? 0.05f : m_fDeltaTime;
        cb.iChunkCount = inst.iChunkCount;
        cb.fFloorY = m_fFloorY; cb.fFriction = m_fFriction;
        // [축 보정] 월드 중력을 이 벽의 모델 공간으로 변환 → VS 의 월드행렬을 거쳐 실제 월드-다운으로 낙하.
        //   모델 공간 물리인데 중력을 월드축(0,-9.8,0) 그대로 주면 FBX 축/벽 회전에 따라 옆으로 날아간다.
        //   gModel = gWorld · inverse(matWorld 3x3)  →  gModel · matWorld3x3 == gWorld (월드-다운 정확).
        //   inverse 가 스케일도 흡수하므로 별도 모델반경 스케일 불필요(속도감은 m_vGravity 로 튜닝).
        XMMATRIX Wg = XMLoadFloat4x4(&inst.matWorld);
        Wg.r[0] = XMVectorSetW(Wg.r[0], 0.f);
        Wg.r[1] = XMVectorSetW(Wg.r[1], 0.f);
        Wg.r[2] = XMVectorSetW(Wg.r[2], 0.f);
        Wg.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);     // 회전+스케일만(이동 제거)
        XMMATRIX invWg = XMMatrixInverse(nullptr, Wg);
        XMFLOAT3 gm; XMStoreFloat3(&gm, XMVector3TransformNormal(XMLoadFloat3(&m_vGravity), invWg));
        cb.vGravity = {gm.x, gm.y, gm.z};
        cb.iColliderCount = inst.iColliderCount;   // [A] 셰이더에 콜라이더 수 전달
        pCmd->SetComputeRoot32BitConstants(0, 8, &cb, 0);
        pCmd->SetComputeRootUnorderedAccessView(1, inst.pChunkBuffer->GetGPUVirtualAddress());
        pCmd->SetComputeRootUnorderedAccessView(2, inst.pMatrixBuffer->GetGPUVirtualAddress());
        pCmd->SetComputeRootShaderResourceView(3, inst.pColliderBuffer->GetGPUVirtualAddress());  // [A] t0

        _uint groups = (inst.iChunkCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
        pCmd->Dispatch(groups, 1, 1);

        Transition(pCmd, inst.pMatrixBuffer.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Transition(pCmd, inst.pChunkBuffer.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);

        if (inst.bBroken)
        {
            inst.fLife += m_fDeltaTime;
            if (inst.fLife > m_fLifeMax)
            {
                // 소멸 → 슬롯 반환 (CMap 이 이미 SetDead 라면 모델은 그대로 두고 슬롯만 회수)
                inst.bUsed = false; inst.bBroken = false; inst.pModel = nullptr;
                inst.iChunkCount = 0; inst.fLife = 0.f;
            }
        }
    }
}

// ----------------------------------------------------------------------------
//  Render: 활성 슬롯 전체 드로우. FrameCB 링에 슬롯별 world 기록.
// ----------------------------------------------------------------------------
void CFracture_System::Render(ID3D12GraphicsCommandList* pCmd)
{
    if (nullptr == pCmd) return;

    _uint iFrame = (_uint)m_pGameInstance->GetCurrentFrameIndex();
    if (iFrame >= (_uint)FRAME_COUNT) iFrame = 0;

    _bool bPipelineSet = false;
    _uint iRing = 0;

    for (_uint i = 0; i < MAX_FRACTURE_WALLS; ++i)
    {
        INSTANCE& inst = m_Instances[i];
        if (!inst.bUsed || nullptr == inst.pModel) continue;

        if (!bPipelineSet)
        {
            pCmd->SetGraphicsRootSignature(m_pGraphicRS.Get());
            pCmd->SetPipelineState(m_pGraphicPSO.Get());
            bPipelineSet = true;
        }

        Update_FrameCB(iFrame, iRing, inst.matWorld);

        D3D12_GPU_VIRTUAL_ADDRESS cbAddr =
            m_pFrameCB[iFrame]->GetGPUVirtualAddress() + (UINT64)iRing * m_iFrameCBStride;
        pCmd->SetGraphicsRootConstantBufferView(0, cbAddr);
        pCmd->SetGraphicsRootDescriptorTable(1, m_pGameInstance->Get_GPUHandle(inst.iMatrixSrvIndex));

        CTexture* pTex = inst.pModel->Get_DiffuseTexture(inst.iMeshIndex);
        if (pTex)
            pTex->Bind_ShaderResource(pCmd, (RootParameterIndex)2);

        inst.pModel->Render_Raw(pCmd, inst.iMeshIndex);
        ++iRing;
    }
}

void CFracture_System::Update_FrameCB(_uint iFrame, _uint iRingSlot, const _float4x4& matWorld)
{
    FRAME_CB cb;
    cb.matWorld = matWorld;
    cb.matView = m_pGameInstance->Get_CurrentCameraView();
    cb.matProj = m_pGameInstance->Get_CurrentCameraProjection();
    cb.vLightDir = NormalizeSafe({-0.4f, -1.0f, -0.3f}, {0.f, -1.f, 0.f});
    cb.fAmbient = 0.35f;
    memcpy(m_pFrameCBMapped[iFrame] + (size_t)iRingSlot * m_iFrameCBStride, &cb, sizeof(FRAME_CB));
}

CFracture_System* CFracture_System::Create(EngineContext* pContext)
{
    CFracture_System* pInstance = new CFracture_System(pContext);
    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : CFracture_System");
        Safe_Release(pInstance);
    }
    return pInstance;
}

void CFracture_System::Free()
{
    for (_int i = 0; i < FRAME_COUNT; ++i)
    {
        if (m_pFrameCB[i] && m_pFrameCBMapped[i]) { m_pFrameCB[i]->Unmap(0, nullptr); m_pFrameCBMapped[i] = nullptr; }
        m_pFrameCB[i].Reset();
    }
    for (_uint i = 0; i < MAX_FRACTURE_WALLS; ++i)
    {
        INSTANCE& inst = m_Instances[i];
        if (inst.pChunkUpload && inst.pUploadMapped) { inst.pChunkUpload->Unmap(0, nullptr); inst.pUploadMapped = nullptr; }
        inst.pChunkUpload.Reset();
        inst.pChunkBuffer.Reset();
        inst.pMatrixBuffer.Reset();
        inst.pModel = nullptr;
    }
    m_pComputePSO.Reset();  m_pComputeRS.Reset();
    m_pGraphicPSO.Reset();  m_pGraphicRS.Reset();
    m_pDevice = nullptr;

    __super::Free();
}