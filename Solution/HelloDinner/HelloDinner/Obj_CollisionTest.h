#pragma once
#include "GameObject.h"
#include "Collider.h"
class CObj_CollisionTest final : public CGameObject
{
public:
	struct Obj_CollisionTest_DESC : public CGameObject::GAMEOBJECT_DESC
	{
		_float3		vPosition = {};
		_float3		vRotation = {};		// degree 단위
		_float3		vScale = { 1.f, 1.f, 1.f };
		CCollider::COLLIDERTYPE	eColliderType = CCollider::TYPE_END;
	};

protected:
	CObj_CollisionTest ( EngineContext* _pcontext );
	CObj_CollisionTest ( const CObj_CollisionTest& Prototype );
	virtual ~CObj_CollisionTest () = default;

public:
	virtual HRESULT Initialize_Prototype () override;
	virtual HRESULT Initialize ( void* pArg ) override;
	virtual void Priority_Update ( _float fTimeDelta ) override;
	virtual void Update ( _float fTimeDelta ) override;
	virtual void Late_Update ( _float fTimeDelta ) override;
#ifdef _DEBUG
	virtual void Render ( ID3D12GraphicsCommandList* _commandList ) override;
#endif

protected:
	HRESULT Ready_Components ();

protected:
	CCollider* m_pColliderCom { nullptr };
	CCollider::COLLIDERTYPE	m_eColliderType = CCollider::TYPE_END;

public:
	static CObj_CollisionTest* Create ( EngineContext* _pcontext );
	virtual CGameObject* Clone ( void* pArg ) override;
	virtual void Free () override;
};

