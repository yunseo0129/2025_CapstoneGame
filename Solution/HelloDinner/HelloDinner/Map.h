#pragma once
#include "GameObject.h"
#include "Collider.h"

class CMap : public CGameObject
{
public:
	struct MAP_DESC : public CGameObject::GAMEOBJECT_DESC
	{
		_float3		vPosition = {};
		_float3		vRotation = {};		// degree 단위
		_float3		vScale = { 1.f, 1.f, 1.f };
		_wstring	strModelTag = L"";
		_uint		iModelLevelIndex = 0;

		// collider 정보까지 넣어서 관리하자
		CCollider::COLLIDERTYPE eColliderType = CCollider::TYPE_END;
		_float3    vCenterCollider = {};
		_float3    vExtentsCollider = {};
		_float3    vRotationCollider = {};
		_float	   fRadius = 0.f;
	};

private:
	CMap(EngineContext* _pcontext);
	CMap(const CMap& Prototype);
	virtual ~CMap() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(_float fTimeDelta) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual void Render(ID3D12GraphicsCommandList* _commandList) override;
	virtual void ShadowRender(ID3D12GraphicsCommandList* _commandList) override;
	 
public:
	// Frustum Culling
	virtual bool Get_WorldBoundingSphere(_float3& pOutCenter, _float& pOutRadius) const override;

private:
	HRESULT Ready_Components();

private:
	class CModel* m_pModelCom = { nullptr };
	class CCollider* m_pColliderCom = { nullptr };

	_wstring	m_strModelTag = L"";
	_uint		m_iModelLevelIndex = 0;

	CCollider::COLLIDERTYPE m_eColliderType = CCollider::TYPE_END;
	_float3    m_vCenterCollider = {};
	_float3    m_vExtentsCollider = {};
	_float3    m_vRotationCollider = {};
	_float	   m_fRadius = 0.f;

public:
	static CMap* Create(EngineContext* _pcontext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};