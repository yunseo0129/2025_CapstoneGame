#include "pch.h"
#include "ServerCollision.h"
#include <algorithm>
#include <iostream>

// =============================================================================
// SServerCollider — 팩토리
// =============================================================================

SServerCollider SServerCollider::FromSphere(XMFLOAT3 center, float radius)
{
    SServerCollider c;
    c.type   = SCOL_SPHERE;
    c.sphere = BoundingSphere(center, radius);
    return c;
}

SServerCollider SServerCollider::FromAABB(XMFLOAT3 center, XMFLOAT3 extents)
{
    SServerCollider c;
    c.type = SCOL_AABB;
    c.aabb = BoundingBox(center, extents);
    return c;
}

SServerCollider SServerCollider::FromOBB(XMFLOAT3 center, XMFLOAT3 extents, XMFLOAT4 orientation)
{
    SServerCollider c;
    c.type = SCOL_OBB;
    c.obb  = BoundingOrientedBox(center, extents, orientation);
    return c;
}

// =============================================================================
// SServerCollider — 바운드 조회
// (클라 CBounding_*.Get_SphereBound / Get_AABBBound 와 동일)
// =============================================================================

bool SServerCollider::Get_SphereBound(XMFLOAT3& outCenter, float& outRadius) const
{
    switch (type) {
    case SCOL_SPHERE:
        outCenter = sphere.Center;
        outRadius = sphere.Radius;
        return true;
    case SCOL_AABB:
        outCenter = aabb.Center;
        XMStoreFloat(&outRadius, XMVector3Length(XMLoadFloat3(&aabb.Extents)));
        return true;
    case SCOL_OBB:
        outCenter = obb.Center;
        XMStoreFloat(&outRadius, XMVector3Length(XMLoadFloat3(&obb.Extents)));
        return true;
    }
    return false;
}

bool SServerCollider::Get_AABBBound(XMFLOAT3& outCenter, XMFLOAT3& outExtents) const
{
    switch (type) {
    case SCOL_AABB:
        outCenter  = aabb.Center;
        outExtents = aabb.Extents;
        return true;
    case SCOL_SPHERE:
        outCenter  = sphere.Center;
        outExtents = { sphere.Radius, sphere.Radius, sphere.Radius };
        return true;
    case SCOL_OBB: {
        // OBB 8 코너를 감싸는 AABB (Bounding_OBB::Get_AABBBound 동일)
        XMFLOAT3 corners[8];
        obb.GetCorners(corners);
        XMVECTOR vMin = XMLoadFloat3(&corners[0]);
        XMVECTOR vMax = vMin;
        for (int i = 1; i < 8; ++i) {
            XMVECTOR v = XMLoadFloat3(&corners[i]);
            vMin = XMVectorMin(vMin, v);
            vMax = XMVectorMax(vMax, v);
        }
        XMStoreFloat3(&outCenter,  XMVectorScale(XMVectorAdd(vMin, vMax), 0.5f));
        XMStoreFloat3(&outExtents, XMVectorScale(XMVectorSubtract(vMax, vMin), 0.5f));
        return true;
    }
    }
    return false;
}

// =============================================================================
// SServerCollider — Intersect_Offset
// 클라 CBounding_Sphere/AABB/OBB::Intersect_Offset 와 동일.
// "this를 vOffset 만큼 이동했을 때 other와 겹치는가?"
// = other를 -vOffset 만큼 오프셋하여 static this와 테스트.
// =============================================================================

