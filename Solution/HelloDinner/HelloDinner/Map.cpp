#include "Map.h"
#include "Transform.h"
#include "GameInstance.h"
#include "Model.h"
#include "Particle_System.h"
#include "Fracture_System.h"
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

    m_pTransformCom->EulerRotationQuaternion(pDesc->vRotation.x, pDesc->vRotation.y, pDesc->vRotation.z);

    m_pTransformCom->Set_State(CTransform::STATE_POSITION,
        XMVectorSet(pDesc->vPosition.x, pDesc->vPosition.y, pDesc->vPosition.z, 1.f));

    // Collider 정보
    m_eColliderType = pDesc->eColliderType;
    m_vCenterCollider = pDesc->vCenterCollider;
    m_vExtentsCollider = pDesc->vExtentsCollider;
    m_vRotationCollider = pDesc->vRotationCollider;
    m_fRadius = pDesc->fRadius;
    m_bBreakable = pDesc->bBreakable;
    m_iBreakPreset = pDesc->iBreakPreset;
    m_iWallId = pDesc->iWallId;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    XMStoreFloat4x4(&m_xmf4x4CachedWorld, m_pTransformCom->Get_WorldMatrix());

    // [Fracture] 부서지는 벽은 Fracture_System 에 bind 상태로 등록 -> 온전한 벽으로 전담 렌더
    if (m_bBreakable)
    {
        if (auto pFS = m_pGameInstance->Get_FractureSystem())
            m_iFractureSlot = pFS->Register(m_pModelCom, m_iWallId, m_xmf4x4CachedWorld, 0, m_pColliderCom);

        char szDbgReg[160];
        sprintf_s(szDbgReg, "[CMap] breakable wallId=%u -> fractureSlot=%d (must be >=0)\n",
            m_iWallId, m_iFractureSlot);
        OutputDebugStringA(szDbgReg);
    }

    m_pColliderCom->Set_Owner(this);

    if (m_pColliderCom)
        m_pGameInstance->Add_CollisionGroup(0, m_pColliderCom);  // 0 = GROUP_MAP


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
    if (m_bDead) return;

    if (!m_pGameInstance->Is_CullingBVHEnabled())
        Cull_And_Submit(CRenderer::RG_NONBLEND);
}

void CMap::Render(ID3D12GraphicsCommandList* _commandList)
{
    if (m_bDead) return;

    // [Fracture] 등록된 벽은 Fracture_System 이 직접 그린다(모델은 ANIM 이라 DEFAULT PSO 로 못 그림). 모델 렌더 skip.
    if (m_iFractureSlot >= 0)
    {
#ifdef _DEBUG
        m_pGameInstance->Add_RenderCollider(m_pColliderCom);
#endif
        return;
    }

    _commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &m_xmf4x4CachedWorld, 0);
    m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::DEFAULT);

    _uint iNumMeshes = m_pModelCom->Get_NumMeshes();
    for (_uint i = 0; i < iNumMeshes; ++i)
        m_pModelCom->Render(_commandList, i);

#ifdef _DEBUG
    m_pGameInstance->Add_RenderCollider(m_pColliderCom);
#endif
}

void CMap::ShadowRender(ID3D12GraphicsCommandList* _commandList)
{
    // [Fracture] 등록된 벽은 그림자 skip(ANIM 메시라 정적 그림자 PSO와 입력 레이아웃 불일치). 시스템이 전담.
    if (m_iFractureSlot >= 0) return;

    _commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &m_xmf4x4CachedWorld, 0);

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

void CMap::Break()
{
    if (m_bDead) return;

    if (CParticle_System* pPS = m_pGameInstance->Get_ParticleSystem())
    {
        const _float3 vBreakPos = {m_xmf4x4CachedWorld._41, m_xmf4x4CachedWorld._42, m_xmf4x4CachedWorld._43};

        CParticle_System::EMIT_DESC dust;     // 흙먼지(절차적, 많고 부풀어 천천히)
        dust.eType = CParticle_System::WALL_DEBRIS;
        dust.vCenter = vBreakPos;
        dust.vExtents = m_vExtentsCollider;
        pPS->Emit(dust);

        CParticle_System::EMIT_DESC debris;   // 파편(텍스처, 적고 무겁게 빠르게)
        debris.eType = CParticle_System::WALL_DEBRIS_2;
        debris.vCenter = vBreakPos;
        debris.vExtents = m_vExtentsCollider;
        pPS->Emit(debris);
    }

    // 이웃에게 내가 사라졌다는 정보 전달
    if (m_pLeftNeighbor)
        m_pLeftNeighbor->Expose_Right();
    if (m_pRightNeighbor)
        m_pRightNeighbor->Expose_Left();
    m_pLeftNeighbor = nullptr;
    m_pRightNeighbor = nullptr;

    if (m_pColliderCom)
    {
        m_pGameInstance->Delete_CollisionGroup(0, m_pColliderCom);
        m_pColliderCom->Set_Enable(false);
    }
    m_pGameInstance->Invalidate_StaticBVH();

    // [Fracture] 등록된 벽이면 SetDead 안 함(시스템이 조각을 계속 그림). 물리 파괴만 트리거.
    if (m_iFractureSlot >= 0)
    {
        OutputDebugStringA("[CMap] Break -> FRACTURE\n");
        if (auto pFS = m_pGameInstance->Get_FractureSystem())
        {
            const _float3 vBreakPoint = {m_xmf4x4CachedWorld._41, m_xmf4x4CachedWorld._42, m_xmf4x4CachedWorld._43};
            pFS->Break_ByWallId(m_iWallId, {vBreakPoint, m_iWallId /*seed*/});
        }
        m_bBroken = true;
    }
    else
    {
        OutputDebugStringA("[CMap] Break -> SetDead (not registered)\n");
        SetDead();
    }
}

void CMap::Expose_Left()
{
    m_bLeftExposed = true;
    m_pLeftNeighbor = nullptr;
#ifdef _DEBUG
    _float3 vP; XMStoreFloat3(&vP, m_pTransformCom->Get_State(CTransform::STATE_POSITION));
    char buf[128]; sprintf_s(buf, "[Wall x=%.1f] LEFT exposed\n", vP.x);
    OutputDebugStringA(buf);
#endif
}

void CMap::Expose_Right()
{
    m_bRightExposed = true;
    m_pRightNeighbor = nullptr;
#ifdef _DEBUG
    _float3 vP; XMStoreFloat3(&vP, m_pTransformCom->Get_State(CTransform::STATE_POSITION));
    char buf[128]; sprintf_s(buf, "[Wall x=%.1f] RIGHT exposed\n", vP.x);
    OutputDebugStringA(buf);
#endif
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