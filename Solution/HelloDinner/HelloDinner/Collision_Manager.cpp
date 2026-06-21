#include "Collision_Manager.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"
#include "Map.h"
#include "Bullets.h"
#include "Particle_System.h"

IMPLEMENT_SINGLETON(CCollision_Manager)

CCollision_Manager::CCollision_Manager(): m_pGameInstance {CGameInstance::GetInstance()}
{

}

void CCollision_Manager::Update_Collision()
{
	//for (int i = 0; i < GROUP_END; ++i)
	//{
	//	for (int j = i; j < GROUP_END; ++j)
	//	{
	//		// 초기 설정 확인
	//		if (!m_CollisionMatrix[i][j]) continue;

	//		// 충돌 처리
	//	}
	//}
}

void CCollision_Manager::Clear_CollisionGroup()
{
    for (_int i = 0; i < GROUP_END; ++i) {
        if (i == GROUP_MAP) continue;   // static은 보존 (BVH때문)
        m_Colliders[i].clear();
    }
}

void CCollision_Manager::Set_CollisionMatrix(COLLISION_GROUP _lgroup, COLLISION_GROUP _rgroup, _bool _is)
{
	// 충돌 규칙 설정 (왼쪽 그룹이 오른쪽 그룹과 충돌처리 여부)
	// Ex) Set_CollisionMatrix(GROUP_PLAYER, GROUP_MAP, true); // 플레이어는 맵과 충돌처리한다.
	m_CollisionMatrix[_lgroup][_rgroup] = _is;
}

void CCollision_Manager::Add_CollisionGroup(COLLISION_GROUP _eGroup, CCollider* _pCollider)
{
    m_Colliders[_eGroup].push_back(_pCollider);
    if (_eGroup == GROUP_MAP) m_bStaticBVHDirty = true;
}

void CCollision_Manager::Delete_CollisionGroup(COLLISION_GROUP _eGroup, CCollider* _pCollider)
{
	auto& vec = m_Colliders[_eGroup];
	for (size_t i = 0; i < vec.size(); ++i)
	{
		if (vec[i] == _pCollider)
		{
			vec[i] = vec.back();
			vec.pop_back();
			break;
		}
	}
}

vector<class CCollider*> CCollision_Manager::CollisionCheck_with_Group(CCollider* _pCollider, COLLISION_GROUP _eGroup)
{
	vector<class CCollider*> re;
	for (CCollider* collider : m_Colliders[_eGroup])
	{
		// 충돌 처리 걸린것들 pushback
	}

	return re;
}

bool CCollision_Manager::CollisionCheck_with_Collider(CCollider* _pMyCollider, CCollider* _pOtherCollider)
{
	return false;
}

bool CCollision_Manager::CheckMove(CCollider* me, const XMFLOAT3& move, XMFLOAT3& outSlide)
{
    return CheckMove(me, move, outSlide, nullptr);
}

bool CCollision_Manager::CheckMove(CCollider* me, const XMFLOAT3& move, XMFLOAT3& outSlide, vector<CCollider*>* outHits)
{
    // 첫 호출 시 자동 빌드 (또는 invalidate 후 재빌드)
    if (m_bStaticBVHDirty && !m_Colliders[GROUP_MAP].empty())
        Build_StaticBVH();

    bool bHit = false;
    XMFLOAT3 finalMove = move;
    const int MAX_ITER = 4;

    // BVH 쿼리로 후보 추출 (fallback: 전체 GROUP_MAP)
    vector<CCollider*> queryResults;
    vector<CCollider*>* pCandidates = nullptr;

    if (m_bBVHEnabled && m_StaticBVH.Is_Built()) {
        _float3 myC; _float myR;
        if (me->Get_SphereBound(myC, myR)) {
            XMVECTOR vMoveLen = XMVector3Length(XMLoadFloat3(&move));
            _float fMoveDist = XMVectorGetX(vMoveLen);
            _float fExpandR = myR + fMoveDist * (MAX_ITER + 1);   // 슬라이드 반복 흡수
            m_StaticBVH.Query_Sphere(myC, fExpandR, queryResults);
            pCandidates = &queryResults;
            m_iLastQueryCandidates = (_int)queryResults.size();
        }
    }
    if (!pCandidates) {
        pCandidates = &m_Colliders[GROUP_MAP];
        m_iLastQueryCandidates = (_int)pCandidates->size();
    }

    for (int iter = 0; iter < MAX_ITER; ++iter) {
        bool bAnyHitThisIter = false;
        for (CCollider* other : *pCandidates) {
            if (!other->Get_Enable()) continue;
            if (IsCollidingAfterMove(me, other, finalMove)) {
                bHit = true;
                bAnyHitThisIter = true;

                if (outHits) {
                    bool bDup = false;
                    for (CCollider* h : *outHits) { if (h == other) { bDup = true; break; } }
                    if (!bDup) outHits->push_back(other);
                }

                XMFLOAT3 normal = me->Get_CollisionNormal(other);
                finalMove = Slide(finalMove, normal);
            }
        }
        if (!bAnyHitThisIter) break;

        XMVECTOR vLenSq = XMVector3LengthSq(XMLoadFloat3(&finalMove));
        if (XMVectorGetX(vLenSq) < 1e-8f) { finalMove = {0, 0, 0}; break; }
    }
    outSlide = finalMove;
    return bHit;
}

