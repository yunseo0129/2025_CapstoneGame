#include "Player_3rd.h"
#include "Transform.h"
#include "GameInstance.h"
#include "Model.h"
#include "Bounding_AABB.h"

CPlayer_3rd::CPlayer_3rd(EngineContext* pContext)
	: CGameObject(pContext)
{
}

CPlayer_3rd::CPlayer_3rd(const CPlayer_3rd& Prototype)
	: CGameObject(Prototype.m_pContext)
{
	m_vColliderComs = Prototype.m_vColliderComs;
	for (auto& pCollider : m_vColliderComs)
	{
		if (pCollider != nullptr)
			Safe_AddRef(pCollider);
	}
}

HRESULT CPlayer_3rd::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CPlayer_3rd::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return E_FAIL;

	Player_3rd_DESC* pDesc = static_cast<Player_3rd_DESC*>(pArg);

	m_strModelTag = pDesc->strModelTag;
	m_iModelLevelIndex = pDesc->iModelLevelIndex;

	// CGameObject::Initialize가 Transform 생성 및 속도 설정
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	// JSON에서 받은 TRS 적용
	m_pTransformCom->Scaling(pDesc->vScale.x, pDesc->vScale.y, pDesc->vScale.z);
	m_pTransformCom->RotationQuaternion(pDesc->vRotation.x, pDesc->vRotation.y, pDesc->vRotation.z);
	m_pTransformCom->Set_State(CTransform::STATE_POSITION,
		XMVectorSet(pDesc->vPosition.x, pDesc->vPosition.y, pDesc->vPosition.z, 1.f));

	if (FAILED(Ready_Components()))
		return E_FAIL;

	return S_OK;
}

void CPlayer_3rd::Priority_Update(_float fTimeDelta)
{
}

void CPlayer_3rd::Update(_float fTimeDelta)
{
	// 충돌체가 오브젝트 잘 따라가는지 확인하기 위해서 x축 이동
	_vector vCurrentPos = m_pTransformCom->Get_State(CTransform::STATE_POSITION);
	vCurrentPos = XMVectorAdd(vCurrentPos, XMVectorSet(10.f * fTimeDelta, 0.f, 0.f, 0.f));
	m_pTransformCom->Set_State(CTransform::STATE_POSITION, vCurrentPos);
}

void CPlayer_3rd::Late_Update(_float fTimeDelta)
{
	m_pGameInstance->Add_RenderObject(CRenderer::RG_NONBLEND, this);
}

void CPlayer_3rd::Render(ID3D12GraphicsCommandList* _commandList)
{
	// Transform 컴포넌트의 월드 행렬을 RootConstantBuffer에 넘겨준다.
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

HRESULT CPlayer_3rd::Ready_Components()
{
	if (FAILED(Add_Component(m_iModelLevelIndex, m_strModelTag,
		TEXT("Com_Model"), reinterpret_cast<CComponent**>(&m_pModelCom))))
	{
		MSG_BOX("Failed to Add Component : Model in Player_3rd");
		return E_FAIL;
	}

	return S_OK;
}

CPlayer_3rd* CPlayer_3rd::Create(EngineContext* pContext)
{
	return nullptr;
}

CGameObject* CPlayer_3rd::Clone(void* pArg)
{
	return nullptr;
}

void CPlayer_3rd::Free()
{
	__super::Free();
	for (auto& pCollider : m_vColliderComs)
	{
		Safe_Release(pCollider);
	}
}