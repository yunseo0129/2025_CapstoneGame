#pragma once
#include "GameObject.h"

class CChick_3rd : public CGameObject
{
public:
	struct Player_3rd_DESC : public CGameObject::GAMEOBJECT_DESC
	{
		_float3		vPosition = {};
		_float3		vRotation = {};		// degree ¥‹¿ß
		_float3		vScale = { 1.f, 1.f, 1.f };
		_wstring	strModelTag = L"";
		_uint		iModelLevelIndex = 0;
	};

private:
	CChick_3rd(EngineContext* _pcontext);
	CChick_3rd(const CChick_3rd& Prototype);
	virtual ~CChick_3rd() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(_float fTimeDelta) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual void Render(ID3D12GraphicsCommandList* _commandList) override;

private:
	HRESULT Ready_Components();

private:
	class CModel* m_pModelCom = { nullptr };

	_wstring	m_strModelTag = L"";
	_uint		m_iModelLevelIndex = 0;

public:
	static CChick_3rd* Create(EngineContext* _pcontext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};