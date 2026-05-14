#pragma once

#include "Base.h"

class CTimer_Manager final : public CBase
{
private:
	CTimer_Manager(void);
	virtual ~CTimer_Manager(void) = default;

public:
	_float Get_TimeDelta(const _wstring& strTimerTag);

public:
	HRESULT	Add_Timer(const _wstring& strTimerTag);
	void Update_TimeDelta(const _wstring& strTimerTag);


private:
	map<const _wstring, class CTimer*>			m_mapTimer;

private:
	class CTimer* Find_Timer(const _wstring& strTimerTag);


public:
	static CTimer_Manager* Create();
	virtual void Free(void) override;
};
