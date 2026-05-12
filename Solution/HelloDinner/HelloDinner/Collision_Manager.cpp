#include "Collision_Manager.h"
#include "GameInstance.h"
#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"

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
	for (auto& vec : m_Colliders)
		vec.clear();
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
	bool bHit = false;
	XMFLOAT3 finalMove = move;

	// 최대 반복 횟수 설정 (충돌 해결이 완전히 될 때까지 여러 번 시도)
	const int MAX_ITER = 4;
	for (int iter = 0; iter < MAX_ITER; ++iter)
	{
		bool bAnyHitThisIter = false;

		for (CCollider* other : m_Colliders[GROUP_MAP])
		{
			// MAP 오브젝트의 Collider가 활성화되어있는지 확인
			if (!other->Get_Enable()) continue;

			// 이동 후 충돌 해?
			if (IsCollidingAfterMove(me, other, finalMove))
			{
				bHit = true;

				// 충돌한 면의 법선 벡터 구하기
				// XMFLOAT3 normal = GetCollisionNormal(me, other);
				XMFLOAT3 normal = me->Get_CollisionNormal(other);
				finalMove = Slide(finalMove, normal);
			}
		}

		// 더 이상 충돌 없으면 조기 종료
		if (!bAnyHitThisIter) break;

		// 슬라이드 결과가 거의 0이면 정지로 간주
		XMVECTOR vLenSq = XMVector3LengthSq(XMLoadFloat3(&finalMove));
		if (XMVectorGetX(vLenSq) < 1e-8f)
		{
			finalMove = {0.f, 0.f, 0.f};
			break;
		}
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

CCollision_Manager* CCollision_Manager::Create()
{
	return new CCollision_Manager();
}

void CCollision_Manager::Free()
{
	Clear_CollisionGroup();
}