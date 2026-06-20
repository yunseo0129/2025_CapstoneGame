#include "OverllapedEXP.h"

OverllapedEXP::OverllapedEXP()
{
	m_wsabuf.len = BUF_SIZE;
	m_wsabuf.buf = m_send_buf;
	m_comp_type = OP_RECV;
	ZeroMemory(&m_over, sizeof(m_over));
}

OverllapedEXP::OverllapedEXP(char* packet)
{
	unsigned char pkt_size = static_cast<unsigned char>(packet[0]);
	m_wsabuf.len = pkt_size;
	m_wsabuf.buf = m_send_buf;
	ZeroMemory(&m_over, sizeof(m_over));
	m_comp_type = OP_SEND;
	memcpy(m_send_buf, packet, pkt_size);
}