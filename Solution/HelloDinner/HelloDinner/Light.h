#pragma once

#include "Base.h"

class CLight final : public CBase
{
private:
	CLight(EngineContext* pContext);
	virtual ~CLight() = default;

public:
	const LIGHT_DESC* Get_LightDesc() const {
		return &m_LightDesc;
	}

public:
	HRESULT Initialize(const LIGHT_DESC& LightDesc);

private:
	EngineContext* m_pContext = { nullptr };
	LIGHT_DESC				m_LightDesc{};

public:
	static CLight* Create(EngineContext* pContext, const LIGHT_DESC& LightDesc);
	virtual void Free() override;
};