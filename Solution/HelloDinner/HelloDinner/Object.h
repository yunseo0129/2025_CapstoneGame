#pragma once
#include "Base.h"
// 컴포넌트, 트랜스폼 컴포넌트 만들고 포함하기

class CGameObject abstract : public CBase
{
public:
	typedef struct : public CTransform::TRANSFORM_DESC
	{
		_uint			iData;
	}GAMEOBJECT_DESC;
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