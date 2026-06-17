#include "BVH.h"
#include "Collider.h"

void CBVH::Build(const vector<CCollider*>& colliders)
{
    Clear();
    if (colliders.empty()) return;

    // 1. 빌드용 임시 프리미티브
    vector<BuildPrim> prims;
    prims.reserve(colliders.size());
    for (CCollider* p : colliders) {
        if (!p) continue;
        _float3 c, e;
        if (!p->Get_AABBBound(c, e)) continue;
        prims.push_back({BoundingBox(c, e), p});
    }
    if (prims.empty()) return;

    // 2. 재귀 빌드 (Node 개수 상한은 2N-1, reallocate 방지)
    m_Nodes.reserve(prims.size() * 2);
    Build_Recursive(prims, 0, (_int)prims.size(), 0);

    // 3. collider 순서 그대로 m_Primitives로 복사 (정렬된 순서)
    m_Primitives.reserve(prims.size());
    for (auto& p : prims) m_Primitives.push_back(p.collider);

    m_bBuilt = true;
}

void CBVH::Clear()
{
    m_Nodes.clear();
    m_Primitives.clear();
    m_bBuilt = false;
    m_iMaxDepth = 0;
}

_int CBVH::Build_Recursive(vector<BuildPrim>& prims, _int primStart, _int primCount, _int depth)
{
    _int nodeIdx = (_int)m_Nodes.size();
    m_Nodes.emplace_back();

    if (depth > m_iMaxDepth) m_iMaxDepth = depth;

    // (a) 박스 합산
    XMVECTOR vMin = XMVectorReplicate(FLT_MAX);
    XMVECTOR vMax = XMVectorReplicate(-FLT_MAX);
    for (_int i = primStart; i < primStart + primCount; ++i) {
        XMVECTOR c = XMLoadFloat3(&prims[i].box.Center);
        XMVECTOR e = XMLoadFloat3(&prims[i].box.Extents);
        vMin = XMVectorMin(vMin, XMVectorSubtract(c, e));
        vMax = XMVectorMax(vMax, XMVectorAdd(c, e));
    }
    XMVECTOR vCenter = XMVectorScale(XMVectorAdd(vMin, vMax), 0.5f);
    XMVECTOR vExtents = XMVectorScale(XMVectorSubtract(vMax, vMin), 0.5f);
    XMStoreFloat3(&m_Nodes[nodeIdx].box.Center, vCenter);
    XMStoreFloat3(&m_Nodes[nodeIdx].box.Extents, vExtents);

    // (b) leaf 조건
    if (primCount <= LEAF_THRESHOLD) {
        m_Nodes[nodeIdx].primitiveStart = primStart;
        m_Nodes[nodeIdx].primitiveCount = primCount;
        return nodeIdx;
    }

    // (c) 가장 긴 축
    XMFLOAT3 ext; XMStoreFloat3(&ext, vExtents);
    _int axis = 0;
    if (ext.y > ext.x && ext.y >= ext.z) axis = 1;
    else if (ext.z > ext.x)              axis = 2;

    // (d) median split (centroid 기준 nth_element)
    _int mid = primStart + primCount / 2;
    std::nth_element(
        prims.begin() + primStart,
        prims.begin() + mid,
        prims.begin() + primStart + primCount,
        [axis](const BuildPrim& a, const BuildPrim& b) {
            return (&a.box.Center.x)[axis] < (&b.box.Center.x)[axis];
        });

    // (e) 재귀
    _int leftIdx = Build_Recursive(prims, primStart, mid - primStart, depth + 1);
    _int rightIdx = Build_Recursive(prims, mid, primCount - (mid - primStart), depth + 1);

    // 인덱스로 다시 접근 (재귀 후 m_Nodes 무효화 대비 — 단 reserve로 안전하긴 함)
    m_Nodes[nodeIdx].leftChild = leftIdx;
    m_Nodes[nodeIdx].rightChild = rightIdx;
    return nodeIdx;
}

void CBVH::Query_Sphere(const _float3& center, _float radius, vector<CCollider*>& outResults) const
{
    if (!m_bBuilt || m_Nodes.empty()) return;
    BoundingSphere sphere(center, radius);
    Query_Sphere_Recursive(0, sphere, outResults);
}

void CBVH::Query_Frustum(const BoundingFrustum& frustum, vector<CCollider*>& outResults) const
{
    if (!m_bBuilt || m_Nodes.empty()) return;
    Query_Frustum_Recursive(0, frustum, outResults);
}

void CBVH::Query_Sphere_Recursive(_int nodeIdx, const BoundingSphere& sphere, vector<CCollider*>& outResults) const
{
    const Node& node = m_Nodes[nodeIdx];
    if (!node.box.Intersects(sphere)) return;

    if (node.IsLeaf()) {
        // 후보 추출 단계: leaf 안의 모든 primitive 등록 (정확한 검사는 호출측)
        for (_int i = node.primitiveStart; i < node.primitiveStart + node.primitiveCount; ++i)
            outResults.push_back(m_Primitives[i]);
        return;
    }
    if (node.leftChild >= 0) Query_Sphere_Recursive(node.leftChild, sphere, outResults);
    if (node.rightChild >= 0) Query_Sphere_Recursive(node.rightChild, sphere, outResults);
}

void CBVH::Query_Frustum_Recursive(_int nodeIdx, const BoundingFrustum& frustum, vector<CCollider*>& outResults) const
{
    const Node& node = m_Nodes[nodeIdx];
    if (frustum.Contains(node.box) == DISJOINT) return;

    if (node.IsLeaf()) {
        for (_int i = node.primitiveStart; i < node.primitiveStart + node.primitiveCount; ++i)
            outResults.push_back(m_Primitives[i]);
        return;
    }
    if (node.leftChild >= 0) Query_Frustum_Recursive(node.leftChild, frustum, outResults);
    if (node.rightChild >= 0) Query_Frustum_Recursive(node.rightChild, frustum, outResults);
}