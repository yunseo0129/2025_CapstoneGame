#include "Camera.h"
#include "GameInstance.h"

CCamera::CCamera(EngineContext* pContext)
	: CGameObject{ pContext }
{
	XMStoreFloat4x4 ( &m_xmf4x4View , XMMatrixIdentity () );
	XMStoreFloat4x4 ( &m_xmf4x4Projection , XMMatrixIdentity () );
	m_xmf3Position = XMFLOAT3 ( 0.0f , 0.0f , 0.0f );
}

CCamera::CCamera(const CCamera& Prototype)
	: CGameObject{ Prototype.m_pContext }
{
	m_xmf3Position = Prototype.m_xmf3Position;
	XMStoreFloat4x4(&m_xmf4x4View, XMLoadFloat4x4(&Prototype.m_xmf4x4View));
	XMStoreFloat4x4(&m_xmf4x4Projection, XMLoadFloat4x4(&Prototype.m_xmf4x4Projection));
}

HRESULT CCamera::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CCamera::Initialize(void* pArg)
{
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	CAMERA_DESC* pDesc = static_cast<CAMERA_DESC*>(pArg);

	m_pTransformCom->Set_State(CTransform::STATE_POSITION, XMVectorSetW(XMLoadFloat3(&pDesc->vEye), 1.f));
	m_pTransformCom->LookAt(XMVectorSetW(XMLoadFloat3(&pDesc->vAt), 1.f));

	m_fFovy = pDesc->fFovy;
	m_fAspect = pDesc->fAspect;
	m_fNear = pDesc->fNear;
	m_fFar = pDesc->fFar;

	Create_CameraBuffer ();

	return S_OK;
}

void CCamera::Priority_Update(_float fTimeDelta)
{

}

void CCamera::Update(_float fTimeDelta)
{

}

void CCamera::Late_Update(_float fTimeDelta)
{
    Update_ViewProjection();
}

HRESULT CCamera::Render()
{
	return S_OK;
}

void CCamera::Update_ViewProjection()
{
    m_bFrustumValid = false;

    XMStoreFloat4x4(&m_xmf4x4View, m_pTransformCom->Get_WorldMatrix_Inverse());
    XMStoreFloat4x4(&m_xmf4x4Projection, XMMatrixPerspectiveFovLH(m_fFovy, m_fAspect, m_fNear, m_fFar));
    XMStoreFloat3(&m_xmf3Position, m_pTransformCom->Get_State(CTransform::STATE_POSITION));

    // [Frustum Culling]
    XMMATRIX matView = XMLoadFloat4x4(&m_xmf4x4View);
    XMMATRIX matProj = XMLoadFloat4x4(&m_xmf4x4Projection);

    XMVECTOR det;
    XMMATRIX matViewInv = XMMatrixInverse(&det, matView);
    if (fabsf(XMVectorGetX(det)) < 1e-6f)
        return;  // singular (첫 프레임 등) — 안전망

    // projection 공간 frustum 생성 → view inverse로 world 공간으로 변환
    BoundingFrustum::CreateFromMatrix(m_FrustumWorld, matProj);
    m_FrustumWorld.Transform(m_FrustumWorld, matViewInv);

    m_bFrustumValid = true;
}

_bool CCamera::IsSphereInFrustum(const _float3& vCenter, _float fRadius) const
{
    if (!m_bFrustumValid)
        return true;  // 안전망: 첫 프레임 등

    BoundingSphere sphere(vCenter, fRadius);
    return m_FrustumWorld.Intersects(sphere);
}

HRESULT CCamera::Bind_CameraBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex)
{
    _int iFrameIndex = m_pGameInstance->GetCurrentFrameIndex();

    Update_ViewProjection();

    memcpy(&m_pCbMappedCameras[iFrameIndex]->m_xmf4x4View, &m_xmf4x4View, sizeof(_float4x4));
    memcpy(&m_pCbMappedCameras[iFrameIndex]->m_xmf4x4Proj, &m_xmf4x4Projection, sizeof(_float4x4));
    memcpy(&m_pCbMappedCameras[iFrameIndex]->m_xmf3Position, &m_xmf3Position, sizeof(_float3));

    pCmdList->SetGraphicsRootConstantBufferView(_eIndex, m_pCameraBuffers[iFrameIndex]->GetGPUVirtualAddress());

    return S_OK;
}

void CCamera::DebugPrintMatrix ( const char* name , const XMFLOAT4X4& m )
{
	char buf[256];
	sprintf_s ( buf , "%s:\n[%f %f %f %f]\n[%f %f %f %f]\n[%f %f %f %f]\n[%f %f %f %f]\n" ,
		name ,
		m._11 , m._12 , m._13 , m._14 ,
		m._21 , m._22 , m._23 , m._24 ,
		m._31 , m._32 , m._33 , m._34 ,
		m._41 , m._42 , m._43 , m._44 );
	OutputDebugStringA ( buf );
}

void CCamera::DebugPrintFloat3 ( const char* name , const XMFLOAT3& v )
{
	char buf[128];
	sprintf_s ( buf , "%s: (%f, %f, %f)\n" , name , v.x , v.y , v.z );
	OutputDebugStringA ( buf );
}

HRESULT CCamera::Create_CameraBuffer ()
{
	_uint ncbElementBytes = ( ( sizeof ( CB_VS_CAMERA ) + 255 ) & ~255 );

	D3D12_HEAP_PROPERTIES d3dHeapPropertiesDesc;
	::ZeroMemory ( &d3dHeapPropertiesDesc , sizeof ( D3D12_HEAP_PROPERTIES ) );
	d3dHeapPropertiesDesc.Type = D3D12_HEAP_TYPE_UPLOAD;
	d3dHeapPropertiesDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapPropertiesDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapPropertiesDesc.CreationNodeMask = 1;
	d3dHeapPropertiesDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC d3dResourceDesc;
	::ZeroMemory ( &d3dResourceDesc , sizeof ( D3D12_RESOURCE_DESC ) );
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

	for ( _int i = 0; i < FRAME_COUNT; ++i )
	{
		HRESULT hResult = m_pContext->device->CreateCommittedResource (
			&d3dHeapPropertiesDesc , D3D12_HEAP_FLAG_NONE ,
			&d3dResourceDesc , D3D12_RESOURCE_STATE_GENERIC_READ ,
			NULL , __uuidof( ID3D12Resource ) , ( void** )&m_pCameraBuffers[i] );

		if ( FAILED ( hResult ) )
			return E_FAIL;

		m_pCameraBuffers[i]->Map ( 0 , NULL , ( void** )&m_pCbMappedCameras[i] );
	}

	return S_OK;
}

CGameObject* CCamera::Clone (void* Arg) 
{

	return nullptr;
}

void CCamera::Free()
{
	for ( _int i = 0; i < FRAME_COUNT; ++i )
	{
		if (m_pCbMappedCameras[i])
		{
			m_pCbMappedCameras[i] = nullptr;
		}
	}
	__super::Free();
}
