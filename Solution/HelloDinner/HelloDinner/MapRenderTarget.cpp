#include "MapRenderTarget.h"
#include "GameInstance.h"
#include "Map.h"
#include "Model.h"
#include <list>

CMapRenderTarget::CMapRenderTarget(EngineContext* _pContext, _uint _iWidth, _uint _iHeight)
    : m_pContext {_pContext}
    , m_pGameInstance {CGameInstance::GetInstance()}
    , m_iWidth {_iWidth}
    , m_iHeight {_iHeight}
{
    m_Viewport.TopLeftX = 0.f;
    m_Viewport.TopLeftY = 0.f;
    m_Viewport.Width = static_cast<float>(_iWidth);
    m_Viewport.Height = static_cast<float>(_iHeight);
    m_Viewport.MinDepth = 0.f;
    m_Viewport.MaxDepth = 1.f;

    m_ScissorRect = {0, 0, (LONG)_iWidth, (LONG)_iHeight};

    XMStoreFloat4x4(&m_matView, XMMatrixIdentity());
    XMStoreFloat4x4(&m_matProj, XMMatrixIdentity());
}

HRESULT CMapRenderTarget::Initialize()
{
    Create_ColorTarget();
    Create_DepthTarget();
    if (FAILED(Create_CameraBuffer()))   return E_FAIL;
    if (FAILED(Create_InstanceBuffer())) return E_FAIL;
    return S_OK;
}

// =====================================================================
//  탑다운 카메라 갱신
//   eye = 중심 위 높은 곳, look = 수직 아래(-Y), up = 수평 방향(피치 무시)
//   직교 폭 = halfExtent*2
// =====================================================================
void CMapRenderTarget::Set_View(const _float3& vCenterXZ, _float fHalfExtent, const _float3& vUpDirXZ)
{
    if (fHalfExtent < 0.001f) fHalfExtent = 0.001f;

    // 맵이 ×100 스케일이라 좌표가 수백~수천에 분포한다. 카메라를 충분히 높이
    // 올리고 near~far 를 아주 넓게 잡아 어떤 높이의 맵도 절두체 안에 들어오게 한다.
    const _float fHeight = 5000.f;   // 카메라 높이 (맵 최고점보다 충분히 위)
    const _float fNear = 1.f;
    const _float fFar = 12000.f;     // near~far 안에 맵 전체 높이 포함

    XMVECTOR vEye = XMVectorSet(vCenterXZ.x, vCenterXZ.y + fHeight, vCenterXZ.z, 1.f);
    XMVECTOR vAt = XMVectorSet(vCenterXZ.x, vCenterXZ.y, vCenterXZ.z, 1.f);

    // up 은 수평 방향(화면 위쪽으로 갈 월드 방향). y 성분 제거 후 정규화.
    XMVECTOR vUp = XMVectorSet(vUpDirXZ.x, 0.f, vUpDirXZ.z, 0.f);
    if (XMVectorGetX(XMVector3LengthSq(vUp)) < 1e-6f)
        vUp = XMVectorSet(0.f, 0.f, 1.f, 0.f); // 안전값: 북쪽
    vUp = XMVector3Normalize(vUp);

    XMMATRIX matView = XMMatrixLookAtLH(vEye, vAt, vUp);
    XMMATRIX matProj = XMMatrixOrthographicLH(
        fHalfExtent * 2.f, fHalfExtent * 2.f, fNear, fFar);

    XMStoreFloat4x4(&m_matView, matView);
    XMStoreFloat4x4(&m_matProj, matProj);
    XMStoreFloat3(&m_vEyePos, vEye);
}

// =====================================================================
//  Begin / End Pass
// =====================================================================
void CMapRenderTarget::Begin_Pass(ID3D12GraphicsCommandList* _cmdList)
{
    _cmdList->RSSetViewports(1, &m_Viewport);
    _cmdList->RSSetScissorRects(1, &m_ScissorRect);

    // 컬러 타겟: READ -> RENDER_TARGET
    if (m_eColorState != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_pColorTex.Get(), m_eColorState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        _cmdList->ResourceBarrier(1, &barrier);
        m_eColorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    const FLOAT clear[4] = {m_vClearColor.x, m_vClearColor.y, m_vClearColor.z, m_vClearColor.w};
    _cmdList->ClearRenderTargetView(m_hCpuRtvHandle, clear, 0, nullptr);
    _cmdList->ClearDepthStencilView(m_hCpuDsvHandle,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);

    _cmdList->OMSetRenderTargets(1, &m_hCpuRtvHandle, FALSE, &m_hCpuDsvHandle);

    // 탑다운 카메라를 b0(Camera) 에 바인딩
    Bind_CameraBuffer(_cmdList);
}

void CMapRenderTarget::End_Pass(ID3D12GraphicsCommandList* _cmdList)
{
    // 컬러 타겟: RENDER_TARGET -> GENERIC_READ (SRV 샘플링용)
    if (m_eColorState != D3D12_RESOURCE_STATE_GENERIC_READ)
    {
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_pColorTex.Get(), m_eColorState, D3D12_RESOURCE_STATE_GENERIC_READ);
        _cmdList->ResourceBarrier(1, &barrier);
        m_eColorState = D3D12_RESOURCE_STATE_GENERIC_READ;
    }
}

