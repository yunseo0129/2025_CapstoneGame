#pragma once

#include "Bounding.h"

class CBounding_AABB final : public CBounding
{
public:
	struct BOUND_AABB_DESC : public CBounding::BOUND_DESC
	{
		_float3		vExtents;
	};
private:
	CBounding_AABB(EngineContext* pContext);
	virtual ~CBounding_AABB() = default;

public:
	const BoundingBox* Get_Desc() const {
		return m_pBoundDesc;
	}

public:
	virtual HRESULT Initialize(const BOUND_DESC* pBoundDesc);
	virtual void Update(_fmatrix WorldMatrix) override;
	virtual void Render(PrimitiveBatch<VertexPositionColor>* pBatch) override;
	virtual _bool Intersect(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding) override;
	virtual _bool Intersect_Offset(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding, const _float3& vOffset) override;
	virtual _float3 Get_CollisionNormal(CCollider::COLLIDERTYPE eTargetType, CBounding* pTargetBounding) override;
	virtual _float3 Get_Center() override;
    virtual void Get_SphereBound(_float3& outCenter, _float& outRadius) const override;
    virtual void Get_AABBBound(_float3& outCenter, _float3& outExtents) const override;

private:
	BoundingBox* m_pOriginalBoundDesc = {};
	BoundingBox* m_pBoundDesc = {};

public:
	static CBounding_AABB* Create(EngineContext* pContext, const BOUND_DESC* pBoundDesc);
	virtual void Free() override;
};
