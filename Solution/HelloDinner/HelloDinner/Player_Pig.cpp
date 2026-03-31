#include "Player_Pig.h"
#include "GameInstance.h"
#include "Transform.h"
#include "Ketchup_Gun.h"
#include "Bounding_AABB.h"
#include "Bounding_Sphere.h"
#include "Bounding_OBB.h"

CPlayer_Pig::CPlayer_Pig(EngineContext* _pcontext)
	: CContainerObj{ _pcontext }
{

}

CPlayer_Pig::CPlayer_Pig(const CPlayer_Pig& Prototype)
	: CContainerObj(Prototype.m_pContext)
{

}

HRESULT CPlayer_Pig::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CPlayer_Pig::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return E_FAIL;

	PLAYER_PIG_DESC* pDesc = static_cast<PLAYER_PIG_DESC*>(pArg);
	m_strModelTag = pDesc->strModelTag;
	m_iModelLevelIndex = pDesc->iModelLevelIndex;
	pDesc->iNumPartObj = 1;
	pDesc->fSpeedPerSec = 1.f;
	pDesc->fRotationPerSec = 1.f;
	pDesc->fSpeedPerSec = 1.f;
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	m_pTransformCom->Scaling(1.f, 1.f, 1.f);
	m_pTransformCom->RotationQuaternion(pDesc->vRotation.x, pDesc->vRotation.y, pDesc->vRotation.z);
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

void CPlayer_Pig::Priority_Update(_float fTimeDelta)
{
	__super::Priority_Update(fTimeDelta);
}

void CPlayer_Pig::Update(_float fTimeDelta)
{
	m_pModelCom->Play_Animation(fTimeDelta);

	for (CCollider* pCollider : m_vColliderComs)
	{
		if (pCollider != nullptr)
			pCollider->Update();
	}
	for (CCollider* pCollider : m_vMapColliderComs)
	{
		if (pCollider != nullptr)
			pCollider->Update();
	}

	__super::Update(fTimeDelta);
}

void CPlayer_Pig::Late_Update(_float fTimeDelta)
{
	m_pGameInstance->Add_RenderObject(CRenderer::RG_NONBLEND, this);
	__super::Late_Update(fTimeDelta);
}

void CPlayer_Pig::Render(ID3D12GraphicsCommandList* _commandList)
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

	// 콜라이더 디버깅
#ifdef _DEBUG
	for (CCollider* pCollider : m_vColliderComs)
	{
		if (pCollider != nullptr)
			m_pGameInstance->Add_RenderCollider(pCollider);
	}
	for (CCollider* pCollider : m_vMapColliderComs)
	{
		if (pCollider != nullptr)
			m_pGameInstance->Add_RenderCollider(pCollider);
	}
#endif
}

void CPlayer_Pig::ShadowRender(ID3D12GraphicsCommandList* _commandList)
{
	// Transform 컴포넌트의 월드 행렬을 RootConstantBuffer에 넘겨준다.
	XMFLOAT4X4 WorldMatrix;
	XMStoreFloat4x4(&WorldMatrix, m_pTransformCom->Get_WorldMatrix());
	_commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &WorldMatrix, 0);

	// 2. PSO 설정
	m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::SHADOW_ANIM);

	// 3. 메쉬별 렌더링 (머티리얼 바인딩 + DrawIndexedInstanced)
	_uint iNumMeshes = m_pModelCom->Get_NumMeshes();
	for (_uint i = 0; i < iNumMeshes; ++i)
	{
		m_pModelCom->Bind_BoneMatrices(_commandList, i);
		m_pModelCom->Render(_commandList, i, true);
	}

}

HRESULT CPlayer_Pig::Ready_PartObjects()
{
	// 케첩건
	{
		CKetchup_Gun::KETCHUP_GUN_DESC cdesc;
		cdesc.strModelTag = L"Prototype_Component_ketchupGun";
		cdesc.iModelLevelIndex = 1;
		cdesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
		cdesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("weapon");
		cdesc.vScale = _float3(1.f, 1.f, 1.f);
		m_PartObjects[0] = static_cast<CPartObj*>(m_pGameInstance->Clone_Prototype(Engine::PROTOTYPE::PROTO_GAMEOBJ, 1, TEXT("Prototype_GameObject_Ketchup_Gun"), &cdesc));
		if (nullptr == m_PartObjects[0])
			return E_FAIL;
	}
	return S_OK;
}

