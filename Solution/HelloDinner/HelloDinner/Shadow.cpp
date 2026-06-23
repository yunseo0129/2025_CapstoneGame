#include "Shadow.h"
#include "Camera.h"
#include "Light.h"
#include "GameInstance.h"

CShadow::CShadow(EngineContext* pContext, _uint width, _uint height)
    : m_pContext {pContext}
    , m_pGameInstance {CGameInstance::GetInstance()}
{
    mSceneBounds.Center = XMFLOAT3(0.f, 0.f, 0.f);
    mSceneBounds.Radius = 1.f;

    m_Viewport.TopLeftX = 0.0f;
    m_Viewport.TopLeftY = 0.0f;
    m_Viewport.Width = static_cast<float>(width);
    m_Viewport.Height = static_cast<float>(height);
    m_Viewport.MinDepth = 0.0f;
    m_Viewport.MaxDepth = 1.0f;

    m_iHeight = height;
    m_iWidth = width;

    m_ScissorRect = {0, 0, (LONG)width, (LONG)height};
}

HRESULT CShadow::Initialize()
{
    Create_Resource();
    Create_ShadowBuffer();
    return S_OK;
}

void CShadow::Update(CCamera* _camera, CLight* _light)
{
    UpdateBoundingSphere(_camera);

    UpdateMatrix(_light);
}

void CShadow::Bind_ShadowBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex)
{
    _int iFrameIndex = m_pGameInstance->GetCurrentFrameIndex();

    memcpy(&m_pCbMappedShadow[iFrameIndex]->m_xmf4x4View, &m_LightView, sizeof(_float4x4));
    memcpy(&m_pCbMappedShadow[iFrameIndex]->m_xmf4x4Proj, &m_LightProj, sizeof(_float4x4));
    memcpy(&m_pCbMappedShadow[iFrameIndex]->m_xmf3Position, &m_LightPos, sizeof(_float3));

    pCmdList->SetGraphicsRootConstantBufferView(_eIndex, m_pShadowbuffer[iFrameIndex]->GetGPUVirtualAddress());
}

void CShadow::Bind_ShadowMap(ID3D12GraphicsCommandList* cmdList, RootParameterIndex _eIndex)
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrvHandle = m_pGameInstance->Get_GPUHandle(m_iSRVIndex);
    cmdList->SetGraphicsRootDescriptorTable(_eIndex, hGpuSrvHandle);
}

void CShadow::Begin_Pass(ID3D12GraphicsCommandList* cmdList)
{
    cmdList->RSSetViewports(1, &m_Viewport);
    cmdList->RSSetScissorRects(1, &m_ScissorRect);

    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_pShadowMap.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);
    cmdList->ResourceBarrier(1, &barrier);

    cmdList->ClearDepthStencilView(m_hCpuDsvHandle,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
    cmdList->OMSetRenderTargets(0, nullptr, FALSE, &m_hCpuDsvHandle);

    Bind_ShadowBuffer(cmdList, RootParameterIndex::Camera);
}

void CShadow::End_Pass(ID3D12GraphicsCommandList* cmdList)
{
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_pShadowMap.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &barrier);
}

_bool CShadow::IsSphereInBounds(const _float3& vCenter, _float fRadius) const
{
    BoundingSphere sphere(vCenter, fRadius);
    return mSceneBounds.Intersects(sphere);
}

