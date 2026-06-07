#pragma once
#include "ContainerObj.h"
#include "Model.h"

class CPlayer_Pig final : public CContainerObj
{
public:
	struct PLAYER_PIG_DESC : public CContainerObj::CONTAINEROBJ_DESC
	{
		_float3 			vPos = _float3(1.f, 1.f, 1.f);
		_uint				iModelLevelIndex = 0;
		_float3				vRotation = {};
		_wstring			strModelTag = L"";
	};
	enum PLAYER_1RD_COLLIDER_TYPE { COLLIDER_MAIN, COLLIDER_HEAD, COLLIDER_ARM_UP_L, COLLIDER_ARM_UP_R, COLLIDER_ARM_LOW_L, COLLIDER_ARM_LOW_R, COLLIDER_THIGH_L, COLLIDER_THIGH_R, COLLIDER_SHIN_L, COLLIDER_SHIN_R, COLLIDER_BODY, COLLIDER_END };

private:
	CPlayer_Pig(EngineContext* pContext);
	CPlayer_Pig(const CPlayer_Pig& Prototype);
	virtual ~CPlayer_Pig() = default;

public:
	virtual HRESULT		Initialize_Prototype() override;
	virtual HRESULT		Initialize(void* pArg) override;
	virtual void		Priority_Update(_float fTimeDelta) override;
	virtual void		Update(_float fTimeDelta) override;
	virtual void		Late_Update(_float fTimeDelta) override;
	virtual void		Render(ID3D12GraphicsCommandList* _commandList) override;
	virtual void		ShadowRender(ID3D12GraphicsCommandList* _commandList) override;
    virtual bool        Get_WorldBoundingSphere(_float3& outCenter, _float& outRadius) const override;

	//CCollider* Get_CollisionCom() const { return m_pColliderCom; }
	//virtual void TakeDamage(int iDamage) PURE;

private:
	virtual HRESULT				Ready_PartObjects();
	virtual HRESULT				Ready_Components();

private:
	class CModel* m_pModelCom = { nullptr };
	vector<class CCollider*> m_vColliderComs;
	vector<class CCollider*> m_vMapColliderComs;
	_uint				m_iState = 0;
	_int				m_iHealth = 0;
	_wstring			m_strModelTag = L"";
	_uint				m_iModelLevelIndex = 0;

public:
	void Apply_NetworkMatrix(const float* pMatrix);

public:
	static CPlayer_Pig* Create(EngineContext* pContext);
	virtual CGameObject* Clone(void* pArg);
	virtual void Free() override;


};