//#pragma once
//
//#include "Bounding.h"
//
//class CBounding_AABB final : public CBounding
//{
//public:
//	typedef struct : public CBounding::BOUND_DESC
//	{
//		_float3		vExtents;
//	}BOUND_AABB_DESC;
//private:
//	CBounding_AABB(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
//	virtual ~CBounding_AABB() = default;
//
//public:
//	const BoundingBox* Get_Desc() const {
//		return m_pBoundDesc;
//	}
//
//public:
//	virtual HRESULT Initialize(const BOUND_DESC* pBoundDesc);
//	virtual void Update(_fmatrix WorldMatrix) override;
//	virtual void Render(PrimitiveBatch<VertexPositionColor>* pBatch) override;
//	virtual _bool Intersect(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding) override;
//	virtual _float3 Get_Center() override;
//
//private:
//	BoundingBox* m_pOriginalBoundDesc = {};
//	BoundingBox* m_pBoundDesc = {};
//
//public:
//	static CBounding_AABB* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, const BOUND_DESC* pBoundDesc);
//	virtual void Free() override;
//};