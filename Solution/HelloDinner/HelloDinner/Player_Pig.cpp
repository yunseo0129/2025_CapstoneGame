#include "Player_Pig.h"
#include "GameInstance.h"
#include <cmath>
#include "Transform.h"
#include "Ketchup_Gun.h"
#include "Bounding_AABB.h"
#include "Bounding_Sphere.h"
#include "Bounding_OBB.h"
#include "../Server/protocol.h"
#include "Map.h"
#include "Collider.h"

namespace {
    constexpr float PIG_GRAVITY      = -9.8f;  // 중력 가속도 (units/s^2, Player_1rd와 동일)
    constexpr float PIG_TERMINAL_VEL = 20.f;   // 최대 낙하 속도 (터널링 방지)
}

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
static int a = 0;
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
	m_pModelCom->SetUp_Animation(0, true, true);
    m_pModelCom->SetUp_Animation(0, true, false);

	if (FAILED(Ready_PartObjects()))
		return E_FAIL;

    for (CCollider* p : m_vColliderComs)
        if (p) p->Set_Owner(this);

	return S_OK;
}

void CPlayer_Pig::Priority_Update(_float fTimeDelta)
{
	__super::Priority_Update(fTimeDelta);
}

void CPlayer_Pig::Update(_float fTimeDelta)
{
    // 포물선 발사 중에는 로컬 탄도 연출만 적용하고 데드레코닝 위치 갱신을 억제한다.
    // Apply_NetworkMatrix는 계속 호출되어 m_drTargetMat만 갱신 → 착지 후 부드럽게 수렴.
    if (m_bLaunching)
        Update_Launch(fTimeDelta);
    else
        Update_DeadReckoning(fTimeDelta);

    if (!m_pModelCom->Get_UpperBlend())
    {
        if (m_pModelCom->Play_Animation(fTimeDelta, true))
        {
            if (11 == m_pModelCom->Get_UpperAnimNum())
            {
                m_pModelCom->Change_Animation(0, 5.f, true, true);
                m_pModelCom->Change_Animation(0, 5.f, true, false);
            }
            else if (12 == m_pModelCom->Get_UpperAnimNum())
            {
                m_isReloading = false;
                m_pModelCom->Change_Animation(0, 0.f, true, true);
            }
        }
    }
    else
        m_pModelCom->Blend_Animation(fTimeDelta, true);

    if (!m_pModelCom->Get_LowerBlend())
        m_pModelCom->Play_Animation(fTimeDelta, false);
    else
        m_pModelCom->Blend_Animation(fTimeDelta, false);

    // 상하체 보간 없는 모델들만 사용
	/*if (!m_pModelCom->Get_Blend())
		m_pModelCom->Play_Animation(fTimeDelta);
	else
        m_pModelCom->Blend_Animation(fTimeDelta);*/

    Drive_Animation();

    m_pModelCom->Merge_UpperLower();

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
    Cull_And_Submit(CRenderer::RG_NONBLEND);
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
	/*for (CCollider* pCollider : m_vColliderComs)
	{
		if (pCollider != nullptr)
			m_pGameInstance->Add_RenderCollider(pCollider);
	}
	for (CCollider* pCollider : m_vMapColliderComs)
	{
		if (pCollider != nullptr)
			m_pGameInstance->Add_RenderCollider(pCollider);
	}*/
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
		cdesc.iModelLevelIndex = m_iModelLevelIndex;
		cdesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();
		cdesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("weapon.R");
		cdesc.vScale = _float3(1.f, 1.f, 1.f);
		m_PartObjects[0] = static_cast<CPartObj*>(m_pGameInstance->Clone_Prototype(Engine::PROTOTYPE::PROTO_GAMEOBJ, m_iModelLevelIndex, TEXT("Prototype_GameObject_Ketchup_Gun"), &cdesc));
		if (nullptr == m_PartObjects[0])
			return E_FAIL;
	}

    // 마요네즈건
    {
        CKetchup_Gun::KETCHUP_GUN_DESC cdesc;
        cdesc.strModelTag = L"Prototype_Component_MayonaiseGun";
        cdesc.iModelLevelIndex = m_iModelLevelIndex;
        cdesc.pParentMatrix = m_pTransformCom->Get_WorldFloat4x4_Ptr();;
        cdesc.pSocketMatrix = m_pModelCom->Get_BoneMatrix("weapon.R");
        cdesc.vScale = _float3(1.f, 1.f, 1.f);
        m_PartObjects.push_back(static_cast<CPartObj*>(m_pGameInstance->Clone_Prototype(Engine::PROTOTYPE::PROTO_GAMEOBJ, m_iModelLevelIndex, TEXT("Prototype_GameObject_Ketchup_Gun"), &cdesc)));
        if (nullptr == m_PartObjects[1])
            return E_FAIL;

        m_PartObjects[1]->SetOnOff(false);
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
    
    m_pModelCom->Set_SpineBoneIndex(m_pModelCom->Get_BoneIndex("spine"));

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
				MSG_BOX("Failed to Add Component : ARM_UP_L Collider in Player_3rd");
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

        _uint i = 0;
        for (CCollider* c : m_vColliderComs)
        {
            m_pGameInstance->Add_CollisionGroup(1, c);
            c->Set_PartNum(i);
            ++i;
        }
	}

	return S_OK;
}

