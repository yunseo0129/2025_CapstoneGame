#pragma once
// 서버 권위 맵 이동 충돌 처리 모듈
// 클라이언트 CCollision_Manager::CheckMove 알고리즘을 엔진 비종속으로 이식.
// 참고: HelloDinner/Collision_Manager.{h,cpp}, BVH.{h,cpp}, Collider.h
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <cmath>
#include <cfloat>

using namespace DirectX;

// ─── 콜라이더 타입 ───────────────────────────────────────────────────────────
enum SCOLLIDER_TYPE { SCOL_SPHERE, SCOL_AABB, SCOL_OBB };

// ─── 경량 서버 콜라이더 ─────────────────────────────────────────────────────
// CCollider에서 CComponent 상속/엔진 의존을 제거한 구조체.
// type 필드에 따라 sphere/aabb/obb 중 하나가 유효한 월드 위치로 갱신된다.
struct SServerCollider
{
    SCOLLIDER_TYPE      type   = SCOL_SPHERE;
    bool                enable = true;

    // type에 따라 유효한 필드 하나만 사용
    BoundingBox             aabb;
    BoundingOrientedBox     obb;
    BoundingSphere          sphere;

    // ─── 팩토리 ───────────────────────────────────────────────────────────
    static SServerCollider FromSphere(XMFLOAT3 center, float radius);
    static SServerCollider FromAABB(XMFLOAT3 center, XMFLOAT3 extents);
    static SServerCollider FromOBB(XMFLOAT3 center, XMFLOAT3 extents, XMFLOAT4 orientation);

    // ─── BVH 빌드·쿼리용 바운드 ─────────────────────────────────────────
    bool Get_SphereBound(XMFLOAT3& outCenter, float& outRadius) const;
    bool Get_AABBBound(XMFLOAT3& outCenter, XMFLOAT3& outExtents) const;

    // ─── 충돌 판정 ───────────────────────────────────────────────────────
    // 'this' 를 vMove 만큼 이동했을 때 'other' 와 겹치는가?
    // (other를 -vMove 만큼 오프셋하여 static 상태의 this 와 검사 — CheckMove 핵심)
    bool Intersect_Offset(const SServerCollider* other, const XMFLOAT3& vOffset) const;

    // 충돌 면 법선 (this = 이동 중인 sphere 기준)
    XMFLOAT3 Get_CollisionNormal(const SServerCollider* other) const;

    // 레이 교차
    bool IntersectsRay(FXMVECTOR rayOrigin, FXMVECTOR rayDir, float& distance) const;
};

// ─── 정적 맵 콜라이더 BVH ───────────────────────────────────────────────────
// CBVH(BVH.h)를 SServerCollider* 기반으로 이식.
class SSVBH
{
public:
    struct Node {
        BoundingBox box;
        int leftChild  = -1;
        int rightChild = -1;
        int primStart  = -1;
        int primCount  = 0;
        bool IsLeaf() const { return leftChild < 0 && rightChild < 0; }
    };

    void Build(const std::vector<SServerCollider*>& colliders);
    void Clear();
    bool Is_Built() const { return m_built; }
    int  Get_NodeCount() const { return (int)m_Nodes.size(); }

    void Query_Sphere(const XMFLOAT3& center, float radius, std::vector<SServerCollider*>& out) const;

private:
    struct BuildPrim { BoundingBox box; SServerCollider* col; };
    int  Build_Recursive(std::vector<BuildPrim>& prims, int start, int count, int depth);
    void Query_Recursive(int node, const BoundingSphere& sphere, std::vector<SServerCollider*>& out) const;

    static const int LEAF_THRESHOLD = 4;
    std::vector<Node>              m_Nodes;
    std::vector<SServerCollider*>  m_Primitives;
    bool                           m_built    = false;
    int                            m_maxDepth = 0;
};

// ─── CheckMove ───────────────────────────────────────────────────────────────
// CCollision_Manager::CheckMove 와 동일한 알고리즘(BVH 후보 추출 + MAX_ITER=4 슬라이드).
//
// me        : 이동하는 플레이어 sphere (월드 위치로 미리 갱신해야 함)
// move      : 이번 프레임 이동 벡터 (gravity+input 합산)
// outSlide  : 충돌 해소 후 실제 이동 벡터 (출력)
// outHits   : 충돌한 맵 콜라이더 목록 (nullptr=불필요)
// bvh       : 맵 정적 BVH
// mapColliders : BVH 미빌드 시 fallback 전체 목록
bool ServerCheckMove(
    SServerCollider*                        me,
    const XMFLOAT3&                         move,
    XMFLOAT3&                               outSlide,
    std::vector<SServerCollider*>*          outHits,
    const SSVBH&                            bvh,
    const std::vector<SServerCollider*>&    mapColliders
);
