#include "Ketchup_Gun.h"
#include "GameInstance.h"
#include "Model.h"

CKetchup_Gun::CKetchup_Gun(EngineContext* pContext)
	: CPartObj{ pContext }
{
}

CKetchup_Gun::CKetchup_Gun(const CKetchup_Gun& Prototype)
	: CPartObj(Prototype.m_pContext)
{
}

HRESULT CKetchup_Gun::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CKetchup_Gun::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return E_FAIL;

	KETCHUP_GUN_DESC* pDesc = static_cast<KETCHUP_GUN_DESC*>(pArg);
	m_pSocketMatrix = pDesc->pSocketMatrix;
	m_strModelTag = pDesc->strModelTag;
	m_iModelLevelIndex = pDesc->iModelLevelIndex;

	if (FAILED(__super::Initialize(pDesc)))
		return E_FAIL;

	m_pTransformCom->Scaling(pDesc->vScale.x, pDesc->vScale.y, pDesc->vScale.z);

	if (FAILED(Ready_Components()))
		return E_FAIL;

	m_pTransformCom->RotationQuaternion(XM_PI / 2, 0.f, 0.f);

    m_pModelCom->SetUp_Animation(2, true);
	return S_OK;
}

void CKetchup_Gun::Priority_Update(_float fTimeDelta)
{
}

void CKetchup_Gun::Update(_float fTimeDelta)
{
    if (!m_pModelCom->Get_Blend())
        m_pModelCom->Play_Animation(fTimeDelta);
    else
        m_pModelCom->Blend_Animation(fTimeDelta);


	XMStoreFloat4x4(&m_WorldMatrix, m_pTransformCom->Get_WorldMatrix() * XMLoadFloat4x4(m_pSocketMatrix) * XMLoadFloat4x4(m_pParentMatrix));
}

void CKetchup_Gun::Late_Update(_float fTimeDelta)
{
    Cull_And_Submit(CRenderer::RG_NONBLEND);
}

void CKetchup_Gun::Render(ID3D12GraphicsCommandList* _commandList)
{
	XMFLOAT4X4 WorldMatrix;
	XMStoreFloat4x4(&WorldMatrix, m_pTransformCom->Get_WorldMatrix());
	_commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &m_WorldMatrix, 0);

	// 2. PSO 설정
	m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::DEFAULT);

	// 3. 메쉬별 렌더링 (머티리얼 바인딩 + DrawIndexedInstanced)
	_uint iNumMeshes = m_pModelCom->Get_NumMeshes();
	for (_uint i = 0; i < iNumMeshes; ++i)
	{
		//m_pModelCom->Bind_BoneMatrices(_commandList, i);
		m_pModelCom->Render(_commandList, i);
	}
}

void CKetchup_Gun::ShadowRender(ID3D12GraphicsCommandList* _commandList)
{
	XMFLOAT4X4 WorldMatrix;
	XMStoreFloat4x4(&WorldMatrix, m_pTransformCom->Get_WorldMatrix());
	_commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &m_WorldMatrix, 0);

	// 2. PSO 설정
	m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::SHADOW_STATIC);

	// 3. 메쉬별 렌더링 (머티리얼 바인딩 + DrawIndexedInstanced)
	_uint iNumMeshes = m_pModelCom->Get_NumMeshes();
	for (_uint i = 0; i < iNumMeshes; ++i)
	{
		//m_pModelCom->Bind_BoneMatrices(_commandList, i);
		m_pModelCom->Render(_commandList, i, true);
	}
}

HRESULT CKetchup_Gun::Ready_Components()
{
	if (FAILED(Add_Component(m_iModelLevelIndex, m_strModelTag,
		TEXT("Com_Model"), reinterpret_cast<CComponent**>(&m_pModelCom))))
	{
		MSG_BOX("Failed to Add Component : Model in CKetchup_Gun");
		return E_FAIL;
	}

	return S_OK;
}

CKetchup_Gun* CKetchup_Gun::Create(EngineContext* pContext)
{
	CKetchup_Gun* pInstance = new CKetchup_Gun(pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CKetchup_Gun");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CGameObject* CKetchup_Gun::Clone(void* pArg)
{
	CKetchup_Gun* pInstance = new CKetchup_Gun(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CKetchup_Gun");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CKetchup_Gun::Free()
{
	__super::Free();
}
