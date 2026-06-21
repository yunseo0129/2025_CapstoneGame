#pragma once
#include "Base.h"
#include "BVH.h"

class CCollision_Manager final: public CBase
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
    struct RAY_HIT
    {
        bool hit = false;
        float distance = FLT_MAX;

        CCollider* pCollider = nullptr;
        CGameObject* pTarget = nullptr;
    };
private:
    CCollision_Manager();
    virtual ~CCollision_Manager() = default;

public:
    void Update_Collision();
    void Clear_CollisionGroup();
    void Set_CollisionMatrix(COLLISION_GROUP _lgroup, COLLISION_GROUP _rgroup, _bool _is);
    void Add_CollisionGroup(COLLISION_GROUP _eGroup, class CCollider* _pCollider);
    // 모든애들 콜리젼에 추가해주고 총알벡터에서 타겟 아직없는애들만 충돌시켜서 타격지점 정해주기
    void Delete_CollisionGroup(COLLISION_GROUP _eGroup, class CCollider* _pCollider);
    vector<class CCollider*> CollisionCheck_with_Group(class CCollider* _pCollider, COLLISION_GROUP _eGroup);
    bool CollisionCheck_with_Collider(class CCollider* _pMyCollider, class CCollider* _pOtherCollider);
    bool CheckMove(CCollider* me, const XMFLOAT3& move, XMFLOAT3& outSlide);
    bool CheckMove(CCollider* me, const XMFLOAT3& move, XMFLOAT3& outSlide, vector<class CCollider*>* outHits);
    RAY_HIT RayCast(
        _vector rayPos,
        _vector rayDir,
        COLLISION_GROUP group
        , CGameObject* pShooter);

    // 총알관련
    void Set_Bullets(class CBullets* _p);
    class CBullets* Get_Bullets() {
        return m_pBullets;
    }
    void NewBullet(_vector _look, _vector _pos, CGameObject* pShooter);

    //BVH
    void Build_StaticBVH();
    void Invalidate_StaticBVH() { m_bStaticBVHDirty = true; }

    void  Set_BVHEnabled(_bool b) { m_bBVHEnabled = b; }
    _bool Is_BVHEnabled()         const { return m_bBVHEnabled; }
    _int  Get_BVHNodeCount()      const { return m_StaticBVH.Get_NodeCount(); }
    _int  Get_BVHPrimitiveCount() const { return m_StaticBVH.Get_PrimitiveCount(); }
    _int  Get_BVHMaxDepth()       const { return m_StaticBVH.Get_MaxDepth(); }
    _int  Get_LastQueryCandidates() const { return m_iLastQueryCandidates; }

    // [Fracture] 파편 충돌 브로드페이즈: 정적 BVH 구 질의 노출
    void Query_StaticColliders_Sphere(const _float3& center, _float radius, vector<class CCollider*>& out) const
    {
        m_StaticBVH.Query_Sphere(center, radius, out);
    }

    void Cull_StaticBVH(const DirectX::BoundingFrustum* pMainFrustum,
        const DirectX::BoundingSphere* pShadowBounds);

    void  Set_CullingBVHEnabled(_bool b) { m_bCullingBVHEnabled = b; }
    _bool Is_CullingBVHEnabled()      const { return m_bCullingBVHEnabled; }
    _int  Get_LastFrustumCandidates() const { return m_iLastFrustumCandidates; }
    _int  Get_LastShadowCandidates()  const { return m_iLastShadowCandidates; }


private:
    bool IsCollidingAfterMove(CCollider* me, CCollider* other, const XMFLOAT3& move);
    XMFLOAT3 GetCollisionNormal(CCollider* me, CCollider* other);
    XMFLOAT3 Slide(const XMFLOAT3& move, const XMFLOAT3& normal);

private:
    class CGameInstance* m_pGameInstance = nullptr;
    vector<class CCollider*>	m_Colliders[GROUP_END]; // 그룹마다 따로 콜라이더 넣어주기
    _bool						m_CollisionMatrix[GROUP_END][GROUP_END]; // 왼쪽 그룹이 오른쪽 그룹과 충돌처리 여부(왼쪽이 메인)

    class CBullets* m_pBullets;

    //BVH
    CBVH                  m_StaticBVH;
    _bool                 m_bStaticBVHDirty = true;
    _bool                 m_bBVHEnabled = true;
    mutable _int          m_iLastQueryCandidates = 0;

    _bool m_bCullingBVHEnabled = true;
    _int  m_iLastFrustumCandidates = 0;
    _int  m_iLastShadowCandidates = 0;

public:
    static CCollision_Manager* Create();
    virtual void Free();
};