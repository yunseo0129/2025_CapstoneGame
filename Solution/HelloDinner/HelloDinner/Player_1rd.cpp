#include "Player_1rd.h"
#include "GameInstance.h"
#include "Transform.h"
#include "Ketchup_Gun.h"
#include "Bounding_AABB.h"
#include "Bounding_Sphere.h"
#include "Bounding_OBB.h"
#include "Camera_FPV.h"
#include "Map.h"
#include "Collider.h"

namespace {
    constexpr float GRAVITY = -9.8f;  // 중력 가속도 (units/s^2)
    constexpr float JUMP_SPEED = 4.5f;   // 점프 초기 수직 속도
    constexpr float TERMINAL_VEL = 20.f;   // 최대 낙하 속도(터널링 방지)
}

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
    pDesc->fSpeedPerSec = 1.f;
    pDesc->fRotationPerSec = 1.f;
    pDesc->fSpeedPerSec = 1.f;
    m_pCamera = pDesc->pCamera;
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    m_pTransformCom->Scaling(1.f, 1.f, 1.f);
    m_pTransformCom->RotationQuaternion(pDesc->vRotation.x, pDesc->vRotation.y, pDesc->vRotation.z);
    m_pTransformCom->Set_State(CTransform::STATE_POSITION,
        XMVectorSet(pDesc->vPos.x, pDesc->vPos.y, pDesc->vPos.z, 1.f));

    XMStoreFloat4x4(&m_matFPSModel, m_pTransformCom->Get_WorldMatrix());

    m_pCamera->Set_WorldMatrix(m_matFPSModel);

    if (FAILED(Ready_Components()))
        return E_FAIL;

    m_iState = 0;
    m_pModelCom->SetUp_Animation(0, true);
    m_pFPSModelCom->SetUp_Animation(0, true);

    if (FAILED(Ready_PartObjects()))
        return E_FAIL;

    for (CCollider* p : m_vColliderComs)
        if (p) p->Set_Owner(this);
    for (CCollider* p : m_vMapColliderComs)
        if (p) p->Set_Owner(this);

    return S_OK;
}

void CPlayer_1rd::Priority_Update(_float fTimeDelta)
{
    Resolve_Movement(fTimeDelta);

    // 1인칭 모델 동기화
    XMMATRIX matFps = m_pTransformCom->Get_WorldMatrix();
    _vector		vRight = matFps.r[0];
    _vector		vUp = matFps.r[1];
    _vector		vLook = matFps.r[2];
    _matrix		RotationMatrix = XMMatrixRotationAxis(vRight, m_fPitchRot);
    matFps.r[0] = XMVector3TransformNormal(vRight, RotationMatrix);
    matFps.r[1] = XMVector3TransformNormal(vUp, RotationMatrix);
    matFps.r[2] = XMVector3TransformNormal(vLook, RotationMatrix);
    matFps *= XMMatrixTranslation(0.f, 1.39f, 0.f);
    XMStoreFloat4x4(&m_matFPSModel, matFps);
    // 카메라 동기화
    m_pCamera->Set_WorldMatrix(m_matFPSModel);

    __super::Priority_Update(fTimeDelta);
}

void CPlayer_1rd::Update(_float fTimeDelta)
{
    m_pModelCom->Play_Animation(fTimeDelta);
    m_pFPSModelCom->Play_Animation(fTimeDelta);

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

#ifdef _DEBUG
    //{
    //    XMFLOAT3 vP; XMStoreFloat3(&vP, m_pTransformCom->Get_State(CTransform::STATE_POSITION));
    //    auto* pS0 = m_vMapColliderComs[0] ? static_cast<const CBounding_Sphere*>(m_vMapColliderComs[0]->Get_Bounding())->Get_Desc() : nullptr;
    //    auto* pS1 = m_vMapColliderComs[1] ? static_cast<const CBounding_Sphere*>(m_vMapColliderComs[1]->Get_Bounding())->Get_Desc() : nullptr;
    //    char buf[256];
    //    sprintf_s(buf, "Player y=%.2f | Foot center=(%.2f, %.2f, %.2f) | Body center=(%.2f, %.2f, %.2f)\n",
    //        vP.y,
    //        pS0 ? pS0->Center.x : 0, pS0 ? pS0->Center.y : 0, pS0 ? pS0->Center.z : 0,
    //        pS1 ? pS1->Center.x : 0, pS1 ? pS1->Center.y : 0, pS1 ? pS1->Center.z : 0);
    //    OutputDebugStringA(buf);
    //}
#endif

    __super::Update(fTimeDelta);
}

void CPlayer_1rd::Late_Update(_float fTimeDelta)
{
    Cull_And_Submit(CRenderer::RG_NONBLEND);
    __super::Late_Update(fTimeDelta);
}

