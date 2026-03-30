#include "Player_1rd.h"
#include "GameInstance.h"
#include "Transform.h"
#include "Ketchup_Gun.h"
#include "Bounding_AABB.h"
#include "Bounding_Sphere.h"
#include "Bounding_OBB.h"

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
	m_pCamera = pDesc->pCamera;
	pDesc->iNumPartObj = 1;
	pDesc->fSpeedPerSec = 100.f;
	pDesc->fRotationPerSec = 1.f;
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

void CPlayer_1rd::Priority_Update(_float fTimeDelta)
{
	if (m_bOnOff == true)
	{
		// 마우스 이동량에 따른 카메라 회전
		_long		MouseMove = {};
		if (MouseMove = m_pGameInstance->Get_DIMouseMove(Engine::DIMS_X))
		{
			m_pTransformCom->Turn(XMVectorSet(0.f, 1.f, 0.f, 0.f), MouseMove * fTimeDelta * 2.2f);
		}

		/*if (MouseMove = m_pGameInstance->Get_DIMouseMove(Engine::DIMS_Y))
		{
			m_pTransformCom->Turn(m_pTransformCom->Get_State(CTransform::STATE_RIGHT), MouseMove * fTimeDelta * 2.2f);
		}*/

		// 키보드 입력에 따른 카메라 이동
		if (m_pGameInstance->Key_Pressing(DIK_W) && m_pGameInstance->Key_Pressing(DIK_S))
		{
			if (m_pGameInstance->Key_Pressing(DIK_A) && m_pGameInstance->Key_Pressing(DIK_D))
			{

			}
			else if (m_pGameInstance->Key_Pressing(DIK_A))
			{
				m_pTransformCom->Go_Left(fTimeDelta);
			}
			else if (m_pGameInstance->Key_Pressing(DIK_D))
			{
				m_pTransformCom->Go_Right(fTimeDelta);
			}
		}
		else if (m_pGameInstance->Key_Pressing(DIK_W))
		{
			if (m_pGameInstance->Key_Pressing(DIK_A) && m_pGameInstance->Key_Pressing(DIK_D))
			{
				m_pTransformCom->Go_Straight(fTimeDelta);
			}
			else if (m_pGameInstance->Key_Pressing(DIK_A))
			{
				m_pTransformCom->Go_Straight(fTimeDelta * 0.7071f);
				m_pTransformCom->Go_Left(fTimeDelta * 0.7071f);
			}
			else if (m_pGameInstance->Key_Pressing(DIK_D))
			{
				m_pTransformCom->Go_Straight(fTimeDelta * 0.7071f);
				m_pTransformCom->Go_Right(fTimeDelta * 0.7071f);
			}
			else
			{
				m_pTransformCom->Go_Straight(fTimeDelta);
			}
		}
		else if (m_pGameInstance->Key_Pressing(DIK_S))
		{
			if (m_pGameInstance->Key_Pressing(DIK_A) && m_pGameInstance->Key_Pressing(DIK_D))
			{
				m_pTransformCom->Go_Backward(fTimeDelta);
			}
			else if (m_pGameInstance->Key_Pressing(DIK_A))
			{
				m_pTransformCom->Go_Backward(fTimeDelta * 0.7071f);
				m_pTransformCom->Go_Left(fTimeDelta * 0.7071f);
			}
			else if (m_pGameInstance->Key_Pressing(DIK_D))
			{
				m_pTransformCom->Go_Backward(fTimeDelta * 0.7071f);
				m_pTransformCom->Go_Right(fTimeDelta * 0.7071f);
			}
			else
			{
				m_pTransformCom->Go_Backward(fTimeDelta);
			}
		}
		else if (m_pGameInstance->Key_Pressing(DIK_A) && m_pGameInstance->Key_Pressing(DIK_D))
		{

		}
		else if (m_pGameInstance->Key_Pressing(DIK_A))
		{
			m_pTransformCom->Go_Left(fTimeDelta);
		}
		else if (m_pGameInstance->Key_Pressing(DIK_D))
		{
			m_pTransformCom->Go_Right(fTimeDelta);
		}

		if (m_pGameInstance->Key_Pressing(DIK_SPACE))
		{
			m_pTransformCom->Go_Up(fTimeDelta);
		}
		else if (m_pGameInstance->Key_Pressing(DIK_LCONTROL))
		{
			m_pTransformCom->Go_Up(-fTimeDelta);
		}
	}

	__super::Priority_Update(fTimeDelta);
}

void CPlayer_1rd::Update(_float fTimeDelta)
{
	m_pModelCom->Play_Animation(fTimeDelta);

	for (CCollider* pCollider : m_vColliderComs)
	{
		if (pCollider != nullptr)
			pCollider->Update();
	}

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

	// 콜라이더 디버깅
#ifdef _DEBUG
	for (CCollider* pCollider : m_vColliderComs)
	{
		if (pCollider != nullptr)
			m_pGameInstance->Add_RenderCollider(pCollider);
	}
#endif
}

HRESULT CPlayer_1rd::Ready_PartObjects()
{
	// 케첩건
	{
		CKetchup_Gun::KETCHUP_GUN_DESC cdesc;
		cdesc.strModelTag = L"Prototype_Component_ketchupGun";
		cdesc.iModelLevelIndex = 1;
		cdesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
		cdesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hand.R");
		cdesc.vScale = _float3(0.2f, 0.2f, 0.2f);
		m_PartObjects[0] = static_cast<CPartObj*>(m_pGameInstance->Clone_Prototype(Engine::PROTOTYPE::PROTO_GAMEOBJ, 1, TEXT("Prototype_GameObject_Ketchup_Gun"), &cdesc));
		if (nullptr == m_PartObjects[0])
			return E_FAIL;
	}
	return S_OK;
}

HRESULT CPlayer_1rd::Ready_Components()
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

		// Test Collider
		{
			CBounding_AABB::BOUND_AABB_DESC ColliderDesc;
			ColliderDesc.vExtents = _float3(50.f, 125.0f, 50.f);
			ColliderDesc.vCenter = _float3(0.0f, 0.0f, 0.0f);
			ColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hand.L");
			ColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_AABB"),
				TEXT("Com_Collider_AABB"), reinterpret_cast<CComponent**>(&m_vColliderComs[0]), &ColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Collider in Player_3rd");
				return E_FAIL;
			}
		}
		// Test Collider
		{
			CBounding_Sphere::BOUND_SPHERE_DESC HeadColliderDesc;
			HeadColliderDesc.fRadius = 55.f;
			HeadColliderDesc.vCenter = _float3(0.f, 0.f, 0.f);
			HeadColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hand.L");
			HeadColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_Sphere"),
				TEXT("Com_Collider_SPHERE"), reinterpret_cast<CComponent**>(&m_vColliderComs[1]), &HeadColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
		// Test Collider
		{
			CBounding_OBB::BOUND_OBB_DESC ColliderDesc;
			ColliderDesc.vExtents = _float3(50.f, 100.0f, 20.f);
			ColliderDesc.vCenter = _float3(0.0f, 0.0f, 0.0f);
			ColliderDesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("hand.L");
			ColliderDesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
			if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_OBB"),
				TEXT("Com_Collider_OBB"), reinterpret_cast<CComponent**>(&m_vColliderComs[2]), &ColliderDesc)))
			{
				MSG_BOX("Failed to Add Component : Head Collider in Player_3rd");
				return E_FAIL;
			}
		}
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
