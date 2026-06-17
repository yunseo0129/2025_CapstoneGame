#pragma once
#include "Base.h"

class CComponent abstract : public CBase
{
protected:
	CComponent(EngineContext* _context);
	CComponent(const CComponent& Prototype);
	virtual ~CComponent() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg);

protected:
	EngineContext*			m_pContext = { nullptr };
	class CGameInstance*	m_pGameInstance = { nullptr };
	_bool					m_isCloned = { false };

public:
	virtual CComponent* Clone(void* pArg) = 0;
	virtual void Free() override;
};