HRESULT CPlayer_Pig::Ready_Components()
{
	// Model 컴포넌트 생성
	if (FAILED(Add_Component(m_iModelLevelIndex, m_strModelTag,
		TEXT("Com_Model"), reinterpret_cast<CComponent**>(&m_pModelCom))))
	{
		MSG_BOX("Failed to Add Component : Model in CPlayer_1rd");
		return E_FAIL;
	}

	// Collider 컴포넌트 생성
	{
		m_vColliderComs.resize(COLLIDER_END, nullptr);
		m_vMapColliderComs.resize(2, nullptr);

		// MapCollider
		{
			CBounding_Sphere::BOUND_SPHERE_DESC HeadColliderDesc;
			HeadColliderDesc.fRadius = 0.41f;
			HeadColliderDesc.vCenter = _float3(0.f, 0.41f, 0.f);
			HeadColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("origin");
			HeadColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_Sphere"),
				TEXT("Com_MapCollider0"), reinterpret_cast<CComponent**>(&m_vMapColliderComs[0]), &HeadColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
		{
			CBounding_Sphere::BOUND_SPHERE_DESC HeadColliderDesc;
			HeadColliderDesc.fRadius = 0.41f;
			HeadColliderDesc.vCenter = _float3(0.f, 1.23f, 0.f);
			HeadColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("origin");
			HeadColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_Sphere"),
				TEXT("Com_MapCollider1"), reinterpret_cast<CComponent**>(&m_vMapColliderComs[1]), &HeadColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}

		// COLLIDER_MAIN
		{
			CBounding_AABB::BOUND_AABB_DESC ColliderDesc;
			ColliderDesc.vExtents = _float3(0.41f, 0.82f, 0.41f);
			ColliderDesc.vCenter = _float3(0.0f, 0.82f, 0.0f);
			ColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("origin");
			ColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_AABB"),
				TEXT("Com_Collider0"), reinterpret_cast<CComponent**>(&m_vColliderComs[COLLIDER_MAIN]), &ColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Collider in Player_3rd");
				return E_FAIL;
			}
		}
		// COLLIDER_HEAD
		{
			CBounding_Sphere::BOUND_SPHERE_DESC HeadColliderDesc;
			HeadColliderDesc.fRadius = 0.3f;
			HeadColliderDesc.vCenter = _float3(0.f, 0.f, 0.f);
			HeadColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hitbox_head");
			HeadColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_Sphere"),
				TEXT("Com_Collider1"), reinterpret_cast<CComponent**>(&m_vColliderComs[COLLIDER_HEAD]), &HeadColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
		// COLLIDER_ARM_UP_L
		{
			CBounding_OBB::BOUND_OBB_DESC ColliderDesc;
			ColliderDesc.vExtents = _float3(0.2f, 0.08f, 0.08f);
			ColliderDesc.vCenter = _float3(0.0f, 0.0f, 0.0f);
			ColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hitbox_upper_arm.L");
			ColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
				TEXT("Com_Collider2"), reinterpret_cast<CComponent**>(&m_vColliderComs[COLLIDER_ARM_UP_L]), &ColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
		// COLLIDER_ARM_UP_R
		{
			CBounding_OBB::BOUND_OBB_DESC ColliderDesc;
			ColliderDesc.vExtents = _float3(0.2f, 0.08f, 0.08f);
			ColliderDesc.vCenter = _float3(0.0f, 0.0f, 0.0f);
			ColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hitbox_upper_arm.R");
			ColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
				TEXT("Com_Collider3"), reinterpret_cast<CComponent**>(&m_vColliderComs[COLLIDER_ARM_UP_R]), &ColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
		// COLLIDER_ARM_LOW_L
		{
			CBounding_OBB::BOUND_OBB_DESC ColliderDesc;
			ColliderDesc.vExtents = _float3(0.2f, 0.08f, 0.08f);
			ColliderDesc.vCenter = _float3(0.0f, 0.0f, 0.0f);
			ColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hitbox_lower_arm.L");
			ColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
				TEXT("Com_Collider4"), reinterpret_cast<CComponent**>(&m_vColliderComs[COLLIDER_ARM_LOW_L]), &ColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
		// COLLIDER_ARM_LOW_R
		{
			CBounding_OBB::BOUND_OBB_DESC ColliderDesc;
			ColliderDesc.vExtents = _float3(0.2f, 0.08f, 0.08f);
			ColliderDesc.vCenter = _float3(0.0f, 0.0f, 0.0f);
			ColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hitbox_lower_arm.R");
			ColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
				TEXT("Com_Collider5"), reinterpret_cast<CComponent**>(&m_vColliderComs[COLLIDER_ARM_LOW_R]), &ColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
		// COLLIDER_THIGH_L
		{
			CBounding_Sphere::BOUND_SPHERE_DESC HeadColliderDesc;
			HeadColliderDesc.fRadius = 0.28f;
			HeadColliderDesc.vCenter = _float3(0.05f, -0.0f, -0.03f);
			HeadColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hitbox_thigh.L");
			HeadColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_Sphere"),
				TEXT("Com_Collider6"), reinterpret_cast<CComponent**>(&m_vColliderComs[COLLIDER_THIGH_L]), &HeadColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
		// COLLIDER_THIGH_R
		{
			CBounding_Sphere::BOUND_SPHERE_DESC HeadColliderDesc;
			HeadColliderDesc.fRadius = 0.28f;
			HeadColliderDesc.vCenter = _float3(-0.05f, -0.0f, -0.03f);
			HeadColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hitbox_thigh.R");
			HeadColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_Sphere"),
				TEXT("Com_Collider7"), reinterpret_cast<CComponent**>(&m_vColliderComs[COLLIDER_THIGH_R]), &HeadColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
		// COLLIDER_SHIN_L
		{
			CBounding_OBB::BOUND_OBB_DESC ColliderDesc;
			ColliderDesc.vExtents = _float3(0.12f, 0.12f, 0.15f);
			ColliderDesc.vCenter = _float3(0.0f, 0.0f, 0.0f);
			ColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hitbox_shin.L");
			ColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
				TEXT("Com_Collider8"), reinterpret_cast<CComponent**>(&m_vColliderComs[COLLIDER_SHIN_L]), &ColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
		// COLLIDER_SHIN_R
		{
			CBounding_OBB::BOUND_OBB_DESC ColliderDesc;
			ColliderDesc.vExtents = _float3(0.12f, 0.12f, 0.15f);
			ColliderDesc.vCenter = _float3(0.0f, 0.0f, 0.0f);
			ColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hitbox_shin.R");
			ColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
				TEXT("Com_Collider9"), reinterpret_cast<CComponent**>(&m_vColliderComs[COLLIDER_SHIN_R]), &ColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
		// COLLIDER_BODY
		{
			CBounding_Sphere::BOUND_SPHERE_DESC HeadColliderDesc;
			HeadColliderDesc.fRadius = 0.35f;
			HeadColliderDesc.vCenter = _float3(0.f, 0.f, -0.05f);
			HeadColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hitbox_spine");
			HeadColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_Sphere"),
				TEXT("Com_Collider10"), reinterpret_cast<CComponent**>(&m_vColliderComs[COLLIDER_BODY]), &HeadColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
	}

	return S_OK;
}

CPlayer_Pig* CPlayer_Pig::Create(EngineContext* _pcontext)
{
	CPlayer_Pig* pInstance = new CPlayer_Pig(_pcontext);
	if (FAILED(pInstance->Initialize_Prototype()))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Create : CPlayer_Pig");
	}
	return pInstance;
}

CGameObject* CPlayer_Pig::Clone(void* pArg)
{
	CPlayer_Pig* pInstance = new CPlayer_Pig(*this);
	if (FAILED(pInstance->Initialize(pArg)))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Clone : CPlayer_Pig");
	}
	return pInstance;
}

void CPlayer_Pig::Free()
{
	__super::Free();

	for (CCollider* pCollider : m_vColliderComs)
	{
		if (pCollider != nullptr)
			Safe_Release(pCollider);
	}
	for (CCollider* pCollider : m_vMapColliderComs)
	{
		if (pCollider != nullptr)
			Safe_Release(pCollider);
	}
}
