#include "Player_1rd.h"
#include "GameInstance.h"


CPlayer_1rd::CPlayer_1rd(EngineContext* _pcontext)
	: CContainerObj{ _pcontext }
{
}

CPlayer_1rd::CPlayer_1rd(const CPlayer_1rd& Prototype)
	: CContainerObj(Prototype)
{
}

HRESULT CPlayer_1rd::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CPlayer_1rd::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return E_FAIL;

	Player_1RD_DESC* pDesc = static_cast<Player_1RD_DESC*>(pArg);
	m_strModelTag = pDesc->strModelTag;
	m_iModelLevelIndex = pDesc->iModelLevelIndex;

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	m_pTransformCom->Set_State(CTransform::STATE_POSITION, pDesc->vPos);

	return S_OK;
}

void CPlayer_1rd::Priority_Update(_float fTimeDelta)
{
	__super::Priority_Update(fTimeDelta);
}

void CPlayer_1rd::Update(_float fTimeDelta)
{
	__super::Update(fTimeDelta);
}

void CPlayer_1rd::Late_Update(_float fTimeDelta)
{
	__super::Late_Update(fTimeDelta);
}

void CPlayer_1rd::Render(ID3D12GraphicsCommandList* _commandList)
{

}

HRESULT CPlayer_1rd::Ready_Components()
{
	return S_OK;
}

HRESULT CPlayer_1rd::Bind_ShaderResources()
{
	/*if (FAILED(m_pTransformCom->Bind_ShaderResource(m_pShaderCom, "g_WorldMatrix")))
		return E_FAIL;
	if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix", &m_pGameInstance->Get_TransformFloat4x4(CPipeLine::D3DTS_VIEW))))
		return E_FAIL;
	if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix", &m_pGameInstance->Get_TransformFloat4x4(CPipeLine::D3DTS_PROJ))))
		return E_FAIL;
	if (FAILED(m_pShaderCom->Bind_RawValue("g_isRed", &m_isRed, sizeof(_bool))))
		return E_FAIL;

	const LIGHT_DESC* pLightDesc = m_pGameInstance->Get_LightDesc(0);


	if (FAILED(m_pShaderCom->Bind_RawValue("g_vLightDir", &pLightDesc->vDirection, sizeof(_float4))))
		return E_FAIL;
	if (FAILED(m_pShaderCom->Bind_RawValue("g_vLightDiffuse", &pLightDesc->vDiffuse, sizeof(_float4))))
		return E_FAIL;
	if (FAILED(m_pShaderCom->Bind_RawValue("g_vLightAmbient", &pLightDesc->vAmbient, sizeof(_float4))))
		return E_FAIL;
	if (FAILED(m_pShaderCom->Bind_RawValue("g_vLightSpecular", &pLightDesc->vSpecular, sizeof(_float4))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_RawValue("g_vCamPosition", m_pGameInstance->Get_CamPosition(), sizeof(_float4))))
		return E_FAIL;

*/

	return S_OK;
}

CPlayer_1rd* CPlayer_1rd::Create(EngineContext* _pcontext)
{
	return nullptr;
}

CGameObject* CPlayer_1rd::Clone(void* pArg)
{
	return nullptr;
}

void CPlayer_1rd::Free()
{
	__super::Free();

	//Safe_Release(m_pColliderCom);
	Safe_Release(m_pModelCom);
}
