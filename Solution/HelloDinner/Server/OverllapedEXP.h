#pragma once
#include "pch.h"

// OVERLAPPED 구조체와 WSABUF 구조체를 확장하여 관리하는 클래스
class OverllapedEXP 
{
public:
	WSAOVERLAPPED	m_over;
	WSABUF			m_wsabuf;
	char			m_send_buf[BUF_SIZE];
	COMP_TYPE		m_comp_type;

public:
	OverllapedEXP();
	~OverllapedEXP() = default;
	OverllapedEXP(char* packet);
};