void CPlayer_Pig::Drive_Animation()
{
    // 사망 중에는 이동 애니메이션 상태머신을 동작시키지 않는다 (사망 클립 덮어쓰기 방지)
    if (m_isDie) return;

    // 사격 (Player_1rd::Shoot 동일 — 상체 전용, 이동·점프와 독립)
    if (m_bShootPending)
    {
        m_bShootPending = false;
        if (!m_isReloading)
            m_pModelCom->Change_Animation(3, 0.f, false, true);
    }

    // 장전 (Player_1rd::Reload 동일)
    if (m_bReloadPending)
    {
        m_bReloadPending = false;
        if (!m_isReloading)
        {
            m_isReloading = true;
            m_pModelCom->Change_Animation(12, 3.f, false, true);
            static_cast<CKetchup_Gun*>(m_PartObjects[0])->Get_Model()->Change_Animation(2, 3.f, false);
            static_cast<CKetchup_Gun*>(m_PartObjects[1])->Get_Model()->Change_Animation(2, 3.f, false);
        }
    }

    // 공중 상태 — 착지 전까지 SPACE를 다시 눌러도 재트리거 금지
    // m_bIsJumping은 Apply_NetworkMatrix에서 Y<0.05 감지 시 해제
    if (m_bIsJumping) return;

    // 착지 후에도 애니메이션 11이 재생 중이면 끝날 때까지 대기
    if (m_pModelCom->Get_UpperAnimNum() == 11 || m_pModelCom->Get_LowerAnimNum() == 11)
        return;

    const bool bSpace = (m_keyInput & KEY_SPACE) != 0;

    // 착지 상태에서 SPACE 라이징 에지 → 점프 애니메이션 1회 재생
    if (bSpace && !(m_prevKeyInput & KEY_SPACE))
    {
        m_bIsJumping = true;
        m_pModelCom->Change_Animation(11, 0.f, false, true);
        m_pModelCom->Change_Animation(11, 0.f, false, false);
        return;
    }

    // 이동 애니메이션
    const bool bW     = (m_keyInput & KEY_W)     != 0;
    const bool bS     = (m_keyInput & KEY_S)     != 0;
    const bool bA     = (m_keyInput & KEY_A)     != 0;
    const bool bD     = (m_keyInput & KEY_D)     != 0;
    const bool bShift = (m_keyInput & KEY_SHIFT) != 0;
    const bool bCtrl  = (m_keyInput & KEY_CTRL)  != 0;

    const bool isMove   = bW || bS || bA || bD;
    const bool isRun    = bShift && !bCtrl;
    const bool isCrouch = bCtrl;

    _uint lowerAnim = m_pModelCom->Get_LowerAnimNum();

    if (isMove)
    {
        if (isCrouch)   { if (lowerAnim != 5)  m_pModelCom->Change_Animation(5,  2.f, true, false); }
        else if (isRun) { if (lowerAnim != 10) m_pModelCom->Change_Animation(10, 2.f, true, false); }
        else            { if (lowerAnim != 8)  m_pModelCom->Change_Animation(8,  2.f, true, false); }
    }
    else
    {
        if (isCrouch)   { if (lowerAnim != 4) m_pModelCom->Change_Animation(4,  10.f, true, false); }
        else            { if (lowerAnim != 0) m_pModelCom->Change_Animation(0,  10.f, true, false); }
    }
}

