#pragma once
#include "pch.h"

class OverllapedEXP
{
public:
    WSAOVERLAPPED   m_over;
    WSABUF          m_wsabuf;
    char            m_send_buf[BUF_SIZE];
    COMP_TYPE       m_comp_type;

public:
    OverllapedEXP()
    {
        m_wsabuf.len = BUF_SIZE;
        m_wsabuf.buf = m_send_buf;
        m_comp_type = OP_RECV;
        ZeroMemory(&m_over, sizeof(m_over));
    }

    ~OverllapedEXP() = default;

    OverllapedEXP(char* packet)
    {
        m_wsabuf.len = static_cast<unsigned char>(packet[0]);
        m_wsabuf.buf = m_send_buf;
        ZeroMemory(&m_over, sizeof(m_over));
        m_comp_type = OP_SEND;
        memcpy(m_send_buf, packet, m_wsabuf.len);
    }
};