void CShadow::UpdateMatrix(CLight* _light)
{
    // 태양만 구현 후에 다른 광원도 구현할 예정
    XMVECTOR vLightDir = XMVector3Normalize(_light->Get_Direction());
    XMVECTOR vtarget = XMLoadFloat3(&mSceneBounds.Center);
    XMVECTOR vLightPos = vtarget - (vLightDir * mSceneBounds.Radius);
    XMVECTOR vUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
    // 빛 방향도 수직이면 up 벡터를 z축으로 변경
    if (abs(XMVectorGetY(vLightDir)) > 0.99f)
    {
        vUp = XMVectorSet(0.f, 0.f, 1.f, 0.f);
    }
    XMMATRIX lightView = XMMatrixLookAtLH(vLightPos, vtarget, vUp);

    float r = mSceneBounds.Radius;
    // [가장자리 팝 방지] near 를 라이트 쪽으로 당겨, 경계구 밖(화면 위/뒤)의
    //  캐스터도 깊이를 기록하게 한다. near=0 이면 그들이 클리핑되어 그림자가 튄다.
    float nearZ = -r;
    float farZ = 2.f * r;

    XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(-r, r, -r, r, nearZ, farZ);

    XMMATRIX viewProj = lightView * lightProj;

    // 원점(0,0,0,1)을 shadow clip space로 변환
    XMVECTOR shadowOrigin = XMVectorSet(0.f, 0.f, 0.f, 1.f);
    shadowOrigin = XMVector4Transform(shadowOrigin, viewProj);

    // [-1,1] NDC → [0, m_iWidth] 텍셀 좌표로 변환
    float halfShadowMapSize = m_iWidth * 0.5f;
    shadowOrigin = XMVectorScale(shadowOrigin, halfShadowMapSize);

    // 정수 텍셀 위치로 반올림한 뒤, 그 차이를 보정값으로
    XMVECTOR rounded = XMVectorRound(shadowOrigin);
    XMVECTOR roundOffset = XMVectorSubtract(rounded, shadowOrigin);
    roundOffset = XMVectorScale(roundOffset, 1.f / halfShadowMapSize);

    // proj 행렬의 translation 성분에 offset 가산 (z, w는 0)
    XMFLOAT4X4 projF;
    XMStoreFloat4x4(&projF, lightProj);
    projF._41 += XMVectorGetX(roundOffset);
    projF._42 += XMVectorGetY(roundOffset);
    lightProj = XMLoadFloat4x4(&projF);

    XMStoreFloat3(&m_LightPos, vLightPos);

    XMMATRIX T(
        0.5f, 0.f, 0.f, 0.f,
        0.f, -0.5f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.5f, 0.5f, 0.f, 1.f);

    XMMATRIX S = lightView * lightProj * T;
    XMStoreFloat4x4(&m_LightView, lightView);
    XMStoreFloat4x4(&m_LightProj, lightProj);
    XMStoreFloat4x4(&m_ShadowTransform, S);

    m_Lightnear = nearZ;
    m_Lightfar = farZ;
}

void CShadow::UpdateBoundingSphere(CCamera* _camera)
{
    // 현재 시야를 포함하는 구체의 중심과 반지름을 계산하여 mSceneBounds에 저장
    XMFLOAT4X4 cameraView = _camera->Get_CameraView();
    XMFLOAT4X4 cameraProj = _camera->Get_CameraProjection();
    XMMATRIX matView = XMLoadFloat4x4(&cameraView);
    XMMATRIX matProj = XMLoadFloat4x4(&cameraProj);
    XMMATRIX matViewProj = XMMatrixMultiply(matView, matProj);
    XMVECTOR det;
    XMMATRIX matInvViewProj = XMMatrixInverse(&det, matViewProj);

    XMVECTOR ndcCorners[8] =
    {
        XMVectorSet(-1.f, 1.f, 0.f, 1.f),	// 가까운 평면 왼쪽 위
        XMVectorSet(1.f, 1.f, 0.f, 1.f),	// 가까운 평면 오른쪽 위
        XMVectorSet(1.f, -1.f, 0.f, 1.f),	// 가까운 평면 오른쪽 아래
        XMVectorSet(-1.f, -1.f, 0.f, 1.f),	// 가까운 평면 왼쪽 아래
        XMVectorSet(-1.f, 1.f, 1.f, 1.f),	// 먼 평면 왼쪽 위
        XMVectorSet(1.f, 1.f, 1.f, 1.f),	// 먼 평면 오른쪽 위
        XMVectorSet(1.f, -1.f, 1.f, 1.f),	// 먼 평면 오른쪽 아래
        XMVectorSet(-1.f, -1.f, 1.f, 1.f)	// 먼 평면 왼쪽 아래
    };

    XMVECTOR worldCorners[8];
    for (int i = 0; i < 8; ++i)
    {
        worldCorners[i] = XMVector4Transform(ndcCorners[i], matInvViewProj);
        worldCorners[i] = XMVectorDivide(worldCorners[i], XMVectorReplicate(XMVectorGetW(worldCorners[i])));
    }

    XMVECTOR sphereCenter = XMVectorZero();
    for (int i = 0; i < 8; ++i)
    {
        sphereCenter = XMVectorAdd(sphereCenter, worldCorners[i]);
    }
    sphereCenter = XMVectorScale(sphereCenter, 1.0f / 8.0f);

    XMVECTOR vDiag1 = XMVector3Length(XMVectorSubtract(worldCorners[0], worldCorners[6]));
    XMVECTOR vDiag2 = XMVector3Length(XMVectorSubtract(worldCorners[4], worldCorners[6]));
    float radius = max(XMVectorGetX(vDiag1), XMVectorGetX(vDiag2)) * 0.5f;

    // [안정화] 반경을 고정 스텝으로 올림 → 프레임 간 동일한 값 보장.
    //  부동소수 지터/FOV 변화로 반경이 흔들리면 텍셀 스케일이 변해 스내핑이 그리드를
    //  고정하지 못하고 그림자가 일렁인다. 양자화로 텍셀 크기를 프레임 간 불변으로 유지.
    const float fRadiusStep = 1.0f;   // 월드 단위(씬 스케일에 맞게 조정)
    radius = ceilf(radius / fRadiusStep) * fRadiusStep;


    mSceneBounds.Center = XMFLOAT3(XMVectorGetX(sphereCenter), XMVectorGetY(sphereCenter), XMVectorGetZ(sphereCenter));
    mSceneBounds.Radius = radius;
}

