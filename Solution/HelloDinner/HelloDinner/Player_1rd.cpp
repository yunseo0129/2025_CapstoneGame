#include "Player_1rd.h"
#include "GameInstance.h"
#include "Transform.h"
#include "Ketchup_Gun.h"

CPlayer_1rd::CPlayer_1rd(EngineContext* _pcontext)
	: CContainerObj{ _pcontext }
{

}

CPlayer_1rd::CPlayer_1rd(const CPlayer_1rd& Prototype)
	: CContainerObj(Prototype.m_pContext)
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
	pDesc->iNumPartObj = 1;
	pDesc->fSpeedPerSec = 10.f;

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	m_pTransformCom->Scaling(1.f, 1.f, 1.f);
	m_pTransformCom->Set_State(CTransform::STATE_POSITION,
		XMVectorSet(pDesc->vPos.x, pDesc->vPos.y, pDesc->vPos.z, 1.f));

	if (FAILED(Ready_Components()))
		return E_FAIL;

	m_iState = 0;
	m_pModelCom->SetUp_Animation(0, true);

	if (FAILED(Ready_PartObjects()))
		return E_FAIL;

	return S_OK;
}

void CPlayer_1rd::Priority_Update(_float fTimeDelta)
{
	m_pTransformCom->Go_Left(fTimeDelta);
	__super::Priority_Update(fTimeDelta);
}

void CPlayer_1rd::Update(_float fTimeDelta)
{
	m_pModelCom->Play_Animation(fTimeDelta);
	__super::Update(fTimeDelta);
}

void CPlayer_1rd::Late_Update(_float fTimeDelta)
{
	m_pGameInstance->Add_RenderObject(CRenderer::RG_NONBLEND, this);
	__super::Late_Update(fTimeDelta);
}

void CPlayer_1rd::Render(ID3D12GraphicsCommandList* _commandList)
{
	XMFLOAT4X4 WorldMatrix;
	XMStoreFloat4x4(&WorldMatrix, m_pTransformCom->Get_WorldMatrix());
	_commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &WorldMatrix, 0);

	// 2. PSO 설정
	m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::ANIM);

	// 3. 메쉬별 렌더링 (머티리얼 바인딩 + DrawIndexedInstanced)
	_uint iNumMeshes = m_pModelCom->Get_NumMeshes();
	for (_uint i = 0; i < iNumMeshes; ++i)
	{
		m_pModelCom->Bind_BoneMatrices(_commandList, i);
		m_pModelCom->Render(_commandList, i);
	}
}

HRESULT CPlayer_1rd::Ready_PartObjects()
{
	// 케첩건
	{
		CKetchup_Gun::KETCHUP_GUN_DESC cdesc;
		cdesc.strModelTag = L"Prototype_Component_ketchupGun";
		cdesc.iModelLevelIndex = 1;
		cdesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
		cdesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hand.L");
		cdesc.vScale = _float3(0.2f, 0.2f, 0.2f);
		m_PartObjects[0] = static_cast<CPartObj*>(m_pGameInstance->Clone_Prototype(Engine::PROTOTYPE::PROTO_GAMEOBJ, 1, TEXT("Prototype_GameObject_Ketchup_Gun"), &cdesc));
		if (nullptr == m_PartObjects[0])
			return E_FAIL;
	}
	return S_OK;
}

HRESULT CPlayer_1rd::Ready_Components()
{
	if (FAILED(Add_Component(m_iModelLevelIndex, m_strModelTag,
		TEXT("Com_Model"), reinterpret_cast<CComponent**>(&m_pModelCom))))
	{
		MSG_BOX("Failed to Add Component : Model in CPlayer_1rd");
		return E_FAIL;
	}

	return S_OK;
}

CPlayer_1rd* CPlayer_1rd::Create(EngineContext* _pcontext)
{
	CPlayer_1rd* pInstance = new CPlayer_1rd(_pcontext);
	if (FAILED(pInstance->Initialize_Prototype()))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Create : CPlayer_1rd");
	}
	return pInstance;
}

CGameObject* CPlayer_1rd::Clone(void* pArg)
{
	CPlayer_1rd* pInstance = new CPlayer_1rd(*this);
	if (FAILED(pInstance->Initialize(pArg)))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Clone : CPlayer_1rd");
	}
	return pInstance;
}

void CPlayer_1rd::Free()
{
	__super::Free();

	//Safe_Release(m_pColliderCom);
}