bool SServerCollider::Intersect_Offset(const SServerCollider* other, const XMFLOAT3& vOffset) const
{
    XMVECTOR vOff = XMLoadFloat3(&vOffset);

    switch (type) {

    // ── this=Sphere ──
    case SCOL_SPHERE:
        switch (other->type) {
        case SCOL_AABB: {
            BoundingBox tmp = other->aabb;
            XMStoreFloat3(&tmp.Center, XMVectorAdd(XMLoadFloat3(&tmp.Center), vOff));
            return sphere.Intersects(tmp);
        }
        case SCOL_OBB: {
            BoundingOrientedBox tmp = other->obb;
            XMStoreFloat3(&tmp.Center, XMVectorAdd(XMLoadFloat3(&tmp.Center), vOff));
            return sphere.Intersects(tmp);
        }
        case SCOL_SPHERE: {
            BoundingSphere tmp = other->sphere;
            XMStoreFloat3(&tmp.Center, XMVectorAdd(XMLoadFloat3(&tmp.Center), vOff));
            return sphere.Intersects(tmp);
        }
        }
        break;

    // ── this=AABB ──
    case SCOL_AABB:
        switch (other->type) {
        case SCOL_AABB: {
            BoundingBox tmp = other->aabb;
            XMStoreFloat3(&tmp.Center, XMVectorAdd(XMLoadFloat3(&tmp.Center), vOff));
            return aabb.Intersects(tmp);
        }
        case SCOL_OBB: {
            BoundingOrientedBox tmp = other->obb;
            XMStoreFloat3(&tmp.Center, XMVectorAdd(XMLoadFloat3(&tmp.Center), vOff));
            return aabb.Intersects(tmp);
        }
        case SCOL_SPHERE: {
            BoundingSphere tmp = other->sphere;
            XMStoreFloat3(&tmp.Center, XMVectorAdd(XMLoadFloat3(&tmp.Center), vOff));
            return aabb.Intersects(tmp);
        }
        }
        break;

    // ── this=OBB (플레이어 OBB 콜라이더 대비) ──
    case SCOL_OBB:
        switch (other->type) {
        case SCOL_AABB: {
            BoundingBox tmp = other->aabb;
            XMStoreFloat3(&tmp.Center, XMVectorAdd(XMLoadFloat3(&tmp.Center), vOff));
            return obb.Intersects(tmp);
        }
        case SCOL_OBB: {
            BoundingOrientedBox tmp = other->obb;
            XMStoreFloat3(&tmp.Center, XMVectorAdd(XMLoadFloat3(&tmp.Center), vOff));
            return obb.Intersects(tmp);
        }
        case SCOL_SPHERE: {
            BoundingSphere tmp = other->sphere;
            XMStoreFloat3(&tmp.Center, XMVectorAdd(XMLoadFloat3(&tmp.Center), vOff));
            return obb.Intersects(tmp);
        }
        }
        break;
    }
    return false;
}

// =============================================================================
// SServerCollider — Get_CollisionNormal
// 클라 CBounding_Sphere::Get_CollisionNormal 와 동일 (this=sphere 기준).
// =============================================================================

XMFLOAT3 SServerCollider::Get_CollisionNormal(const SServerCollider* other) const
{
    XMVECTOR vNormal = XMVectorSet(0.f, 1.f, 0.f, 0.f); // 기본 위쪽

    if (type == SCOL_SPHERE) {
        XMVECTOR vMe = XMLoadFloat3(&sphere.Center);

        switch (other->type) {
        case SCOL_AABB: {
            // 구 중심 → AABB 최근접점 방향 (Bounding_Sphere::Get_CollisionNormal)
            XMVECTOR vAABBCenter = XMLoadFloat3(&other->aabb.Center);
            XMVECTOR vExtents    = XMLoadFloat3(&other->aabb.Extents);
            XMVECTOR vMin = XMVectorSubtract(vAABBCenter, vExtents);
            XMVECTOR vMax = XMVectorAdd(vAABBCenter, vExtents);
            XMVECTOR vClosest = XMVectorClamp(vMe, vMin, vMax);
            XMVECTOR vDir = XMVectorSubtract(vMe, vClosest);
            if (XMVector3Equal(vDir, XMVectorZero()))
                vDir = XMVectorSubtract(vMe, vAABBCenter);
            vNormal = XMVector3Normalize(vDir);
            break;
        }
        case SCOL_OBB: {
            // OBB 로컬 공간에서 최근접점 계산 (Bounding_Sphere::Get_CollisionNormal)
            XMVECTOR vOBBCenter   = XMLoadFloat3(&other->obb.Center);
            XMVECTOR vExtents     = XMLoadFloat3(&other->obb.Extents);
            XMVECTOR vOrientation = XMLoadFloat4(&other->obb.Orientation);
            XMVECTOR vInvOri      = XMQuaternionConjugate(vOrientation);
            XMVECTOR vLocal       = XMVector3Rotate(XMVectorSubtract(vMe, vOBBCenter), vInvOri);
            XMVECTOR vLocalCl     = XMVectorClamp(vLocal, XMVectorNegate(vExtents), vExtents);
            XMVECTOR vWorldCl     = XMVectorAdd(XMVector3Rotate(vLocalCl, vOrientation), vOBBCenter);
            XMVECTOR vDir         = XMVectorSubtract(vMe, vWorldCl);
            if (XMVector3Equal(vDir, XMVectorZero()))
                vDir = XMVectorSubtract(vMe, vOBBCenter);
            vNormal = XMVector3Normalize(vDir);
            break;
        }
        case SCOL_SPHERE: {
            vNormal = XMVector3Normalize(
                XMVectorSubtract(XMLoadFloat3(&sphere.Center), XMLoadFloat3(&other->sphere.Center)));
            break;
        }
        }
    }
    else if (type == SCOL_AABB) {
        // AABB: 중심 델타의 지배축 (Bounding_AABB::Get_CollisionNormal)
        XMVECTOR vMe  = XMLoadFloat3(&aabb.Center);
        XMVECTOR vTgt = XMVectorZero();
        switch (other->type) {
        case SCOL_AABB:   vTgt = XMLoadFloat3(&other->aabb.Center);   break;
        case SCOL_OBB:    vTgt = XMLoadFloat3(&other->obb.Center);    break;
        case SCOL_SPHERE: vTgt = XMLoadFloat3(&other->sphere.Center); break;
        }
        XMFLOAT3 d; XMStoreFloat3(&d, XMVectorSubtract(vMe, vTgt));
        float ax = fabsf(d.x), ay = fabsf(d.y), az = fabsf(d.z);
        XMFLOAT3 n{0.f, 0.f, 0.f};
        if      (ax >= ay && ax >= az) n.x = (d.x >= 0.f) ?  1.f : -1.f;
        else if (ay >= ax && ay >= az) n.y = (d.y >= 0.f) ?  1.f : -1.f;
        else                           n.z = (d.z >= 0.f) ?  1.f : -1.f;
        vNormal = XMLoadFloat3(&n);
    }

    XMFLOAT3 result; XMStoreFloat3(&result, vNormal);
    return result;
}

