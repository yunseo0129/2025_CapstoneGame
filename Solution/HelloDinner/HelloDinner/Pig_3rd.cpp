#include "Pig_3rd.h"
#include "Transform.h"
#include "GameInstance.h"
#include "Model.h"
#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"

CPig_3rd::CPig_3rd(EngineContext* pContext)
	: CPlayer_3rd(pContext)
{
}

CPig_3rd::CPig_3rd(const CPig_3rd& Prototype)
	: CPlayer_3rd(Prototype)
{
}

HRESULT CPig_3rd::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CPig_3rd::Initialize(void* pArg)
{
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	m_iState = STATE_IDLE;
	m_pModelCom->SetUp_Animation(0, true);

	if (FAILED(Ready_Components()))
		return E_FAIL;

	return S_OK;
}

void CPig_3rd::Priority_Update(_float fTimeDelta)
{
	__super::Priority_Update(fTimeDelta);
}

void CPig_3rd::Update(_float fTimeDelta)
{
	m_pModelCom->Play_Animation(fTimeDelta);
	
	__super::Update(fTimeDelta);


	// 충돌체 업데이트
	_matrix WorldMatrix = m_pTransformCom->Get_WorldMatrix();
	// 1. Main Collider (AABB) - Root 뼈 기준
	if ( m_vColliderComs[0] != nullptr) {
		m_vColliderComs[0]->Update (m_pTransformCom->Get_WorldMatrix ());
	}

	// 2. Head Collider (Sphere) - "head" 뼈
	if (nullptr != m_vColliderComs[1])
	{
		// 뼈의 Combined Matrix를 가져옵니다.
		_matrix BoneMatrix = XMLoadFloat4x4(m_pModelCom->Get_BoneMatrix("head"));
		// 뼈 행렬 * 월드 행렬 = 최종 소켓 월드 행렬
		_matrix SocketMatrix = XMMatrixMultiply(BoneMatrix, WorldMatrix);
		m_vColliderComs[1]->Update (SocketMatrix);
	}

	/*
	// 3. Body Collider (OBB) - "spine" 뼈 (또는 torso)
	if (nullptr != m_vColliderComs[2])
	{
		_matrix BoneMatrix = XMLoadFloat4x4(m_pModelCom->Get_BoneMatrix("spine"));
		m_vColliderComs[2]->Update(XMMatrixMultiply(BoneMatrix, WorldMatrix));
	}

	// 4. L_UpperArm Collider (OBB) - "upper_arm.L" 뼈
	if (nullptr != m_vColliderComs[3])
	{
		_matrix BoneMatrix = XMLoadFloat4x4(m_pModelCom->Get_BoneMatrix("upper_arm.L"));
		m_vColliderComs[3]->Update(XMMatrixMultiply(BoneMatrix, WorldMatrix));
	}

	// 5. L_LowerArm Collider (OBB) - "lower_arm.L" 뼈
	if (nullptr != m_vColliderComs[4])
	{
		_matrix BoneMatrix = XMLoadFloat4x4(m_pModelCom->Get_BoneMatrix("lower_arm.L"));
		m_vColliderComs[4]->Update(XMMatrixMultiply(BoneMatrix, WorldMatrix));
	}

	// 6. R_UpperArm Collider (OBB) - "upper_arm.R" 뼈
	if (nullptr != m_vColliderComs[5])
	{
		_matrix BoneMatrix = XMLoadFloat4x4(m_pModelCom->Get_BoneMatrix("upper_arm.R"));
		m_vColliderComs[5]->Update(XMMatrixMultiply(BoneMatrix, WorldMatrix));
	}

	// 7. R_LowerArm Collider (OBB) - "lower_arm.R" 뼈
	if (nullptr != m_vColliderComs[6])
	{
		_matrix BoneMatrix = XMLoadFloat4x4(m_pModelCom->Get_BoneMatrix("lower_arm.R"));
		m_vColliderComs[6]->Update(XMMatrixMultiply(BoneMatrix, WorldMatrix));
	}

	// 8. L_Leg Collider (OBB)
	if (nullptr != m_vColliderComs[7])
	{
		_matrix BoneMatrix = XMLoadFloat4x4(m_pModelCom->Get_BoneMatrix("thigh.L"));
		m_vColliderComs[7]->Update(XMMatrixMultiply(BoneMatrix, WorldMatrix));
	}

	// 9. R_Leg Collider (OBB)
	if (nullptr != m_vColliderComs[8])
	{
		_matrix BoneMatrix = XMLoadFloat4x4(m_pModelCom->Get_BoneMatrix("thigh.R"));
		m_vColliderComs[8]->Update(XMMatrixMultiply(BoneMatrix, WorldMatrix));
	}
	*/
}