void CPlayer_Pig::Anim_Test()
{
    if (m_pGameInstance->Key_Pressing(DIK_0))
    {
        // 정지 
        // 풀
        m_pModelCom->Change_Animation(0, 5.f, true, true);
        m_pModelCom->Change_Animation(0, 5.f, true, false);
    }
    if (m_pGameInstance->Key_Pressing(DIK_1))
    {
        // 대기
        // 풀
        m_pModelCom->Change_Animation(1, 5.f, true, true);
        m_pModelCom->Change_Animation(2, 5.f, true, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_2))
    {
        // 사격
        // 상체
        m_pModelCom->Change_Animation(3, 0.f, false , true);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_3))
    {
        // 앉기 정지
        // 하체
        m_pModelCom->Change_Animation(4, 2.f, true, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_4))
    {
        // 앉기 걷기
        // 하체
        m_pModelCom->Change_Animation(5, 1.f, true, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_5))
    {
        // 걷기
        // 하체
        m_pModelCom->Change_Animation(8, 1.f, true, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_6))
    {
        // 달리기
        // 하체
        m_pModelCom->Change_Animation(10, 0.f, true, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_7))
    {
        // 죽기
        // 풀 
        m_pModelCom->Change_Animation(9, 5.f, false, true);
        m_pModelCom->Change_Animation(9, 5.f, false, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_8))
    {
        // 장전
        // 상체
        m_pModelCom->Change_Animation(12, 3.f, false, true);
        static_cast<CKetchup_Gun*>(m_PartObjects[0])->Get_Model()->Change_Animation(2, 3.f, false);
    }
    else if (m_pGameInstance->Key_Pressing(DIK_9))
    {
        // 점프
        // 풀
        m_pModelCom->Change_Animation(11, 5.f, false, true);
        m_pModelCom->Change_Animation(11, 5.f, false, false);
    }
}

bool CPlayer_Pig::Get_WorldBoundingSphere(_float3& outCenter, _float& outRadius) const
{
    if (m_vColliderComs.empty() || !m_vColliderComs[0])
        return false;
    return m_vColliderComs[0]->Get_SphereBound(outCenter, outRadius);
}

void CPlayer_Pig::Die(_float _val)
{
    m_isDie = true;
    m_pModelCom->Change_Animation(9, 5.f, false, true);
    m_pModelCom->Change_Animation(9, 5.f, false, false);
}

void CPlayer_Pig::Revive()
{
    __super::Revive();   // m_isDie=false, m_ihealth=500
    m_isReloading    = false;
    m_bShootPending  = false;
    m_bReloadPending = false;
    m_bIsJumping     = false;
    // 상·하체 idle 애니메이션으로 복귀
    m_pModelCom->Change_Animation(0, 0.f, true, true);
    m_pModelCom->Change_Animation(0, 0.f, true, false);
}

void CPlayer_Pig::Apply_NetworkMatrix(const float* pMatrix, unsigned short keyInput)
{
    XMFLOAT4X4 newMat;
    memcpy(&newMat, pMatrix, sizeof(float) * 16);

    if (!m_drHasData)
    {
        m_drCurrentMat = newMat;
        m_drTargetMat  = newMat;
        m_drHasData    = true;
    }
    else if (m_drTimeSince > 0.001f)
    {
        float invT = 1.f / m_drTimeSince;
        m_drVelocity.x = (newMat.m[3][0] - m_drTargetMat.m[3][0]) * invT;
        m_drVelocity.y = (newMat.m[3][1] - m_drTargetMat.m[3][1]) * invT;
        m_drVelocity.z = (newMat.m[3][2] - m_drTargetMat.m[3][2]) * invT;
        m_drInterval   = m_drInterval * 0.8f + m_drTimeSince * 0.2f;
    }

    m_drTargetMat  = newMat;
    m_drTimeSince  = 0.f;

    m_prevKeyInput = m_keyInput;
    m_keyInput     = keyInput;

    // 라이징 에지 → 펜딩 플래그 설정
    unsigned short risingEdge = (~m_prevKeyInput) & m_keyInput;
    if (risingEdge & KEY_MOUSE_LB)
        m_bShootPending = true;
    if (risingEdge & KEY_R)
        m_bReloadPending = true;

    // 착지 감지: 서버 권위 Y값 기준
    if (m_bIsJumping && newMat.m[3][1] < 0.05f)
        m_bIsJumping = false;
}