bool SServerCollider::IntersectsRay(FXMVECTOR rayOrigin, FXMVECTOR rayDir, float& distance) const
{
    switch (type) {
    case SCOL_SPHERE: return sphere.Intersects(rayOrigin, rayDir, distance);
    case SCOL_AABB:   return aabb.Intersects(rayOrigin, rayDir, distance);
    case SCOL_OBB:    return obb.Intersects(rayOrigin, rayDir, distance);
    }
    return false;
}

// =============================================================================
// SSVBH — Build / Query
// CBVH(BVH.cpp)와 동일 알고리즘, SServerCollider* 기반으로 이식.
// =============================================================================

void SSVBH::Build(const std::vector<SServerCollider*>& colliders)
{
    Clear();
    if (colliders.empty()) return;

    std::vector<BuildPrim> prims;
    prims.reserve(colliders.size());
    for (SServerCollider* c : colliders) {
        if (!c) continue;
        XMFLOAT3 cen, ext;
        if (!c->Get_AABBBound(cen, ext)) continue;
        prims.push_back({ BoundingBox(cen, ext), c });
    }
    if (prims.empty()) return;

    m_Nodes.reserve(prims.size() * 2);
    Build_Recursive(prims, 0, (int)prims.size(), 0);

    m_Primitives.reserve(prims.size());
    for (auto& p : prims) m_Primitives.push_back(p.col);

    m_built = true;
    std::cout << "[Collision] BVH built: node=" << m_Nodes.size()
              << " prim=" << m_Primitives.size()
              << " maxDepth=" << m_maxDepth << std::endl;
}

void SSVBH::Clear()
{
    m_Nodes.clear();
    m_Primitives.clear();
    m_built    = false;
    m_maxDepth = 0;
}

int SSVBH::Build_Recursive(std::vector<BuildPrim>& prims, int start, int count, int depth)
{
    int nodeIdx = (int)m_Nodes.size();
    m_Nodes.emplace_back();
    if (depth > m_maxDepth) m_maxDepth = depth;

    // 합산 AABB
    XMVECTOR vMin = XMVectorReplicate(FLT_MAX);
    XMVECTOR vMax = XMVectorReplicate(-FLT_MAX);
    for (int i = start; i < start + count; ++i) {
        XMVECTOR c = XMLoadFloat3(&prims[i].box.Center);
        XMVECTOR e = XMLoadFloat3(&prims[i].box.Extents);
        vMin = XMVectorMin(vMin, XMVectorSubtract(c, e));
        vMax = XMVectorMax(vMax, XMVectorAdd(c, e));
    }
    XMStoreFloat3(&m_Nodes[nodeIdx].box.Center,  XMVectorScale(XMVectorAdd(vMin, vMax), 0.5f));
    XMStoreFloat3(&m_Nodes[nodeIdx].box.Extents, XMVectorScale(XMVectorSubtract(vMax, vMin), 0.5f));

    // leaf
    if (count <= LEAF_THRESHOLD) {
        m_Nodes[nodeIdx].primStart = start;
        m_Nodes[nodeIdx].primCount = count;
        return nodeIdx;
    }

    // 가장 긴 축으로 median split
    XMFLOAT3 ext; XMStoreFloat3(&ext, XMVectorScale(XMVectorSubtract(vMax, vMin), 0.5f));
    int axis = 0;
    if (ext.y > ext.x && ext.y >= ext.z) axis = 1;
    else if (ext.z > ext.x)              axis = 2;

    int mid = start + count / 2;
    std::nth_element(
        prims.begin() + start,
        prims.begin() + mid,
        prims.begin() + start + count,
        [axis](const BuildPrim& a, const BuildPrim& b) {
            return (&a.box.Center.x)[axis] < (&b.box.Center.x)[axis];
        });

    int left  = Build_Recursive(prims, start, mid - start,          depth + 1);
    int right = Build_Recursive(prims, mid,   count - (mid - start), depth + 1);
    m_Nodes[nodeIdx].leftChild  = left;
    m_Nodes[nodeIdx].rightChild = right;
    return nodeIdx;
}