HRESULT CShadow::Create_ShadowBuffer()
{
    _uint ncbElementBytes = ((sizeof(CB_VS_CAMERA) + 255) & ~255);

    D3D12_HEAP_PROPERTIES d3dHeapPropertiesDesc;
    ::ZeroMemory(&d3dHeapPropertiesDesc, sizeof(D3D12_HEAP_PROPERTIES));
    d3dHeapPropertiesDesc.Type = D3D12_HEAP_TYPE_UPLOAD;
    d3dHeapPropertiesDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    d3dHeapPropertiesDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    d3dHeapPropertiesDesc.CreationNodeMask = 1;
    d3dHeapPropertiesDesc.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC d3dResourceDesc;
    ::ZeroMemory(&d3dResourceDesc, sizeof(D3D12_RESOURCE_DESC));
    d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d3dResourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    d3dResourceDesc.Width = ncbElementBytes;
    d3dResourceDesc.Height = 1;
    d3dResourceDesc.DepthOrArraySize = 1;
    d3dResourceDesc.MipLevels = 1;
    d3dResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    d3dResourceDesc.SampleDesc.Count = 1;
    d3dResourceDesc.SampleDesc.Quality = 0;
    d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    for (_int i = 0; i < FRAME_COUNT; ++i)
    {
        HRESULT hResult = m_pContext->device->CreateCommittedResource(
            &d3dHeapPropertiesDesc, D3D12_HEAP_FLAG_NONE,
            &d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL, __uuidof(ID3D12Resource), (void**)&m_pShadowbuffer[i]);

        if (FAILED(hResult))
            return E_FAIL;

        m_pShadowbuffer[i]->Map(0, NULL, (void**)&m_pCbMappedShadow[i]);
    }

    return S_OK;
}

void CShadow::Create_Resource()
{
    D3D12_RESOURCE_DESC resourceDesc;
    ZeroMemory(&resourceDesc, sizeof(D3D12_RESOURCE_DESC));
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = m_iWidth;
    resourceDesc.Height = m_iHeight;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.f;
    optClear.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    ThrowIfFailed(m_pContext->device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(&m_pShadowMap)));

    // DSV Heap 생성 -> 후에 Light와 Shadow가 많아질 경우 TextureManager처럼 관리하는 방법 추가 예정
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_pContext->device->CreateDescriptorHeap(
        &dsvHeapDesc, IID_PPV_ARGS(&m_pDsvHeap)));
    m_hCpuDsvHandle = m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();

    // DSV 생성
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    m_pContext->device->CreateDepthStencilView(m_pShadowMap.Get(), &dsvDesc, m_hCpuDsvHandle);

    // SRV 생성
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvcpuHandle = m_pGameInstance->Get_CPUHandle();
    m_iSRVIndex = m_pGameInstance->Get_CurrentIndex();
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = m_pGameInstance->Get_GPUHandle(m_iSRVIndex);
    m_pGameInstance->Offset_DescriptorHandle(1);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    m_pContext->device->CreateShaderResourceView(m_pShadowMap.Get(), &srvDesc, srvcpuHandle);
}

CShadow* CShadow::Create(EngineContext* pContext, _uint width, _uint height)
{
    CShadow* pInstance = new CShadow(pContext, width, height);

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : CShadow");
        Safe_Release(pInstance);
    }

    return pInstance;
}


void CShadow::Free()
{
    __super::Free();

}