void CPlayer_Pig::Get_NetworkMatrix(float* outMatrix) const
{
    memcpy(outMatrix, &m_drTargetMat, sizeof(float) * 16);
}

void CPlayer_Pig::Set_Position(const _float3& vPos)
{
    m_pTransformCom->Set_State(CTransform::STATE_POSITION,
        XMVectorSet(vPos.x, vPos.y, vPos.z, 1.f));

    // 발사/낙하 상태 초기화 (이 위치에서 새로 Launch_To를 호출할 수 있도록)
    m_bLaunching       = false;
    m_bLaunchDescending = false;
    m_bLaunchFalling   = false;
    m_vLaunchVel       = _float3(0.f, 0.f, 0.f);

    // 데드레코닝도 새 위치로 초기화 (스냅백 방지)
    XMFLOAT4X4 mat;
    XMStoreFloat4x4(&mat, m_pTransformCom->Get_WorldMatrix());
    m_drCurrentMat = mat;
    m_drTargetMat  = mat;
    m_drHasData    = false;
}

// =====================================================================
//  포물선 발사 (Launch_To / Update_Launch)
//  Player_1rd::Launch_To / Update_Launch 에서 이식.
//  카메라·사운드 블록은 제거 (원격 플레이어에는 카메라 없음).
// =====================================================================
void CPlayer_Pig::Launch_To(const _float3& vTarget, _float fArcHeight)
{
    _float3 vPos;
    XMStoreFloat3(&vPos, m_pTransformCom->Get_State(CTransform::STATE_POSITION));

    const _float g = -PIG_GRAVITY;  // 양수 중력 가속도 (크기)
    if (g <= 0.f) return;

    // 착지 목표 높이 + 정점 높이
    m_fLaunchLandY = vTarget.y;
    const _float fApexY = ((vPos.y > vTarget.y) ? vPos.y : vTarget.y) + fArcHeight;

    // 1) 정점까지 초기 상승 속도: v0y = sqrt(2g(apex - y0))
    const _float fRiseH = (fApexY > vPos.y) ? (fApexY - vPos.y) : 0.f;
    const _float v0y = sqrtf(2.f * g * fRiseH);

    // 2) 총 비행 시간 = 상승 시간 + 정점→착지 하강 시간
    const _float tUp = v0y / g;
    const _float fDropH = (fApexY > m_fLaunchLandY) ? (fApexY - m_fLaunchLandY) : 0.f;
    const _float tDown = sqrtf(2.f * fDropH / g);
    _float tTotal = tUp + tDown;
    if (tTotal < 1e-3f) tTotal = 1e-3f;

    // 3) 수평 속도 = 수평 변위 / 총 시간 (등속)
    const _float vx = (vTarget.x - vPos.x) / tTotal;
    const _float vz = (vTarget.z - vPos.z) / tTotal;

    m_vLaunchVel = _float3(vx, v0y, vz);
    m_bLaunching = true;
    m_bLaunchDescending = false;
    m_bLaunchFalling = false;
}