void CPlayer_1rd::Render(ID3D12GraphicsCommandList* _commandList)
{
    _commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &m_matFPSModel, 0);

    // 2. PSO 설정
    m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::ANIM);

    // 3. 메쉬별 렌더링 (머티리얼 바인딩 + DrawIndexedInstanced)
    _uint iNumMeshes = m_pFPSModelCom->Get_NumMeshes();
    for (_uint i = 0; i < iNumMeshes; ++i)
    {
        m_pFPSModelCom->Bind_BoneMatrices(_commandList, i);
        m_pFPSModelCom->Render(_commandList, i);
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

void CPlayer_1rd::ShadowRender(ID3D12GraphicsCommandList* _commandList)
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

void CPlayer_1rd::Move(_float _fLook, _float _fRight, _float _val)
{
    // 실제 이동/충돌은 Resolve_Movement()에서 한 번에 처리.
    // 여기서는 이번 프레임 입력만 누적한다.
    m_fMoveLook = _fLook;
    m_fMoveRight = _fRight;
}

void CPlayer_1rd::Resolve_Movement(_float fTimeDelta)
{
    // ---- 1) 수평 이동 벡터 (입력 기반) ----
    _vector vHorizontal = XMVectorZero();
    {
        _vector vLook = XMVectorSetY(m_pTransformCom->Get_State(CTransform::STATE_LOOK), 0.f);
        _vector vRight = XMVectorSetY(m_pTransformCom->Get_State(CTransform::STATE_RIGHT), 0.f);

        _vector vDir = XMVector3Normalize(vLook) * m_fMoveLook
            + XMVector3Normalize(vRight) * m_fMoveRight;

        if (XMVectorGetX(XMVector3LengthSq(vDir)) > 1e-6f)
        {
            vDir = XMVector3Normalize(vDir);
            _float fDist = m_pTransformCom->Get_SpeedPerSec() * fTimeDelta;
            vHorizontal = vDir * fDist;
        }
    }

    // ---- 2) 수직 이동 벡터 (중력) ----
    m_fVerticalVelocity += GRAVITY * fTimeDelta;
    if (m_fVerticalVelocity < -TERMINAL_VEL)
        m_fVerticalVelocity = -TERMINAL_VEL;
    _float fDeltaY = m_fVerticalVelocity * fTimeDelta;

    // ---- 3) 합성: 수평 + 수직 ----
    _vector vMove = vHorizontal + XMVectorSet(0.f, fDeltaY, 0.f, 0.f);

    // ---- 4) 콜라이더당 CheckMove 한 번씩 (슬라이드 누적) ----
    vector<CCollider*> vHitColliders;
    for (CCollider* pCollider : m_vMapColliderComs)
    {
        if (pCollider == nullptr) continue;

        _float3 vOffset; XMStoreFloat3(&vOffset, vMove);
        _float3 vSlide;
        if (m_pGameInstance->CheckMove(pCollider, vOffset, vSlide, &vHitColliders))
            vMove = XMLoadFloat3(&vSlide);
    }

    for (CCollider* pHit : vHitColliders)
    {
        if (pHit == nullptr) continue;
        CMap* pMap = dynamic_cast<CMap*>(pHit->Get_Owner());
        if (pMap && pMap->Is_Breakable())
            pMap->Break();
    }

    // ---- 5) 수직 충돌 판정 (합성 결과의 Y 성분으로) ----
    _float fResolvedY = XMVectorGetY(vMove);
    m_bIsGrounded = false;

    if (fDeltaY < -1e-5f && fResolvedY > fDeltaY + 1e-4f)
    {
        // 내려가려 했는데 막힘 → 바닥 착지
        m_bIsGrounded = true;
        m_fVerticalVelocity = 0.f;
    }
    else if (fDeltaY > 1e-5f && fResolvedY < fDeltaY - 1e-4f)
    {
        // 올라가려 했는데 막힘 → 천장
        m_fVerticalVelocity = 0.f;
    }

    // ---- 6) 위치 적용 (한 번) ----
    _vector vPos = m_pTransformCom->Get_State(CTransform::STATE_POSITION);
    vPos = XMVectorAdd(vPos, vMove);
    m_pTransformCom->Set_State(CTransform::STATE_POSITION, vPos);

    // ---- 7) 입력 소비 ----
    m_fMoveLook = 0.f;
    m_fMoveRight = 0.f;
}

void CPlayer_1rd::TurnYaw(_float _val)
{
    m_pTransformCom->Turn(XMVectorSet(0.f, 1.f, 0.f, 0.f), _val);
}

void CPlayer_1rd::TurnPitch(_float _val)
{
    m_fPitchRot += _val;
}

void CPlayer_1rd::Jump(_float _val)
{
    if (m_bIsGrounded)
    {
        m_fVerticalVelocity = JUMP_SPEED;
        m_bIsGrounded = false;
    }
}

void CPlayer_1rd::Crouch(_float _val)
{
    // 웅크리기
}

HRESULT CPlayer_1rd::Ready_PartObjects()
{
    // 케첩건
    {
        CKetchup_Gun::KETCHUP_GUN_DESC cdesc;
        cdesc.strModelTag = L"Prototype_Component_ketchupGun";
        cdesc.iModelLevelIndex = m_iModelLevelIndex;
        cdesc.pParentMatrix = &m_matFPSModel;
        cdesc.pSocketMatrix = m_pFPSModelCom->Get_BoneMatrix("weapon.R");
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
        cdesc.pParentMatrix = &m_matFPSModel;
        cdesc.pSocketMatrix = m_pFPSModelCom->Get_BoneMatrix("weapon.R");
        cdesc.vScale = _float3(1.f, 1.f, 1.f);
        m_PartObjects.push_back(static_cast<CPartObj*>(m_pGameInstance->Clone_Prototype(Engine::PROTOTYPE::PROTO_GAMEOBJ, m_iModelLevelIndex, TEXT("Prototype_GameObject_Ketchup_Gun"), &cdesc)));
        if (nullptr == m_PartObjects[1])
            return E_FAIL;

        m_PartObjects[1]->SetOnOff(false);
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

    // FPS Model 컴포넌트 생성
    if (FAILED(Add_Component(m_iModelLevelIndex, TEXT("Prototype_Component_Player_Pig_fps"),
        TEXT("Com_FPSModel"), reinterpret_cast<CComponent**>(&m_pFPSModelCom))))
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

bool CPlayer_1rd::Get_WorldBoundingSphere(_float3& outCenter, _float& outRadius) const
{
    if (m_vColliderComs.empty() || !m_vColliderComs[0])
        return false;
    return m_vColliderComs[0]->Get_SphereBound(outCenter, outRadius);
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
