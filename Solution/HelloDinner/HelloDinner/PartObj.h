#pragma once

#include "GameObject.h"

class CPartObj abstract : public CGameObject
{
public:
	struct PARTOBJ_DESC : public CGameObject::GAMEOBJECT_DESC
	{
		const _float4x4* pParentMatrix;
	};
protected:
	CPartObj(EngineContext* pContext);
	CPartObj(const CPartObj& Prototype);
	virtual ~CPartObj() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(_float fTimeDelta) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual void Render(ID3D12GraphicsCommandList* _commandList) override;

protected:
	const _float4x4* m_pParentMatrix = {};
	_float4x4				m_WorldMatrix = {};

public:
	virtual void Free() override;
};