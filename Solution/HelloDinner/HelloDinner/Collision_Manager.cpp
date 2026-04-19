#include "Collision_Manager.h"
#include "GameInstance.h"
#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"

IMPLEMENT_SINGLETON(CCollision_Manager)

CCollision_Manager::CCollision_Manager() : m_pGameInstance{ CGameInstance::GetInstance() }
{

}

void CCollision_Manager::Update_Collision()
{
    for (int i = 0; i < GROUP_END; ++i)
    {
        for (int j = i; j < GROUP_END; ++j)
        {
            if (!m_CollisionMatrix[i][j]) continue;

            // 충돌 처리
        }
    }
}

void CCollision_Manager::Clear_CollisionGroup()
{
    for (vector<CCollider*> vec : m_Colliders)
        vec.clear();
}

void CCollision_Manager::Set_CollisionMatrix(COLLISION_GROUP _lgroup, COLLISION_GROUP _rgroup, _bool _is)
{
    m_CollisionMatrix[_lgroup][_rgroup] = _is;
}

void CCollision_Manager::Add_CollisionGroup(COLLISION_GROUP _eGroup, CCollider* _pCollider)
{
	m_Colliders[_eGroup].push_back(_pCollider);
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

bool CCollision_Manager::CheckMove(CCollider* me, const XMFLOAT3& pos, const XMFLOAT3& move, XMFLOAT3& outSlide)
{
    bool bHit = false;
    XMFLOAT3 finalMove = move;

    for (CCollider* other : m_Colliders[GROUP_MAP])
    {
        if (!other->Get_Enable()) continue;

        // 함수 구현 후 주석 해제
       /* if (IsCollidingAfterMove(me, other, pos, move))
        {
            bHit = true;

            XMFLOAT3 normal = GetCollisionNormal(...);
            finalMove = Slide(move, normal);
        }*/
    }

    outSlide = finalMove;
    return bHit;
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

CCollision_Manager* CCollision_Manager::Create()
{
	return new CCollision_Manager();
}

void CCollision_Manager::Free()
{
    Clear_CollisionGroup();
}