// =====================================================================
//  Layer_Map 인스턴싱 렌더 (CRenderer::Render_InstancedQueue 의 축약판)
// =====================================================================
HRESULT CMapRenderTarget::Render_MapLayer(ID3D12GraphicsCommandList* _cmdList, _uint _iLevelIndex, const _wstring& _strLayerTag)
{
    _int frameIdx = m_pGameInstance->GetCurrentFrameIndex();
    if (frameIdx < 0 || frameIdx >= FRAME_COUNT) frameIdx = 0;

    list<CGameObject*> MapObjs = m_pGameInstance->Get_List(_iLevelIndex, _strLayerTag);
    if (MapObjs.empty())
        return S_OK;

    // 모델 태그별로 묶는다 (같은 모델끼리 한 번에 인스턴싱)
    map<_wstring, vector<CMap*>> groups;
    for (auto* pObj : MapObjs)
    {
        CMap* pMap = dynamic_cast<CMap*>(pObj);
        if (!pMap) continue;
        if (pMap->IsDead()) continue;        // [추가] 죽은 오브젝트 제외
        if (pMap->Is_Broken()) continue;     // [추가] fracture 파괴된 벽은 미니맵에 안 그림
        if (!pMap->Get_Model()) continue;
        groups[pMap->Get_ModelTag()].push_back(pMap);
    }
    if (groups.empty())
        return S_OK;

    m_pGameInstance->Set_PipelineState(_cmdList, PSO_TYPE::MAPRT_INSTANCED);

    // 인스턴스 버퍼는 프레임당 하나뿐 -> 그룹마다 채워서 즉시 그린다.
    // (한 프레임에 그룹들이 같은 업로드 버퍼를 순차적으로 덮어쓰지만,
    //  각 그룹의 DrawIndexedInstanced 가 기록된 시점의 데이터를 쓰도록
    //  그룹별로 buffer offset 을 나눠 사용한다.)
    XMFLOAT4X4* pMapped = m_pInstMapped[frameIdx];
    const D3D12_GPU_VIRTUAL_ADDRESS baseGPU = m_pInstanceBuffer[frameIdx]->GetGPUVirtualAddress();

    _uint iWriteCursor = 0; // 이번 프레임에 버퍼에 쓴 인스턴스 누적 개수

    for (auto& kv : groups)
    {
        vector<CMap*>& objs = kv.second;
        if (objs.empty()) continue;

        CModel* pModel = objs[0]->Get_Model();
        if (!pModel) continue;

        // 남은 공간이 없으면 중단
        if (iWriteCursor >= MAX_MAP_INSTANCES) break;

        const _uint iGroupStart = iWriteCursor;
        _uint instanceCount = 0;
        for (CMap* pMap : objs)
        {
            if (iWriteCursor >= MAX_MAP_INSTANCES) break;
            pMapped[iWriteCursor] = pMap->Get_CachedWorldMatrix();
            ++iWriteCursor;
            ++instanceCount;
        }
        if (instanceCount == 0) continue;

        // 이 그룹 전용 VBV (base + start offset)
        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = baseGPU + (D3D12_GPU_VIRTUAL_ADDRESS)iGroupStart * sizeof(XMFLOAT4X4);
        vbv.StrideInBytes = sizeof(XMFLOAT4X4);
        vbv.SizeInBytes = instanceCount * sizeof(XMFLOAT4X4);

        _uint iNumMeshes = pModel->Get_NumMeshes();
        for (_uint m = 0; m < iNumMeshes; ++m)
            pModel->Render_Instanced(_cmdList, m, instanceCount, vbv, false);
    }

    return S_OK;
}

// =====================================================================
//  카메라 CB 바인딩
// =====================================================================
void CMapRenderTarget::Bind_CameraBuffer(ID3D12GraphicsCommandList* _cmdList)
{
    _int frameIdx = m_pGameInstance->GetCurrentFrameIndex();
    if (frameIdx < 0 || frameIdx >= FRAME_COUNT) frameIdx = 0;

    memcpy(&m_pCbMappedCamera[frameIdx]->m_xmf4x4View, &m_matView, sizeof(_float4x4));
    memcpy(&m_pCbMappedCamera[frameIdx]->m_xmf4x4Proj, &m_matProj, sizeof(_float4x4));
    memcpy(&m_pCbMappedCamera[frameIdx]->m_xmf3Position, &m_vEyePos, sizeof(_float3));

    _cmdList->SetGraphicsRootConstantBufferView(
        RootParameterIndex::Camera, m_pCameraBuffer[frameIdx]->GetGPUVirtualAddress());
}

