#pragma once

#include "Base.h"

class CTimer final : public CBase
{
private:
	CTimer(void);
	virtual ~CTimer(void) = default;

public:
	_float Get_TimeDelta(void) const {
		return m_fTimeDelta;
	}

public:
	HRESULT	Ready_Timer(void);
	void Update_Timer(void);

private:
	LARGE_INTEGER			m_FrameTime = {};
	LARGE_INTEGER			m_FixTime = {};
	LARGE_INTEGER			m_LastTime = {};
	LARGE_INTEGER			m_CpuTick = {};

private:
	_float					m_fTimeDelta = {};

public:
	static CTimer* Create(void);
	virtual void Free(void) override;

};

