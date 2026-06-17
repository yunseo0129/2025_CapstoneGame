#pragma once
#include "Base.h"

class CCollider;

class CBVH final
{
public:
    struct Node
    {
        DirectX::BoundingBox box;
        _int  leftChild = -1;
        _int  rightChild = -1;
        _int  primitiveStart = -1;
        _int  primitiveCount = 0;
        _bool IsLeaf() const { return leftChild < 0 && rightChild < 0; }
    };

public:
    CBVH() = default;
    ~CBVH() = default;

    void Build(const vector<CCollider*>& colliders);
    void Clear();

    void Query_Sphere(const _float3& center, _float radius, vector<CCollider*>& outResults) const;
    void Query_Frustum(const DirectX::BoundingFrustum& frustum, vector<CCollider*>& outResults) const;

    _bool Is_Built()              const { return m_bBuilt; }
    _int  Get_NodeCount()         const { return (_int)m_Nodes.size(); }
    _int  Get_PrimitiveCount()    const { return (_int)m_Primitives.size(); }
    _int  Get_MaxDepth()          const { return m_iMaxDepth; }

private:
    struct BuildPrim
    {
        DirectX::BoundingBox box;
        CCollider* collider;
    };

    _int Build_Recursive(vector<BuildPrim>& prims, _int primStart, _int primCount, _int depth);
    void Query_Sphere_Recursive(_int nodeIdx, const DirectX::BoundingSphere& sphere, vector<CCollider*>& outResults) const;
    void Query_Frustum_Recursive(_int nodeIdx, const DirectX::BoundingFrustum& frustum, vector<CCollider*>& outResults) const;

    static const _int LEAF_THRESHOLD = 4;

    vector<Node>       m_Nodes;
    vector<CCollider*> m_Primitives;
    _bool              m_bBuilt = false;
    _int               m_iMaxDepth = 0;
};