void CPig_3rd::Late_Update(_float fTimeDelta)
{
	__super::Late_Update(fTimeDelta);
}

void CPig_3rd::Render(ID3D12GraphicsCommandList* _commandList)
{
	__super::Render(_commandList);
#ifdef _DEBUG
	for ( CCollider* pCollider : m_vColliderComs )
	{
		if ( pCollider != nullptr )
			m_pGameInstance->Add_RenderCollider(pCollider);
	}
#endif
}

HRESULT CPig_3rd::Ready_Components()
{
	/*
	if (FAILED(__super::Ready_Components()))
		return E_FAIL;
	*/

	// Main Collider
	m_vColliderComs.resize ( 2 , nullptr );		// 일단 2개만 (Main AABB, Head Sphere)

	CBounding_AABB::BOUND_AABB_DESC ColliderDesc;
	ColliderDesc.vExtents = _float3(50.f, 125.0f, 50.f);
	ColliderDesc.vCenter = _float3(0.0f, 125.0f, 0.0f);
	if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_AABB"),
		TEXT("Com_Collider"), reinterpret_cast<CComponent**>(&m_vColliderComs[0] ) , &ColliderDesc)) )
	{
		MSG_BOX("Failed to Add Component : Collider in Player_3rd");
		return E_FAIL;
	}

	// Head Collider
	CBounding_Sphere::BOUND_SPHERE_DESC HeadColliderDesc;
	HeadColliderDesc.fRadius = 55.f;
	HeadColliderDesc.vCenter = _float3(0.f, 40.f, 0.f);
	if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_Sphere"),
		TEXT("Com_Collider_Head"), reinterpret_cast<CComponent**>(&m_vColliderComs[1] ) , &HeadColliderDesc)) )
	{
		MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
		return E_FAIL;
	}

	// 언젠가 쓸모 있지 않을까싶어서 일단 몸 팔 다리 콜라이더도 만들어놓음
	/*
	// Body Collider
	CBounding_OBB::BOUND_OBB_DESC BodyColliderDesc;
	BodyColliderDesc.vExtents = _float3(40.f, 50.f, 40.f);
	BodyColliderDesc.vCenter = _float3(0.f, 15.f, 0.f);
	BodyColliderDesc.vRotation = _float3(0.f, 0.f, 0.f);
	if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
		TEXT("Com_Collider_Body"), reinterpret_cast<CComponent**>(&m_vColliderComs[2]), &BodyColliderDesc)))
	{
		MSG_BOX("Failed to Add Component : Body Collider");
		return E_FAIL;
	}

	// Left Upper Arm Collider
	CBounding_OBB::BOUND_OBB_DESC LUpperArmColliderDesc;
	LUpperArmColliderDesc.vExtents = _float3(12.f, 25.f, 12.f);
	LUpperArmColliderDesc.vCenter = _float3(0.f, 10.f, 0.f);
	LUpperArmColliderDesc.vRotation = _float3(0.f, 0.f, 0.f);

	if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
		TEXT("Com_Collider_L_UpperArm"), reinterpret_cast<CComponent**>(&m_vColliderComs[3]), &LUpperArmColliderDesc)))
	{
		MSG_BOX("Failed to Add Component : L_UpperArm Collider");
		return E_FAIL;
	}

	// Left Lower Arm Collider
	CBounding_OBB::BOUND_OBB_DESC LLowerArmColliderDesc;
	LLowerArmColliderDesc.vExtents = _float3(10.f, 25.f, 10.f);
	LLowerArmColliderDesc.vCenter = _float3(0.f, 10.f, 0.f);
	LLowerArmColliderDesc.vRotation = _float3(0.f, 0.f, 0.f);
	if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
		TEXT("Com_Collider_L_LoewrArm"), reinterpret_cast<CComponent**>(&m_vColliderComs[4]), &LLowerArmColliderDesc)))
	{
		MSG_BOX("Failed to Add Component : L_LowerArm Collider");
		return E_FAIL;
	}

	// Right Upper Arm Collider
	CBounding_OBB::BOUND_OBB_DESC RUpperArmColliderDesc;
	RUpperArmColliderDesc.vExtents = _float3(12.f, 25.f, 12.f);
	RUpperArmColliderDesc.vCenter = _float3(0.f, 10.f, 0.f);
	RUpperArmColliderDesc.vRotation = _float3(0.f, 0.f, 0.f);
	if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
		TEXT("Com_Collider_R_UpperArm"), reinterpret_cast<CComponent**>(&m_vColliderComs[5]), &RUpperArmColliderDesc)))
	{
		MSG_BOX("Failed to Add Component : R_UpperArm Collider");
		return E_FAIL;
	}

	// Right Lower Arm Collider
	CBounding_OBB::BOUND_OBB_DESC RLowerArmColliderDesc;
	RLowerArmColliderDesc.vExtents = _float3(10.f, 25.f, 10.f);
	RLowerArmColliderDesc.vCenter = _float3(0.f, 10.f, 0.f);
	RLowerArmColliderDesc.vRotation = _float3(0.f, 0.f, 0.f);
	if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
		TEXT("Com_Collider_R_LoewrArm"), reinterpret_cast<CComponent**>(&m_vColliderComs[6]), &RLowerArmColliderDesc)))
	{
		MSG_BOX("Failed to Add Component : R_LowerArm Collider");
		return E_FAIL;
	}

	// LLeg Collider
	CBounding_OBB::BOUND_OBB_DESC LLegColliderDesc;
	LLegColliderDesc.vExtents = _float3(30.f, 50.f, 30.f);
	LLegColliderDesc.vCenter = _float3(0.f, 50.f, 0.f);
	LLegColliderDesc.vRotation = _float3(0.f, 0.f, 0.f);
	if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
		TEXT("Com_Collider_L_Leg"), reinterpret_cast<CComponent**>(&m_vColliderComs[7]), &LLegColliderDesc)))
	{
		MSG_BOX("Failed to Add Component : L_Leg Collider");
		return E_FAIL;
	}

	// RLeg Collider
	CBounding_OBB::BOUND_OBB_DESC RLegColliderDesc;
	RLegColliderDesc.vExtents = _float3(30.f, 50.f, 30.f);
	RLegColliderDesc.vCenter = _float3(0.f, 50.f, 0.f);
	RLegColliderDesc.vRotation = _float3(0.f, 0.f, 0.f);
	if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
		TEXT("Com_Collider_R_Leg"), reinterpret_cast<CComponent**>(&m_vColliderComs[8]), &RLegColliderDesc)))
	{
		MSG_BOX("Failed to Add Component : R_Leg Collider");
		return E_FAIL;
	}
	*/
	return S_OK;
}

CPig_3rd* CPig_3rd::Create(EngineContext* pContext)
{
	CPig_3rd* pInstance = new CPig_3rd(pContext);
	if (FAILED(pInstance->Initialize_Prototype()))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Create : CPig_3rd");
	}
	return pInstance;
}

CGameObject* CPig_3rd::Clone(void* pArg)
{
	CPig_3rd* pInstance = new CPig_3rd(*this);
	if (FAILED(pInstance->Initialize(pArg)))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Clone : CPig_3rd");
	}
	return pInstance;
}

void CPig_3rd::Free()
{
	__super::Free();
}