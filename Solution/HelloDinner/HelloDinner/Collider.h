#pragma once
#include "Component.h"



/* 객체에 씌워야할 충돌체를 의미한다. */
/* Sphere, AABB, OBB */
class CCollider final : public CComponent
{
public:
	enum COLLIDERTYPE { TYPE_SPHERE, TYPE_AABB, TYPE_OBB, TYPE_END };
    enum PLAYER_1RD_COLLIDER_TYPE { COLLIDER_MAIN, COLLIDER_HEAD, COLLIDER_ARM_UP_L, COLLIDER_ARM_UP_R, COLLIDER_ARM_LOW_L, COLLIDER_ARM_LOW_R, COLLIDER_THIGH_L, COLLIDER_THIGH_R, COLLIDER_SHIN_L, COLLIDER_SHIN_R, COLLIDER_BODY, COLLIDER_END };

private:
	CCollider(EngineContext* pContext);
	CCollider(const CCollider& Prototype);
	virtual ~CCollider() = default;

public:
	virtual HRESULT Initialize_Prototype(COLLIDERTYPE eColliderType);
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Update(_fmatrix WorldMatrix);
	virtual void Update();
#ifdef _DEBUG
	virtual HRESULT Render(ID3D12GraphicsCommandList* pCommandList);
#endif	

public:
	_bool Intersect(const CCollider* pTargetCollider);
	_bool Intersect_Offset(const CCollider* pTargetCollider, const _float3& vOffset);
	_float3 Get_CollisionNormal(const CCollider* pTargetCollider);
	_bool Get_Enable() const { return m_isEnable; }
	void Set_Enable(_bool isWhat) { m_isEnable = isWhat; }
	_float3 Get_Center();
	class CBounding* Get_Bounding() { return m_pBounding; }
    _bool Get_SphereBound(_float3& outCenter, _float& outRadius) const;
    _bool Get_AABBBound(_float3& outCenter, _float3& outExtents) const;
    void  Set_Owner(class CGameObject* p) { m_pOwner = p; }
    class CGameObject* Get_Owner() const { return m_pOwner; }
    _bool IntersectsRay(FXMVECTOR rayOrigin, FXMVECTOR rayDir, _float& distance);
    void  Set_PartNum(_uint _num) { m_iPartNum = _num; }
    _uint Get_PartNum() { return m_iPartNum; }

private:
	COLLIDERTYPE						m_eType = { TYPE_END };
	class CBounding* m_pBounding = { nullptr };
	_bool	m_isEnable = { true };
	const _float4x4* m_pSocketMatrix = { nullptr };		// 소켓 매트릭스 (애니메이션이 있는 모델의 경우, 본에 따라 충돌체가 움직여야하므로)
	const _float4x4* m_pParentMatrix = { nullptr };
	class CGameObject* m_pOwner = { nullptr };
    _uint               m_iPartNum;

#ifdef _DEBUG
	
	PrimitiveBatch<VertexPositionColor>* m_pBatch = { nullptr };
	BasicEffect* m_pEffect = { nullptr };

#endif

public:
	static CCollider* Create(EngineContext* pContext, COLLIDERTYPE eColliderType);
	virtual CComponent* Clone(void* pArg) override;
	virtual void Free() override;
};
