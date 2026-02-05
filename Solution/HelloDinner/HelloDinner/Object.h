#pragma once
#include "Transform.h"

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
	virtual void		Priority_Update(_float fTimeDelta);
	virtual void		Update(_float fTimeDelta);
	virtual void		Late_Update(_float fTimeDelta);
	virtual HRESULT		Render(const ComPtr<ID3D12GraphicsCommandList>& _commandList);

public:
	virtual CGameObject*	Clone(void* pArg) = 0;
	virtual void			Free() override;

public:
	class CComponent*	Find_Component(const _wstring& strComponentTag);

protected:
	HRESULT				Add_Component(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag, const _wstring& strComponentTag, CComponent** ppOut, void* pArg = nullptr);

protected:
	class CTransform*							m_pTransformCom = { nullptr };
	class CGameInstance*						m_pGameInstance = { nullptr };
	EngineContext*								m_pContext = { nullptr };
	map<const _wstring, class CComponent*>		m_Components;
	_uint										m_iData = {};
};