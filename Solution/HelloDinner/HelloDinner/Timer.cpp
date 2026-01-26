#include "Timer.h"

CTimer::CTimer() : m_prev{ 0 }, m_deltaTime{ 0.f }
{
	QueryPerformanceFrequency(&m_frequency);	// Å¸ÀÌ¸ÓÀÇ ÁÖÆÄ¼ö È¹µæ
}

void CTimer::Tick()
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	m_deltaTime = static_cast<FLOAT>((now.QuadPart - m_prev.QuadPart) / static_cast<FLOAT>(m_frequency.QuadPart));
	m_prev = now;
}

FLOAT CTimer::GetElapsedTime() const
{
	return m_deltaTime;
}
