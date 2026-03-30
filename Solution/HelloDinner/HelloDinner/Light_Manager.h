#pragma once

#include "Base.h"

class CLight_Manager final : public CBase
{
private:
	CLight_Manager(EngineContext* pContext);
	virtual ~CLight_Manager() = default;

public:
	const LIGHT_DESC* Get_LightDesc(_uint iIndex) const;

public:
	HRESULT Initialize();
	HRESULT Add_Light(const LIGHT_DESC& LightDesc);

private:
	EngineContext* m_pContext = { nullptr };
	list<class CLight*>				m_Lights;

public:
	static CLight_Manager* Create(EngineContext* pContext);
	virtual void Free() override;
};