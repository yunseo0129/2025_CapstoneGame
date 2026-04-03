#include "Session.h"
#include "SessionManager.h"

Session::Session()
{
	m_player = {};
	m_worldMatrix = WorldMatrixInfo{};
	m_socket = 0;
	m_state = ST_FREE;
	m_prev_remain = 0;
	m_room_id = -1;
	m_lastClientTimestamp = 0;
	m_lastServerTimestamp = 0;
}

unsigned int Session::GetServerTimestamp()
{
	auto now = steady_clock::now();
	return static_cast<unsigned int>(
		duration_cast<milliseconds>(now.time_since_epoch()).count());
}

void Session::UpdateTimestamp(unsigned int clientTimestamp)
{
	m_lastClientTimestamp = clientTimestamp;
	m_lastServerTimestamp = GetServerTimestamp();
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
	p.size = sizeof(SC_LOGIN_INFO_PACKET);
	p.type = SC_LOGIN_INFO;
	p.id = m_player.id;
	memcpy(p.worldMatrix, m_worldMatrix.m, sizeof(float) * 16);

	Send(&p);
}

void Session::Send_Move_Packet(int c_id)
{
	auto& target = SessionManager::GetInstance()->GetClient(c_id);
	SC_MOVE_PLAYER_PACKET p;
	p.size = sizeof(SC_MOVE_PLAYER_PACKET);
	p.type = SC_MOVE_PLAYER;
	p.id = c_id;
	p.keyInput = target.m_player.keyInput;
	p.timestamp = target.m_lastServerTimestamp;
	memcpy(p.worldMatrix, target.m_worldMatrix.m, sizeof(float) * 16);

	Send(&p);
}

void Session::Send_Add_Player_Packet(int c_id)
{
	auto& target = SessionManager::GetInstance()->GetClient(c_id);
	SC_ADD_PLAYER_PACKET p;
	p.size = sizeof(SC_ADD_PLAYER_PACKET);
	p.type = SC_ADD_PLAYER;
	p.id = c_id;
	memcpy(p.worldMatrix, target.m_worldMatrix.m, sizeof(float) * 16);
	strcpy_s(p.name, target.m_player.name);

	Send(&p);
}

void Session::Send_Remove_Player_Packet(int c_id)
{
	SC_REMOVE_PLAYER_PACKET p;
	p.size = sizeof(SC_REMOVE_PLAYER_PACKET);
	p.type = SC_REMOVE_PLAYER;
	p.id = c_id;

	Send(&p);
}

void Session::Send_Match_Wait_Packet(int queue_size)
{
	SC_MATCH_WAIT_PACKET p;
	p.size = sizeof(SC_MATCH_WAIT_PACKET);
	p.type = SC_MATCH_WAIT;
	p.queue_size = queue_size;

	Send(&p);
}

void Session::Send_Match_Success_Packet(int room_id, int player_count, const int* player_ids)
{
	SC_MATCH_SUCCESS_PACKET p;
	p.size = sizeof(SC_MATCH_SUCCESS_PACKET);
	p.type = SC_MATCH_SUCCESS;
	p.room_id = room_id;
	p.player_count = player_count;
	memcpy(p.player_ids, player_ids, sizeof(int) * player_count);

	Send(&p);
}