void SSVBH::Query_Sphere(const XMFLOAT3& center, float radius, std::vector<SServerCollider*>& out) const
{
    if (!m_built || m_Nodes.empty()) return;
    BoundingSphere sphere(center, radius);
    Query_Recursive(0, sphere, out);
}

void SSVBH::Query_Recursive(int nodeIdx, const BoundingSphere& sphere, std::vector<SServerCollider*>& out) const
{
    const Node& node = m_Nodes[nodeIdx];
    if (!node.box.Intersects(sphere)) return;
    if (node.IsLeaf()) {
        for (int i = node.primStart; i < node.primStart + node.primCount; ++i)
            out.push_back(m_Primitives[i]);
        return;
    }
    if (node.leftChild  >= 0) Query_Recursive(node.leftChild,  sphere, out);
    if (node.rightChild >= 0) Query_Recursive(node.rightChild, sphere, out);
}

// =============================================================================
// ServerCheckMove — 내부 도우미
// CCollision_Manager::IsCollidingAfterMove / Slide 와 동일.
// =============================================================================

static bool S_IsCollidingAfterMove(
    const SServerCollider* me,
    const SServerCollider* other,
    const XMFLOAT3&        move)
{
    // other를 -move 만큼 오프셋하여 정지한 me와 겹치는지 확인
    const XMFLOAT3 rev = { -move.x, -move.y, -move.z };
    return me->Intersect_Offset(other, rev);
}

static XMFLOAT3 S_Slide(const XMFLOAT3& move, const XMFLOAT3& normal)
{
    // slide = move - dot(move,normal)*normal  (CCollision_Manager::Slide 동일)
    XMVECTOR vM = XMLoadFloat3(&move);
    XMVECTOR vN = XMLoadFloat3(&normal);
    XMVECTOR vSlide = XMVectorSubtract(vM, XMVectorMultiply(XMVector3Dot(vM, vN), vN));
    XMFLOAT3 result; XMStoreFloat3(&result, vSlide);
    return result;
}

// =============================================================================
// ServerCheckMove — 공개 API
// CCollision_Manager::CheckMove(84-144) 와 동일 알고리즘.
// =============================================================================

bool ServerCheckMove(
    SServerCollider*                        me,
    const XMFLOAT3&                         move,
    XMFLOAT3&                               outSlide,
    std::vector<SServerCollider*>*          outHits,
    const SSVBH&                            bvh,
    const std::vector<SServerCollider*>&    mapColliders)
{
    bool     bHit      = false;
    XMFLOAT3 finalMove = move;
    const int MAX_ITER = 4;

    // BVH 후보 추출 (미빌드 시 전체 mapColliders fallback)
    std::vector<SServerCollider*>  queryResults;
    std::vector<SServerCollider*>* pCandidates = nullptr;

    if (bvh.Is_Built()) {
        XMFLOAT3 myCenter; float myRadius;
        if (me->Get_SphereBound(myCenter, myRadius)) {
            XMVECTOR vMoveLen  = XMVector3Length(XMLoadFloat3(&move));
            float    fMoveDist = XMVectorGetX(vMoveLen);
            float    fExpandR  = myRadius + fMoveDist * (MAX_ITER + 1);
            bvh.Query_Sphere(myCenter, fExpandR, queryResults);
            pCandidates = &queryResults;
        }
    }
    if (!pCandidates) {
        // BVH 없음 — 전체 맵 콜라이더 순회 (빌드 후에는 여기 오지 않음)
        pCandidates = const_cast<std::vector<SServerCollider*>*>(&mapColliders);
    }

    // 슬라이드 반복 (MAX_ITER)
    for (int iter = 0; iter < MAX_ITER; ++iter) {
        bool bAnyHit = false;

        for (SServerCollider* other : *pCandidates) {
            if (!other || !other->enable) continue;

            if (S_IsCollidingAfterMove(me, other, finalMove)) {
                bHit    = true;
                bAnyHit = true;

                if (outHits) {
                    bool dup = false;
                    for (auto* h : *outHits) if (h == other) { dup = true; break; }
                    if (!dup) outHits->push_back(other);
                }

                XMFLOAT3 normal = me->Get_CollisionNormal(other);
                finalMove = S_Slide(finalMove, normal);
            }
        }

        if (!bAnyHit) break;

        XMVECTOR vLenSq = XMVector3LengthSq(XMLoadFloat3(&finalMove));
        if (XMVectorGetX(vLenSq) < 1e-8f) { finalMove = {0.f, 0.f, 0.f}; break; }
    }

    outSlide = finalMove;
    return bHit;
}
