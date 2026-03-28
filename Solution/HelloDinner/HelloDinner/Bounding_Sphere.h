#pragma once

#include "Bounding.h"

class CBounding_Sphere final : public CBounding
{
public:
	typedef struct : public CBounding::BOUND_DESC
	{
		_float		fRadius;
	}BOUND_SPHERE_DESC;
private:
	CBounding_Sphere(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual ~CBounding_Sphere() = default;
public:
	const BoundingSphere* Get_Desc() const {
		return m_pBoundDesc;
	}
public:
	virtual HRESULT Initialize(const BOUND_DESC* pBoundDesc);
	virtual void Update(_fmatrix WorldMatrix) override;
	virtual void Render(PrimitiveBatch<VertexPositionColor>* pBatch) override;
	virtual _bool Intersect(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding) override;
	virtual _float3 Get_Center() override;

private:
	BoundingSphere* m_pOriginalBoundDesc = {};
	BoundingSphere* m_pBoundDesc = {};

public:
	static CBounding_Sphere* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, const BOUND_DESC* pBoundDesc);
	virtual void Free() override;
};