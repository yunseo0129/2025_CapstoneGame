//#pragma once
//
//#include "Base.h"
//#include "DebugDraw.h"
//#include "Collider.h"
//
///* 1. 각 충돌체 클래스들의 부모 클래스다. */
///* 2. 원랜 구조체를 쓰면되는 구조체의 부모 구조체가 없더라. */
//
//class CBounding abstract : public CBase
//{
//public:
//	typedef struct
//	{
//		_float3		vCenter;
//	}BOUND_DESC;
//
//protected:
//	CBounding(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
//	virtual ~CBounding() = default;
//
//public:
//	virtual HRESULT Initialize();
//	virtual void Update(_fmatrix WorldMatrix) = 0;
//	virtual void Render(PrimitiveBatch<VertexPositionColor>* pBatch) = 0;
//	virtual _bool Intersect(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding) = 0;
//	virtual _float3 Get_Center() = 0;
//
//protected:
//	ID3D11Device* m_pDevice = { nullptr };
//	ID3D11DeviceContext* m_pContext = { nullptr };
//
//	_bool					m_isColl = { false };
//
//public:
//	virtual void Free() override;
//};