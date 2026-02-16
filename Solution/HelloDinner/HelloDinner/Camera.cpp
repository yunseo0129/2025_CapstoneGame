#include "Camera.h"
#include "GameInstance.h"

CCamera::CCamera(EngineContext* pContext)
	: CGameObject{ pContext }
{

}

CCamera::CCamera(const CCamera& Prototype)
	: CGameObject{ Prototype }
{

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

	return S_OK;
}

void CCamera::Priority_Update(_float fTimeDelta)
{

}

void CCamera::Update(_float fTimeDelta)
{
	Compute_PipeLineMatrices();

}

void CCamera::Late_Update(_float fTimeDelta)
{

}

HRESULT CCamera::Render()
{

	return S_OK;
}

void CCamera::Compute_PipeLineMatrices()
{
	_float4 vCamPosition;
	XMStoreFloat4(&vCamPosition, m_pTransformCom->Get_State(CTransform::STATE_POSITION));
	m_pGameInstance->Set_CamPosition(vCamPosition);
	m_pGameInstance->Set_Transform(CPipeLine::D3DTS_VIEW, m_pTransformCom->Get_WorldMatrix_Inverse());
	m_pGameInstance->Set_Transform(CPipeLine::D3DTS_PROJ, XMMatrixPerspectiveFovLH(m_fFovy, m_fAspect, m_fNear, m_fFar));
}

CGameObject* CCamera::Clone(void* pArg)
{
	return new CCamera{ *this };
}

void CCamera::Free()
{
	__super::Free();

}