CCollision_Manager::RAY_HIT CCollision_Manager::RayCast(_vector rayPos, _vector rayDir, COLLISION_GROUP group, CGameObject* pShooter)
{
    RAY_HIT result;

    rayDir = XMVector3Normalize(rayDir);

    for (auto collider : m_Colliders[group])
    {
        if (!collider->Get_Enable())
            continue;

        if (pShooter != nullptr && collider->Get_Owner() == pShooter)
        {
            continue;
        }

        float dist;

        if (collider->IntersectsRay(rayPos, rayDir, dist))
        {
            if (dist < result.distance)
            {
                result.hit = true;
                result.distance = dist;
                result.pCollider = collider;
                result.pTarget = collider->Get_Owner();
            }
        }
    }

    return result;
}

void CCollision_Manager::Set_Bullets(CBullets* _p)
{
    m_pBullets = _p;
}

void CCollision_Manager::NewBullet(_vector _look, _vector _pos, CGameObject* pShooter)
{
    CBullets::BULLET b;

    b.vLook = XMVector3Normalize(_look);
    b.vPos = _pos;
    b.fLifeTime = 1.f;

    RAY_HIT playerHit =
        RayCast(b.vPos, b.vLook, GROUP_PLAYER, pShooter);

    RAY_HIT mapHit =
        RayCast(b.vPos, b.vLook, GROUP_MAP);

    if (mapHit.hit &&
        (!playerHit.hit || mapHit.distance < playerHit.distance))
    {
        b.vTarget =
            XMVectorMultiplyAdd(
                b.vLook,
                XMVectorReplicate(mapHit.distance),
                b.vPos);
        b.isHit = true;
        b.isOn = true;

        // 파티클 생성 부분
        if (CParticle_System* pPS = m_pGameInstance->Get_ParticleSystem())
        {
            _float3 vHitPos;
            XMStoreFloat3(&vHitPos, b.vTarget);   // _vector -> _float3

            // 흙먼지(절차적) : 맵 파괴와 같은 WALL_DEBRIS
            CParticle_System::EMIT_DESC dust;
            dust.eType = CParticle_System::WALL_DEBRIS;
            dust.vCenter = vHitPos;
            dust.vExtents = {0.15f, 0.15f, 0.15f};  // 점 충돌이라 좁게
            dust.iCount = 12;                        // 파괴보다 적게
            pPS->Emit(dust);

            // 파편(텍스처) : 맵 파괴와 같은 WALL_DEBRIS_2
            CParticle_System::EMIT_DESC debris;
            debris.eType = CParticle_System::WALL_DEBRIS_2;
            debris.vCenter = vHitPos;
            debris.vExtents = {0.15f, 0.15f, 0.15f};
            debris.iCount = 6;
            pPS->Emit(debris);
        }
        //
    }
    else if (playerHit.hit)
    {
        b.vTarget =
            XMVectorMultiplyAdd(
                b.vLook,
                XMVectorReplicate(playerHit.distance),
                b.vPos);
        b.isHit = true;
        b.isOn = true;

        switch (playerHit.pCollider->Get_PartNum())
        {
        case 0: // COLLIDER_HEAD
            playerHit.pTarget->TakeDamage(50);
            break;
        case 1: // COLLIDER_ARM_UP_L
            playerHit.pTarget->TakeDamage(20);
            break;
        case 2: // COLLIDER_ARM_UP_R
            playerHit.pTarget->TakeDamage(20);
            break;
        case 3: // COLLIDER_ARM_LOW_L
            playerHit.pTarget->TakeDamage(20);
            break;
        case 4: // COLLIDER_ARM_LOW_R
            playerHit.pTarget->TakeDamage(20);
            break;
        case 5: // COLLIDER_THIGH_L
            playerHit.pTarget->TakeDamage(15);
            break;
        case 6: // COLLIDER_THIGH_R
            playerHit.pTarget->TakeDamage(15);
            break;
        case 7: // COLLIDER_SHIN_L
            playerHit.pTarget->TakeDamage(15);
            break;
        case 8: // COLLIDER_SHIN_R
            playerHit.pTarget->TakeDamage(15);
            break;
        case 9: // COLLIDER_BODY
            playerHit.pTarget->TakeDamage(30);
            break;
        }
        // 파티클 생성 부분
        if (CParticle_System* pPS = m_pGameInstance->Get_ParticleSystem())
        {
            _float3 vHitPos;
            XMStoreFloat3(&vHitPos, b.vTarget);   // _vector -> _float3

            // 흙먼지(절차적) : 맵 파괴와 같은 WALL_DEBRIS
            CParticle_System::EMIT_DESC dust;
            dust.eType = CParticle_System::WALL_DEBRIS;
            dust.vCenter = vHitPos;
            dust.vExtents = {0.15f, 0.15f, 0.15f};  // 점 충돌이라 좁게
            dust.iCount = 12;                        // 파괴보다 적게
            pPS->Emit(dust);

            // 파편(텍스처) : 맵 파괴와 같은 WALL_DEBRIS_2
            CParticle_System::EMIT_DESC debris;
            debris.eType = CParticle_System::WALL_DEBRIS_2;
            debris.vCenter = vHitPos;
            debris.vExtents = {0.15f, 0.15f, 0.15f};
            debris.iCount = 6;
            pPS->Emit(debris);
        }
        //
    }

    m_pBullets->NewBullet(b);
}

