#pragma once

#include "Base.h"
#include "DebugDraw.h"
#include "Collider.h"

/* 1. 각 충돌체 클래스들의 부모 클래스다. */
/* 2. 원랜 구조체를 쓰면되는 구조체의 부모 구조체가 없더라. */

class CBounding abstract : public CBase
{
public:
	struct BOUND_DESC
	{
		_float3				vCenter;
		const _float4x4*	pParentMatrix = nullptr;
		const _float4x4*	pSocketMatrix = nullptr;
	};

protected:
	CBounding(EngineContext* pContext);
	virtual ~CBounding() = default;

public:
	virtual HRESULT Initialize();
	virtual void Update(_fmatrix WorldMatrix) = 0;
	virtual void Render(PrimitiveBatch<VertexPositionColor>* pBatch) = 0;
	virtual _bool Intersect(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding) = 0;
	virtual _bool Intersect_Offset(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding, const _float3& vOffset) = 0;
	virtual _float3 Get_CollisionNormal(CCollider::COLLIDERTYPE eTargetType, CBounding* pTargetBounding) = 0;
	virtual _float3 Get_Center() = 0;

    virtual void Get_SphereBound(_float3& outCenter, _float& outRadius) const = 0;
    virtual void Get_AABBBound(_float3& outCenter, _float3& outExtents) const = 0;  

protected:
	EngineContext* m_pContext = { nullptr };

	_bool					m_isColl = { false };

public:
	virtual void Free() override;
};
