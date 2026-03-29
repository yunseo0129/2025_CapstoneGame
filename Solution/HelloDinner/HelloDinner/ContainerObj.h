#pragma once
#include "GameObject.h"

class CContainerObj abstract : public CGameObject
{
public:
	struct CONTAINEROBJ_DESC : public CGameObject::GAMEOBJECT_DESC
	{
		_uint			iNumPartObj;
	};

protected:
	CContainerObj(EngineContext* pContext);
	CContainerObj(const CContainerObj& Prototype);
	virtual ~CContainerObj() = default;
public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(_float fTimeDelta) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual void Render(ID3D12GraphicsCommandList* _commandList) override;

	/* 이 객체를 구성하기위한 부품(몸, 헤드, 무기, 이펙트, 사운드) 객체들 */
protected:
	vector<CGameObject*>			m_PartObjects;

public:
	virtual CGameObject* Clone(void* pArg) = 0;
	virtual void Free() override;
};