void CPlayer_Pig::Update_Launch(_float fTimeDelta)
{
    // 탄도 적분 + 충돌 검사
    m_vLaunchVel.y += PIG_GRAVITY * fTimeDelta;  // PIG_GRAVITY 는 음수
    if (m_vLaunchVel.y < -PIG_TERMINAL_VEL)
        m_vLaunchVel.y = -PIG_TERMINAL_VEL;
    if (m_vLaunchVel.y < 0.f)
        m_bLaunchDescending = true;

    // 이번 프레임 이동량 (충돌로 추락 중이면 수평 성분은 0)
    _float3 vMove = _float3(
        m_vLaunchVel.x * fTimeDelta,
        m_vLaunchVel.y * fTimeDelta,
        m_vLaunchVel.z * fTimeDelta);

    // ---- 충돌 검사 (콜라이더당 CheckMove) ----
    _bool bBlocked = false;
    vector<CCollider*> vHitColliders;
    {
        _vector vMoveVec = XMLoadFloat3(&vMove);
        for (CCollider* pCollider : m_vMapColliderComs)
        {
            if (pCollider == nullptr) continue;
            _float3 vOffset; XMStoreFloat3(&vOffset, vMoveVec);
            _float3 vSlide;
            if (m_pGameInstance->CheckMove(pCollider, vOffset, vSlide, &vHitColliders))
            {
                bBlocked = true;
                vMoveVec = XMLoadFloat3(&vSlide);
            }
        }
        XMStoreFloat3(&vMove, vMoveVec);
    }

    // 충돌한 부서지는 맵 처리
    for (CCollider* pHit : vHitColliders)
    {
        if (pHit == nullptr) continue;
        CMap* pMap = dynamic_cast<CMap*>(pHit->Get_Owner());
        if (pMap && pMap->Is_Breakable())
            pMap->Break();
    }

    // 비행(아치) 도중 처음으로 막히면 → 그 자리에서 수직 추락으로 전환
    if (bBlocked && !m_bLaunchFalling)
    {
        m_bLaunchFalling = true;
        m_vLaunchVel.x = 0.f;
        m_vLaunchVel.z = 0.f;
        m_bLaunchDescending = true;
    }

    _float3 vPos;
    XMStoreFloat3(&vPos, m_pTransformCom->Get_State(CTransform::STATE_POSITION));

    vPos.x += vMove.x;
    vPos.y += vMove.y;
    vPos.z += vMove.z;

    // 종료 조건:
    //  (A) 수직 추락 중 바닥에 막히면 그 자리에서 정지
    //  (B) 정상 아치 비행 시, 하강 중 목표 높이 이하로 내려오면 정지
    _bool bLanded = false;
    if (m_bLaunchFalling)
    {
        if (m_vLaunchVel.y < 0.f && vMove.y > m_vLaunchVel.y * fTimeDelta - 1e-4f)
            bLanded = true;
    }
    else if (m_bLaunchDescending && vPos.y <= m_fLaunchLandY)
    {
        vPos.y = m_fLaunchLandY;
        bLanded = true;
    }

    if (bLanded)
    {
        m_bLaunching = false;
        m_bLaunchDescending = false;
        m_bLaunchFalling = false;
        m_vLaunchVel = _float3(0.f, 0.f, 0.f);

        // 착지 시 데드레코닝 행렬 동기화 → 이후 네트워크 위치 수렴 시 튀지 않음
        XMFLOAT4X4 landedMat;
        XMStoreFloat4x4(&landedMat, m_pTransformCom->Get_WorldMatrix());
        m_drCurrentMat = landedMat;
        m_drTargetMat  = landedMat;
    }

    m_pTransformCom->Set_State(CTransform::STATE_POSITION,
        XMVectorSet(vPos.x, vPos.y, vPos.z, 1.f));
}

void CPlayer_Pig::Update_DeadReckoning(float fTimeDelta)
{
    if (!m_drHasData) return;

    m_drTimeSince += fTimeDelta;

    // 속도로 위치 외삽 (최대 2 패킷 구간까지만)
    float t = (m_drTimeSince < m_drInterval * 2.f) ? m_drTimeSince : m_drInterval * 2.f;
    XMFLOAT4X4 predicted = m_drTargetMat;
    predicted.m[3][0] += m_drVelocity.x * t;
    predicted.m[3][1] += m_drVelocity.y * t;
    predicted.m[3][2] += m_drVelocity.z * t;

    // 너무 멀면 즉시 텔레포트 (스폰 직후 등 대점프 방지)
    float dx = predicted.m[3][0] - m_drCurrentMat.m[3][0];
    float dz = predicted.m[3][2] - m_drCurrentMat.m[3][2];
    if (dx * dx + dz * dz > 9.f)
    {
        m_drCurrentMat = predicted;
    }
    else
    {
        // 지수 평활: rate가 클수록 빠르게 수렴
        const float posRate = 15.f;
        const float rotRate = 10.f;
        float posAlpha = 1.f - expf(-posRate * fTimeDelta);
        float rotAlpha = 1.f - expf(-rotRate * fTimeDelta);

        // 위치 보간 (row 3)
        for (int j = 0; j < 3; ++j)
            m_drCurrentMat.m[3][j] += (predicted.m[3][j] - m_drCurrentMat.m[3][j]) * posAlpha;

        // 회전 보간 (3x3 부분, rows 0-2 cols 0-2)
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                m_drCurrentMat.m[i][j] += (predicted.m[i][j] - m_drCurrentMat.m[i][j]) * rotAlpha;
    }

    m_pTransformCom->Set_WorldMatrix(m_drCurrentMat);
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
