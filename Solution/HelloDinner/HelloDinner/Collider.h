#pragma once
#include "Component.h"



/* 객체에 씌워야할 충돌체를 의미한다. */
/* Sphere, AABB, OBB */
class CCollider final : public CComponent
{
public:
	enum COLLIDERTYPE { TYPE_SPHERE, TYPE_AABB, TYPE_OBB, TYPE_END };
private:
	CCollider(EngineContext* pContext);
	CCollider(const CCollider& Prototype);
	virtual ~CCollider() = default;

public:
	virtual HRESULT Initialize_Prototype(COLLIDERTYPE eColliderType);
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Update(_fmatrix WorldMatrix);
#ifdef _DEBUG
	virtual HRESULT Render(ID3D12GraphicsCommandList* pCommandList);
#endif	

public:
	_bool Intersect(const CCollider* pTargetCollider);
	_bool Get_OnOff() { return m_isOnOff; }
	void Set_OnOff(_bool isWhat) { m_isOnOff = isWhat; }
	_float3 Get_Center();
	class CBounding* Get_Bounding() { return m_pBounding; }

private:
	COLLIDERTYPE						m_eType = { TYPE_END };
	class CBounding* m_pBounding = { nullptr };
	_bool	m_isOnOff = { true };

#ifdef _DEBUG
	
	PrimitiveBatch<VertexPositionColor>* m_pBatch = { nullptr };
	BasicEffect* m_pEffect = { nullptr };

#endif

public:
	static CCollider* Create(EngineContext* pContext, COLLIDERTYPE eColliderType);
	virtual CComponent* Clone(void* pArg) override;
	virtual void Free() override;
};
