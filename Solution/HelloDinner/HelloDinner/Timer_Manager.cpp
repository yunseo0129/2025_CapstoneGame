#include "Timer_Manager.h"
#include "Timer.h"

CTimer_Manager::CTimer_Manager(void)
{

}

_float CTimer_Manager::Get_TimeDelta(const _wstring& strTimerTag)
{
	CTimer* pInstance = Find_Timer(strTimerTag);
	if (nullptr == pInstance)
		return 0.f;

	return pInstance->Get_TimeDelta();
}



HRESULT CTimer_Manager::Add_Timer(const _wstring& strTimerTag)
{
	if (nullptr != Find_Timer(strTimerTag))
		return E_FAIL;

	CTimer* pTimer = CTimer::Create();
	if (nullptr == pTimer)
		return E_FAIL;

	m_mapTimer.emplace(strTimerTag, pTimer);

	return S_OK;
}

void CTimer_Manager::Update_TimeDelta(const _wstring& strTimerTag)
{
	CTimer* pInstance = Find_Timer(strTimerTag);
	if (nullptr == pInstance)
		return;

	pInstance->Update_Timer();
}


CTimer* CTimer_Manager::Find_Timer(const _wstring& strTimerTag)
{
	auto	iter = m_mapTimer.find(strTimerTag);

	if (iter == m_mapTimer.end())
		return nullptr;

	return iter->second;
}



CTimer_Manager* CTimer_Manager::Create()
{
	return new CTimer_Manager();
}

void CTimer_Manager::Free(void)
{
	__super::Free();

	for (auto& Pair : m_mapTimer)
		Safe_Release(Pair.second);
	m_mapTimer.clear();
}
