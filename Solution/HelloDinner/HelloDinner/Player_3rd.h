#pragma once
#include "GameObject.h"

class CPlayer_3rd : public CGameObject
{
public:
	struct Player_3rd_DESC : public CGameObject::GAMEOBJECT_DESC
	{
		_float3		vPosition = {};
		_float3		vRotation = {};		// degree 단위
		_float3		vScale = { 1.f, 1.f, 1.f };
		_wstring	strModelTag = L"";
		_uint		iModelLevelIndex = 0;
	};

protected:
	CPlayer_3rd(EngineContext* _pcontext);
	CPlayer_3rd(const CPlayer_3rd& Prototype);
	virtual ~CPlayer_3rd() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(_float fTimeDelta) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual void Render(ID3D12GraphicsCommandList* _commandList) override;

protected:
	HRESULT Ready_Components();

protected:
	class CModel* m_pModelCom = { nullptr };

	_wstring	m_strModelTag = L"";
	_uint		m_iModelLevelIndex = 0;
	_uint		m_iState = 0;			// 애니메이션 상태

public:
	static CPlayer_3rd* Create(EngineContext* _pcontext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};