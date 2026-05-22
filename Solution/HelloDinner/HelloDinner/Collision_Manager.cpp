#include "Collision_Manager.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"
#include "Map.h"


IMPLEMENT_SINGLETON(CCollision_Manager)

CCollision_Manager::CCollision_Manager(): m_pGameInstance {CGameInstance::GetInstance()}
{

}

void CCollision_Manager::Update_Collision()
{
	for (int i = 0; i < GROUP_END; ++i)
	{
		for (int j = i; j < GROUP_END; ++j)
		{
			// 초기 설정 확인
			if (!m_CollisionMatrix[i][j]) continue;

			// 충돌 처리
		}
	}
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