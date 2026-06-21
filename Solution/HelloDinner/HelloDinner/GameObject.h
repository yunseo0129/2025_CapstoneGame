#pragma once
#include "Transform.h"
#include "Renderer.h"

class CGameObject abstract : public CBase
{
public:
	struct GAMEOBJECT_DESC : public CTransform::TRANSFORM_DESC
	{
		_uint			iData;
	};
protected:
	CGameObject(EngineContext* _pcontext);
	CGameObject(const CGameObject& Prototype);
	~CGameObject() = default;

public:
	virtual HRESULT		Initialize_Prototype();
	virtual HRESULT		Initialize(void* pArg);

public:
	virtual void		Priority_Update(_float fTimeDelta);
	virtual void		Update(_float fTimeDelta);
	virtual void		Late_Update(_float fTimeDelta);
	virtual void		Render(ID3D12GraphicsCommandList* _commandList);
	virtual void		ShadowRender(ID3D12GraphicsCommandList* _commandList);
    virtual void		Render_Blend(ID3D12GraphicsCommandList* _commandList);

public:
	// Frustum Culling
	virtual bool Get_WorldBoundingSphere(_float3& pOutCenter, _float& pOutRadius) const { return false; }

public:
	class CComponent*	Find_Component(const _wstring& strComponentTag);
	const _float4x4* Get_WorldMatrix4x4Ptr();		//RootConstantBuffer에 WorldMatrix를 넘겨줄 때 사용
	bool			IsDead() { return m_bDead; }
	void			SetDead() { m_bDead = true; }
    bool			GetOnOff() { return m_bOnOff; }
    void            SetOnOff(bool _is) { m_bOnOff = _is; }
    void            TakeDamage(_uint _val);
    virtual void    Die(_float _val);

protected:
	HRESULT				Add_Component(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag, const _wstring& strComponentTag, CComponent** ppOut, void* pArg = nullptr);
    void                Cull_And_Submit(CRenderer::RENDERGROUP eGroup);

protected:
	class CGameInstance*						m_pGameInstance = { nullptr };
	EngineContext*								m_pContext = { nullptr };
	
	// 보유한 컴포넌트 정보
	map<const _wstring, class CComponent*>		m_Components;
	class CTransform* m_pTransformCom = { nullptr };

	_uint										m_iData = {};
	bool										m_bDead = false;
	bool										m_bOnOff = true;
    _int                                        m_ihealth = 100;
    _bool                                       m_isDie = false;

public:
	virtual CGameObject* Clone(void* pArg) = 0;
	virtual void			Free() override;

};