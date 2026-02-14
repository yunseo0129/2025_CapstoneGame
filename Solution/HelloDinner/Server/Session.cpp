#include "Session.h"

Session::Session()
{
	m_id = -1;
	m_socket = 0;
	x = y = 0;
	m_name[0] = 0;
	m_state = ST_FREE;
	m_prev_remain = 0;
}


void Session::Recv()
{
	DWORD recv_flag = 0;
	memset(&m_recv_over.m_over, 0, sizeof(m_recv_over.m_over));
	m_recv_over.m_wsabuf.len = BUF_SIZE - m_prev_remain;
	m_recv_over.m_wsabuf.buf = m_recv_over.m_send_buf + m_prev_remain;

	WSARecv(m_socket, &m_recv_over.m_wsabuf, 1, 0, &recv_flag,
		&m_recv_over.m_over, 0);
}

void Session::Send(void* packet)
{
	OverllapedEXP* sdata = new OverllapedEXP{ reinterpret_cast<char*>(packet) };

	WSASend(m_socket, &sdata->m_wsabuf, 1, 0, 0, &sdata->m_over, 0);
}

void Session::Send_Login_Info_Packet()
{
	SC_LOGIN_INFO_PACKET p;
	p.id = m_id;
	p.size = sizeof(SC_LOGIN_INFO_PACKET);
	p.type = SC_LOGIN_INFO;
	p.x = x;
	p.y = y;

	Send(&p);
}

void Session::Send_Move_Packet(int c_id)
{

}

void Session::Send_Add_Player_Packet(int c_id)
{

}

void Session::Send_Remove_Player_Packet(int c_id)
{
	SC_REMOVE_PLAYER_PACKET p;
	p.id = c_id;
	p.size = sizeof(p);
	p.type = SC_REMOVE_PLAYER;

	Send(&p);
}
