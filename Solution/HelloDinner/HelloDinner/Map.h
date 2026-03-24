#pragma once
#include "GameObject.h"

class CMap : public CGameObject
{
public:
	struct MAP_DESC : public CGameObject::GAMEOBJECT_DESC
	{
		_float3		vPosition = {};
		_float3		vRotation = {};		// degree ¥‹¿ß
		_float3		vScale = { 1.f, 1.f, 1.f };
		_wstring	strModelTag = L"";
		_uint		iModelLevelIndex = 0;
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

private:
	HRESULT Ready_Components();

private:
	class CModel* m_pModelCom = { nullptr };

	_wstring	m_strModelTag = L"";
	_uint		m_iModelLevelIndex = 0;

public:
	static CMap* Create(EngineContext* _pcontext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};