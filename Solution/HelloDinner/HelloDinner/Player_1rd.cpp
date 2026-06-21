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
    : CContainerObj {_pcontext}
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

    m_pMyPos = new _float3;
    m_pGameInstance->Set_PlayerPos(m_pMyPos);

    return S_OK;
}

void CPlayer_1rd::Priority_Update(_float fTimeDelta)
{
    // 포물선 비행 중에는 충돌 없는 탄도 적분만 수행(이동/중력 처리 대체).
    if (m_bLaunching)
        Update_Launch(fTimeDelta);
    else
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
    if (m_isCrouch && 11 != m_pModelCom->Get_LowerAnimNum())
        matFps *= XMMatrixTranslation(0.f, 1.f, 0.f);
    else
        matFps *= XMMatrixTranslation(0.f, 1.39f, 0.f);
    XMStoreFloat4x4(&m_matFPSModel, matFps);
    // 카메라 동기화
    m_pCamera->Set_WorldMatrix(m_matFPSModel);

    XMStoreFloat3(m_pMyPos, m_pTransformCom->Get_State(CTransform::STATE_POSITION));

    __super::Priority_Update(fTimeDelta);
}

void CPlayer_1rd::Update(_float fTimeDelta)
{
    if (!m_pModelCom->Get_UpperBlend())
    {
        if (m_pModelCom->Play_Animation(fTimeDelta, true))
        {
            if (12 == m_pModelCom->Get_UpperAnimNum())
            {
                m_isReloading = false;
                m_iAmmo = 30;
                m_pModelCom->Change_Animation(0, 0.f, true, true);
                m_pFPSModelCom->Change_Animation(0, 0.f, true);
            }
        }
    }
    else
        m_pModelCom->Blend_Animation(fTimeDelta, true);

    if (!m_pModelCom->Get_LowerBlend())
    {
        if (m_pModelCom->Play_Animation(fTimeDelta, false))
        {
            if (11 == m_pModelCom->Get_LowerAnimNum())
            {
                m_pModelCom->Change_Animation(0, 0.f, true, false);
            }
        }
    }
    else
        m_pModelCom->Blend_Animation(fTimeDelta, false);

    
    Anima();

    m_pModelCom->Merge_UpperLower();

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
    m_isCrouch = false;
    m_isRun = false;
    m_isMove = false;
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
    // 비행(포물선) 중에는 이동 입력을 무시한다. (시점 회전은 허용)
    if (m_bLaunching)
        return;

    // 실제 이동/충돌은 Resolve_Movement()에서 한 번에 처리.
    // 여기서는 이번 프레임 입력만 누적한다.
    m_fMoveLook = _fLook;
    m_fMoveRight = _fRight;
    if (m_fMoveLook)
        m_isMove = true;
}

