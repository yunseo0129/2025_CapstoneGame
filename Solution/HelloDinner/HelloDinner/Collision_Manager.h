#pragma once
#include "Base.h"

class CCollision_Manager final : public CBase
{
	DECLARE_SINGLETON(CCollision_Manager)

public:
	// Collider class안에서 소유자나 부위 등 ID 값 정해줘서 활용해야 할 듯 (ex 피아식별)
	enum COLLISION_GROUP {
		GROUP_MAP,
		GROUP_PLAYER,
		GROUP_PROJECTILE,
		GROUP_END
	};

private:
	CCollision_Manager();
	virtual ~CCollision_Manager() = default;

public:
	void Update_Collision();
	void Clear_CollisionGroup();
	void Set_CollisionMatrix(COLLISION_GROUP _lgroup, COLLISION_GROUP _rgroup, _bool _is);
	void Add_CollisionGroup(COLLISION_GROUP _eGroup, class CCollider* _pCollider);
	void Delete_CollisionGroup(COLLISION_GROUP _eGroup, class CCollider* _pCollider);
	vector<class CCollider*> CollisionCheck_with_Group(class CCollider* _pCollider, COLLISION_GROUP _eGroup);
	bool CollisionCheck_with_Collider(class CCollider* _pMyCollider, class CCollider* _pOtherCollider);
	bool CheckMove(CCollider* me, const XMFLOAT3& move, XMFLOAT3& outSlide);

private:
	bool IsCollidingAfterMove ( CCollider* me , CCollider* other , const XMFLOAT3& move );
	XMFLOAT3 GetCollisionNormal ( CCollider* me , CCollider* other );
	XMFLOAT3 Slide(const XMFLOAT3& move, const XMFLOAT3& normal);

private:
	class CGameInstance*		m_pGameInstance = nullptr;
	vector<class CCollider*>	m_Colliders[GROUP_END]; // 그룹마다 따로 콜라이더 넣어주기
	_bool						m_CollisionMatrix[GROUP_END][GROUP_END]; // 왼쪽 그룹이 오른쪽 그룹과 충돌처리 여부(왼쪽이 메인)

public:
	static CCollision_Manager* Create();
	virtual void Free();
};