bool CCollision_Manager::IsCollidingAfterMove(CCollider* me, CCollider* other, const XMFLOAT3& move)
{
	XMFLOAT3 reverseMove = {-move.x, -move.y, -move.z};

	// other 충돌체를 임시로 reverseMove 만큼 옮긴 상태로 me와 겹치는지 검사
	return me->Intersect_Offset(other, reverseMove);
}

XMFLOAT3 CCollision_Manager::Slide(const XMFLOAT3& move, const XMFLOAT3& normal)
{
	XMVECTOR vM = XMLoadFloat3(&move);
	XMVECTOR vN = XMLoadFloat3(&normal);

	XMVECTOR dot = XMVector3Dot(vM, vN);
	XMVECTOR slide = vM - dot * vN;

	XMFLOAT3 result;
	XMStoreFloat3(&result, slide);
	return result;
}

XMFLOAT3 CCollision_Manager::GetCollisionNormal(CCollider* me, CCollider* other)
{
	return XMFLOAT3 {0.f, 1.f, 0.f};
}

void CCollision_Manager::Build_StaticBVH()
{
    m_StaticBVH.Build(m_Colliders[GROUP_MAP]);
    m_bStaticBVHDirty = false;
}

void CCollision_Manager::Cull_StaticBVH(const BoundingFrustum* pMainFrustum, const BoundingSphere* pShadowBounds)
{
    if (!m_bCullingBVHEnabled) return;          // OFF: CMap이 자기 Cull_And_Submit 처리
    if (m_Colliders[GROUP_MAP].empty()) return;

    if (m_bStaticBVHDirty)
        Build_StaticBVH();
    if (!m_StaticBVH.Is_Built()) return;

    const _int iTotal = (_int)m_Colliders[GROUP_MAP].size();

    _bool bMainCullOn = m_pGameInstance->Is_CullingEnabled() && pMainFrustum;
    _bool bShadowCullOn = m_pGameInstance->Is_ShadowCullingEnabled() && pShadowBounds;

    // [Main]
    if (bMainCullOn) {
        vector<CCollider*> results;
        m_StaticBVH.Query_Frustum(*pMainFrustum, results);
        m_iLastFrustumCandidates = (_int)results.size();
        for (CCollider* p : results) {
            CGameObject* owner = p ? p->Get_Owner() : nullptr;
            if (owner) {
                if (m_pGameInstance->Is_InstancingEnabled()) {
                    CMap* pMap = dynamic_cast<CMap*>(owner);
                    if (pMap) m_pGameInstance->Add_InstancedRenderObject(pMap->Get_ModelTag(), owner);
                    else      m_pGameInstance->Add_RenderObject(CRenderer::RG_NONBLEND, owner);
                }
                else {
                    m_pGameInstance->Add_RenderObject(CRenderer::RG_NONBLEND, owner);
                }
            }
        }
        m_pGameInstance->Add_CullStat_Main_Bulk((_int)results.size(), iTotal);
    }
    else {
        for (CCollider* p : m_Colliders[GROUP_MAP]) {
            CGameObject* owner = p ? p->Get_Owner() : nullptr;
            if (owner) {
                if (m_pGameInstance->Is_InstancingEnabled()) {
                    CMap* pMap = dynamic_cast<CMap*>(owner);
                    if (pMap) m_pGameInstance->Add_InstancedRenderObject(pMap->Get_ModelTag(), owner);
                    else      m_pGameInstance->Add_RenderObject(CRenderer::RG_NONBLEND, owner);
                }
                else {
                    m_pGameInstance->Add_RenderObject(CRenderer::RG_NONBLEND, owner);
                }
            }
        }
        m_pGameInstance->Add_CullStat_Main_Bulk(iTotal, iTotal);
        m_iLastFrustumCandidates = iTotal;
    }

    // [Shadow]
    if (bShadowCullOn) {
        vector<CCollider*> results;
        m_StaticBVH.Query_Sphere(pShadowBounds->Center, pShadowBounds->Radius, results);
        m_iLastShadowCandidates = (_int)results.size();
        for (CCollider* p : results) {
            CGameObject* owner = p ? p->Get_Owner() : nullptr;
            if (owner) {
                if (m_pGameInstance->Is_InstancingEnabled()) {
                    CMap* pMap = dynamic_cast<CMap*>(owner);
                    if (pMap) m_pGameInstance->Add_ShadowInstancedRenderObject(pMap->Get_ModelTag(), owner);
                    else      m_pGameInstance->Add_ShadowRenderObject(CRenderer::RG_NONBLEND, owner);
                }
                else {
                    m_pGameInstance->Add_ShadowRenderObject(CRenderer::RG_NONBLEND, owner);
                }
            }
        }
        m_pGameInstance->Add_CullStat_Shadow_Bulk((_int)results.size(), iTotal);
    }
    else {
        for (CCollider* p : m_Colliders[GROUP_MAP]) {
            CGameObject* owner = p ? p->Get_Owner() : nullptr;
            if (owner) {
                if (m_pGameInstance->Is_InstancingEnabled()) {
                    CMap* pMap = dynamic_cast<CMap*>(owner);
                    if (pMap) m_pGameInstance->Add_ShadowInstancedRenderObject(pMap->Get_ModelTag(), owner);
                    else      m_pGameInstance->Add_ShadowRenderObject(CRenderer::RG_NONBLEND, owner);
                }
                else {
                    m_pGameInstance->Add_ShadowRenderObject(CRenderer::RG_NONBLEND, owner);
                }
            }
        }
        m_pGameInstance->Add_CullStat_Shadow_Bulk(iTotal, iTotal);
        m_iLastShadowCandidates = iTotal;
    }
}

CCollision_Manager* CCollision_Manager::Create()
{
	return new CCollision_Manager();
}

void CCollision_Manager::Free()
{
    m_StaticBVH.Clear();
    Clear_CollisionGroup();
    m_Colliders[GROUP_MAP].clear();
}