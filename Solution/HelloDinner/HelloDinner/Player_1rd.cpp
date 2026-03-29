#include "Player_1rd.h"
#include "GameInstance.h"
#include "Transform.h"

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

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	m_pTransformCom->Scaling(1.f, 1.f, 1.f);
	m_pTransformCom->Set_State(CTransform::STATE_POSITION, pDesc->vPos);

	if (FAILED(Ready_Components()))
		return E_FAIL;

	m_iState = 0;
	m_pModelCom->SetUp_Animation(0, true);

	return S_OK;
}

void CPlayer_1rd::Priority_Update(_float fTimeDelta)
{
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
	// 모델에 붙을 다른 파츠모델들을 Add_GameObject_ToLayer하지않고 Clone_Prototype해서 m_PartObjects에 추가해준다.
	// 파츠오브젝트는 레이어에 넣어서 사용하지않음 왜냐? 컨테이너오브젝트 내부에서 관리하며 업데이트등 함수들을 제때불러줄거기때문.
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
	Safe_Release(m_pModelCom);
}
