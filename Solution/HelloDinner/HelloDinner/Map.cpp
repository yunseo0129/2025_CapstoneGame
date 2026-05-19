#include "Map.h"
#include "Transform.h"
#include "GameInstance.h"
#include "Model.h"
#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"


CMap::CMap(EngineContext* pContext)
	: CGameObject(pContext)
{
	Safe_AddRef(m_pModelCom);
}

CMap::CMap(const CMap& Prototype)
	: CGameObject(Prototype.m_pContext)
{
	m_strModelTag = Prototype.m_strModelTag;
	m_iModelLevelIndex = Prototype.m_iModelLevelIndex;
	m_pModelCom = Prototype.m_pModelCom;
	Safe_AddRef(m_pModelCom);
}

HRESULT CMap::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CMap::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return E_FAIL;

	MAP_DESC* pDesc = static_cast<MAP_DESC*>(pArg);

	m_strModelTag = pDesc->strModelTag;
	m_iModelLevelIndex = pDesc->iModelLevelIndex;

	// CGameObject::Initialize가 Transform 생성 및 속도 설정
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	// JSON에서 받은 TRS 적용
	m_pTransformCom->Scaling(pDesc->vScale.x, pDesc->vScale.y, pDesc->vScale.z);

	m_pTransformCom->EulerRotationQuaternion(pDesc->vRotation.x,pDesc->vRotation.y,pDesc->vRotation.z);

	m_pTransformCom->Set_State(CTransform::STATE_POSITION,
		XMVectorSet(pDesc->vPosition.x, pDesc->vPosition.y, pDesc->vPosition.z, 1.f));

	// Collider 정보
	m_eColliderType = pDesc->eColliderType;
	m_vCenterCollider = pDesc->vCenterCollider;
	m_vExtentsCollider = pDesc->vExtentsCollider;
	m_vRotationCollider = pDesc->vRotationCollider;
	m_fRadius = pDesc->fRadius;

	if (FAILED(Ready_Components()))
		return E_FAIL;

	return S_OK;
}

void CMap::Priority_Update(_float fTimeDelta)
{
}

void CMap::Update(_float fTimeDelta)
{
}

void CMap::Late_Update(_float fTimeDelta)
{
    Cull_And_Submit(CRenderer::RG_NONBLEND);
    m_pGameInstance->Add_CollisionGroup(0, m_pColliderCom);
}

void CMap::Render(ID3D12GraphicsCommandList* _commandList)
{
	// Transform 컴포넌트의 월드 행렬을 RootConstantBuffer에 넘겨준다.
	XMFLOAT4X4 WorldMatrix;
	XMStoreFloat4x4(&WorldMatrix, m_pTransformCom->Get_WorldMatrix());
	_commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &WorldMatrix, 0);

	// 2. PSO 설정
	m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::DEFAULT);

	// 3. 메쉬별 렌더링 (머티리얼 바인딩 + DrawIndexedInstanced)
	_uint iNumMeshes = m_pModelCom->Get_NumMeshes();
	for (_uint i = 0; i < iNumMeshes; ++i)
	{
		m_pModelCom->Render(_commandList, i);
	}

#ifdef _DEBUG
		m_pGameInstance->Add_RenderCollider(m_pColliderCom);
#endif
}

void CMap::ShadowRender(ID3D12GraphicsCommandList* _commandList)
{
	// Transform 컴포넌트의 월드 행렬을 RootConstantBuffer에 넘겨준다.
	XMFLOAT4X4 WorldMatrix;
	XMStoreFloat4x4(&WorldMatrix, m_pTransformCom->Get_WorldMatrix());
	_commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &WorldMatrix, 0);

	// 2. PSO 설정
	m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::SHADOW_STATIC);

	// 3. 메쉬별 렌더링 (머티리얼 바인딩 + DrawIndexedInstanced)
	_uint iNumMeshes = m_pModelCom->Get_NumMeshes();
	for (_uint i = 0; i < iNumMeshes; ++i)
	{
		m_pModelCom->Render(_commandList, i, true);
	}

}

bool CMap::Get_WorldBoundingSphere(_float3& outCenter, _float& outRadius) const
{
    if (!m_pColliderCom) return false;
    return m_pColliderCom->Get_SphereBound(outCenter, outRadius);
}

HRESULT CMap::Ready_Components()
{
	if (FAILED(Add_Component(m_iModelLevelIndex, m_strModelTag,
		TEXT("Com_Model"), reinterpret_cast<CComponent**>(&m_pModelCom))))
	{
		MSG_BOX("Failed to Add Component : Model in CMap");
		return E_FAIL;
	}

	// Collider 충돌체 생성
	if (m_eColliderType == CCollider::TYPE_SPHERE)
	{
		CBounding_Sphere::BOUND_SPHERE_DESC SphereDesc;
		SphereDesc.fRadius = m_fRadius;
		SphereDesc.vCenter = m_vCenterCollider;
		if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_Sphere"),
			TEXT("Com_Collider"), reinterpret_cast<CComponent**>(&m_pColliderCom), &SphereDesc)))
		{
			MSG_BOX("Failed to Add Component : Collider in CMap");
			return E_FAIL;
		}
	}
	else if (m_eColliderType == CCollider::TYPE_AABB)
	{
		CBounding_AABB::BOUND_AABB_DESC AABBDesc;
		AABBDesc.vExtents = m_vExtentsCollider;
		AABBDesc.vCenter = m_vCenterCollider;
		if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_AABB"),
			TEXT("Com_Collider"), reinterpret_cast<CComponent**>(&m_pColliderCom), &AABBDesc)))
		{
			MSG_BOX("Failed to Add Component : Collider in CMap");
			return E_FAIL;
		}
	}
	else if (m_eColliderType == CCollider::TYPE_OBB)
	{
		CBounding_OBB::BOUND_OBB_DESC OBBDesc;
		OBBDesc.vExtents = m_vExtentsCollider;
		OBBDesc.vCenter = m_vCenterCollider;
		OBBDesc.vRotation = m_vRotationCollider;
		if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
			TEXT("Com_Collider"), reinterpret_cast<CComponent**>(&m_pColliderCom), &OBBDesc)))
		{
			MSG_BOX("Failed to Add Component : Collider in CMap");
			return E_FAIL;
		}
	}

	return S_OK;
}

CMap* CMap::Create(EngineContext* pContext)
{
	CMap* pInstance = new CMap(pContext);
	if (FAILED(pInstance->Initialize_Prototype()))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Create : CMap");
	}
	return pInstance;
}

CGameObject* CMap::Clone(void* pArg)
{
	CMap* pInstance = new CMap(*this);
	if (FAILED(pInstance->Initialize(pArg)))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Clone : CMap");
	}
	return pInstance;
}

void CMap::Free()
{
	__super::Free();
}