void CPlayer_1rd::Resolve_Movement(_float fTimeDelta)
{
    // ---- 1) 수평 이동 벡터 (입력 기반) ----
    _float val = 1.f;
    if (m_isCrouch)
        val = 0.5f;
    else if (m_isRun)
        val = 1.5f;
    _vector vHorizontal = XMVectorZero();
    {
        _vector vLook = XMVectorSetY(m_pTransformCom->Get_State(CTransform::STATE_LOOK), 0.f);
        _vector vRight = XMVectorSetY(m_pTransformCom->Get_State(CTransform::STATE_RIGHT), 0.f);

        _vector vDir = XMVector3Normalize(vLook) * m_fMoveLook
            + XMVector3Normalize(vRight) * m_fMoveRight;

        if (XMVectorGetX(XMVector3LengthSq(vDir)) > 1e-6f)
        {
            vDir = XMVector3Normalize(vDir) * val;
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
    if (m_fMoveLook)
        m_fMoveLook = 0.f;
    m_fMoveLook = 0.f;
    m_fMoveRight = 0.f;
}

void CPlayer_1rd::Anima()
{
    _uint anim = m_pModelCom->Get_LowerAnimNum();
    if (anim != 11)
    {
        if (m_isMove)
        {
            if (m_isCrouch)
            {
                if (5 != anim)
                    m_pModelCom->Change_Animation(5, 2.f, true, false);
            }
            else if (m_isRun)
            {
                if (10 != anim)
                    m_pModelCom->Change_Animation(10, 2.f, true, false);
            }
            else
            {
                if (8 != anim)
                    m_pModelCom->Change_Animation(8, 2.f, true, false);
            }
        }
        else
        {
            if (m_isCrouch)
            {
                if (4 != anim)
                    m_pModelCom->Change_Animation(4, 10.f, true, false);
            }
            else
            {
                if (0 != anim)
                    m_pModelCom->Change_Animation(0, 10.f, true, false);
            }
        }
    }
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
        m_isCrouch = false;
        m_pModelCom->Change_Animation(11, 0.f, false, false);
    }
}

// =====================================================================
//  포물선 낙하(스폰)
//   - 목표(vTarget)까지의 수평 거리를, "정점까지 솟구쳤다 떨어지는" 총 비행
//     시간 동안 등속으로 이동하도록 수평 속도를 잡는다.
//   - 정점 높이 = max(현재y, 목표y) + fArcHeight.
//   - 착지 기준 y(fLandY) = 목표 높이. 하강 중 이 값 이하로 내려오면 멈춤.
// =====================================================================
void CPlayer_1rd::Launch_To(const _float3& vTarget, _float fArcHeight)
{
    _float3 vPos;
    XMStoreFloat3(&vPos, m_pTransformCom->Get_State(CTransform::STATE_POSITION));

    const _float g = -GRAVITY;                 // 양수 중력 가속도(크기)
    if (g <= 0.f)
        return;

    // 착지 목표 높이 + 정점 높이
    m_fLaunchLandY = vTarget.y;
    const _float fApexY = ((vPos.y > vTarget.y) ? vPos.y : vTarget.y) + fArcHeight;

    // 1) 정점까지 필요한 초기 상승 속도:  v0y = sqrt(2 g (apex - y0))
    const _float fRiseH = (fApexY > vPos.y) ? (fApexY - vPos.y) : 0.f;
    const _float v0y = sqrtf(2.f * g * fRiseH);

    // 2) 상승 시간 + 정점에서 착지높이까지의 하강 시간 = 총 비행 시간
    const _float tUp = v0y / g;
    const _float fDropH = (fApexY > m_fLaunchLandY) ? (fApexY - m_fLaunchLandY) : 0.f;
    const _float tDown = sqrtf(2.f * fDropH / g);
    _float tTotal = tUp + tDown;
    if (tTotal < 1e-3f)
        tTotal = 1e-3f; // 0 나눗셈 방지

    // 3) 수평 속도 = 수평 변위 / 총 시간 (등속)
    const _float vx = (vTarget.x - vPos.x) / tTotal;
    const _float vz = (vTarget.z - vPos.z) / tTotal;

    m_vLaunchVel = _float3(vx, v0y, vz);
    m_bLaunching = true;
    m_bLaunchDescending = false;

    // 비행 시작: 점프/지면 상태 초기화 + 기존 수직속도 무시
    m_bIsGrounded = false;
    m_fVerticalVelocity = 0.f;
    m_fMoveLook = 0.f;
    m_fMoveRight = 0.f;
}

void CPlayer_1rd::Set_Position(const _float3& vPos)
{
    // 즉시 텔레포트. 비행/이동 상태를 정지시켜 다음 프레임에 끌려가지 않게 한다.
    m_pTransformCom->Set_State(CTransform::STATE_POSITION,
        XMVectorSet(vPos.x, vPos.y, vPos.z, 1.f));

    m_bLaunching = false;
    m_bLaunchDescending = false;
    m_vLaunchVel = _float3(0.f, 0.f, 0.f);
    m_fVerticalVelocity = 0.f;
    m_fMoveLook = 0.f;
    m_fMoveRight = 0.f;
    m_bIsGrounded = true;
}

void CPlayer_1rd::Update_Launch(_float fTimeDelta)
{
    // 충돌 없는 단순 탄도 적분.
    m_vLaunchVel.y += GRAVITY * fTimeDelta; // GRAVITY 는 음수
    if (m_vLaunchVel.y < 0.f)
        m_bLaunchDescending = true;         // 정점 지나 하강 시작

    _float3 vPos;
    XMStoreFloat3(&vPos, m_pTransformCom->Get_State(CTransform::STATE_POSITION));

    vPos.x += m_vLaunchVel.x * fTimeDelta;
    vPos.y += m_vLaunchVel.y * fTimeDelta;
    vPos.z += m_vLaunchVel.z * fTimeDelta;

    // 하강 중 목표 높이 이하로 내려오면 그 지점에서 착지 종료.
    if (m_bLaunchDescending && vPos.y <= m_fLaunchLandY)
    {
        vPos.y = m_fLaunchLandY;     // 미리 정한 y 에 도달하면 멈춤
        m_bLaunching = false;
        m_bLaunchDescending = false;
        m_vLaunchVel = _float3(0.f, 0.f, 0.f);
        m_fVerticalVelocity = 0.f;
        m_bIsGrounded = true;        // 착지 후 정상 이동/중력 재개
    }

    m_pTransformCom->Set_State(CTransform::STATE_POSITION,
        XMVectorSet(vPos.x, vPos.y, vPos.z, 1.f));
}

void CPlayer_1rd::Crouch(_float _val)
{
    // 웅크리기
    _uint anim = m_pModelCom->Get_LowerAnimNum();
    if (11 != anim)
    {
        m_isCrouch = true;
    }
}

void CPlayer_1rd::Run(_float _val)
{
    if (!m_isCrouch)
    {
        m_isRun = true;
    }
}

void CPlayer_1rd::Reload(_float _val)
{
    if (!m_isReloading)
    {
        m_isReloading = true;
        m_pModelCom->Change_Animation(12, 3.f, false, true);
        m_pFPSModelCom->Change_Animation(7, 3.f, false);
        static_cast<CKetchup_Gun*>(m_PartObjects[0])->Get_Model()->Change_Animation(2, 3.f, false);
        static_cast<CKetchup_Gun*>(m_PartObjects[1])->Get_Model()->Change_Animation(2, 3.f, false);
    }
}

_bool CPlayer_1rd::Shoot(_float _val)
{
    if (m_iAmmo && !m_isReloading)
    {
        m_pModelCom->Change_Animation(3, 0.f, false, true);
        m_pFPSModelCom->Change_Animation(2, 0.f, false);
        --m_iAmmo;

        _vector look = m_pTransformCom->Get_State(CTransform::STATE_LOOK);
        _vector pos = m_pTransformCom->Get_State(CTransform::STATE_POSITION);

        int ran = (int)m_pGameInstance->Compute_Random(1.f, 2.99f);
        _float3 posi;
        XMStoreFloat3(&posi, pos);
        string sound = "attack" + to_string(ran);
        m_pGameInstance->PlaySounds(sound);
        m_pGameInstance->UpdateSoundVolumeByPlayer(sound, posi);

        m_pGameInstance->NewBullet(look, pos, this);

        return true;
    }
    return false;
}

void CPlayer_1rd::Die(_float _val)
{
    m_isDie = true;
    m_pModelCom->Change_Animation(9, 10.f, false, true);
    m_pModelCom->Change_Animation(9, 10.f, false, false);
}

void CPlayer_1rd::Set_Weapon(_int iIndex)
{
    // 유효 범위(현재 0:케첩, 1:마요)만 처리
    if (iIndex < 0 || iIndex >= (_int)m_PartObjects.size())
        return;
    if (iIndex == m_iWeapon)
        return; // 이미 장착 중이면 무시

    // 모든 무기 파트 Off 후, 선택한 무기만 On
    for (_int i = 0; i < (_int)m_PartObjects.size(); ++i)
    {
        if (m_PartObjects[i] != nullptr)
            m_PartObjects[i]->SetOnOff(i == iIndex);
    }

    m_iWeapon = iIndex;
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

    m_pModelCom->Set_SpineBoneIndex(m_pModelCom->Get_BoneIndex("spine"));

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