// =====================================================================
//  컬러 타겟 생성 (RT + RTV + SRV)
// =====================================================================
void CMapRenderTarget::Create_ColorTarget()
{
    const DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = m_iWidth;
    desc.Height = m_iHeight;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = fmt;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE optClear = {};
    optClear.Format = fmt;
    optClear.Color[0] = m_vClearColor.x;
    optClear.Color[1] = m_vClearColor.y;
    optClear.Color[2] = m_vClearColor.z;
    optClear.Color[3] = m_vClearColor.w;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_pContext->device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&m_pColorTex)));
    m_eColorState = D3D12_RESOURCE_STATE_GENERIC_READ;

    // RTV Heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_pContext->device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_pRtvHeap)));
    m_hCpuRtvHandle = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = fmt;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    m_pContext->device->CreateRenderTargetView(m_pColorTex.Get(), &rtvDesc, m_hCpuRtvHandle);

    // SRV (글로벌 SRV 힙에서 한 칸 할당) — 셰도우와 동일 패턴
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = m_pGameInstance->Get_CPUHandle();
    m_iColorSRVIndex = m_pGameInstance->Get_CurrentIndex();
    m_pGameInstance->Offset_DescriptorHandle(1);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = fmt;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    m_pContext->device->CreateShaderResourceView(m_pColorTex.Get(), &srvDesc, srvCpuHandle);
}

// =====================================================================
//  깊이 타겟 생성 (DSV)
// =====================================================================
void CMapRenderTarget::Create_DepthTarget()
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = m_iWidth;
    desc.Height = m_iHeight;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear = {};
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.f;
    optClear.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_pContext->device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &optClear, IID_PPV_ARGS(&m_pDepthTex)));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_pContext->device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_pDsvHeap)));
    m_hCpuDsvHandle = m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    m_pContext->device->CreateDepthStencilView(m_pDepthTex.Get(), &dsvDesc, m_hCpuDsvHandle);
}

// =====================================================================
//  카메라 CB 생성 (프레임별 업로드 버퍼)
// =====================================================================
HRESULT CMapRenderTarget::Create_CameraBuffer()
{
    _uint ncbElementBytes = ((sizeof(CB_VS_CAMERA) + 255) & ~255);

    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    desc.Width = ncbElementBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    for (_int i = 0; i < FRAME_COUNT; ++i)
    {
        if (FAILED(m_pContext->device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&m_pCameraBuffer[i]))))
            return E_FAIL;

        m_pCameraBuffer[i]->Map(0, nullptr, (void**)&m_pCbMappedCamera[i]);
    }
    return S_OK;
}

// =====================================================================
//  인스턴스 버퍼 생성 (프레임별 업로드 버퍼, 월드행렬)
// =====================================================================
HRESULT CMapRenderTarget::Create_InstanceBuffer()
{
    const _uint bufferSize = sizeof(XMFLOAT4X4) * MAX_MAP_INSTANCES;

    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bufferSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    for (_int i = 0; i < FRAME_COUNT; ++i)
    {
        if (FAILED(m_pContext->device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&m_pInstanceBuffer[i]))))
            return E_FAIL;

        m_pInstanceBuffer[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_pInstMapped[i]));

        m_InstanceVBV[i].BufferLocation = m_pInstanceBuffer[i]->GetGPUVirtualAddress();
        m_InstanceVBV[i].StrideInBytes = sizeof(XMFLOAT4X4);
        m_InstanceVBV[i].SizeInBytes = bufferSize;
    }
    return S_OK;
}

CMapRenderTarget* CMapRenderTarget::Create(EngineContext* _pContext, _uint _iWidth, _uint _iHeight, const _float4& _vClearColor)
{
    CMapRenderTarget* pInstance = new CMapRenderTarget(_pContext, _iWidth, _iHeight);
    pInstance->m_vClearColor = _vClearColor;  // 리소스 생성 전에 클리어값 확정 (optClear 일치)
    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : CMapRenderTarget");
        Safe_Release(pInstance);
    }
    return pInstance;
}

void CMapRenderTarget::Free()
{
    for (_int i = 0; i < FRAME_COUNT; ++i)
    {
        if (m_pCameraBuffer[i] && m_pCbMappedCamera[i])
        {
            m_pCameraBuffer[i]->Unmap(0, nullptr);
            m_pCbMappedCamera[i] = nullptr;
        }
        if (m_pInstanceBuffer[i] && m_pInstMapped[i])
        {
            m_pInstanceBuffer[i]->Unmap(0, nullptr);
            m_pInstMapped[i] = nullptr;
        }
    }
    __super::Free();
}