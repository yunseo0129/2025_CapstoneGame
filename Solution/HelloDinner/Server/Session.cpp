#include "Session.h"
#include "SessionManager.h"

Session::Session()
{
	m_player = {};
	m_camera = {};
	m_socket = 0;
	m_state = ST_FREE;
	m_prev_remain = 0;
	m_room_id = -1;
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
	p.cameraPosX = m_camera.positionX;
	p.cameraPosY = m_camera.positionY;
	p.cameraPosZ = m_camera.positionZ;
	p.cameraYaw = m_camera.yaw;
	p.cameraPitch = m_camera.pitch;

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
	p.cameraPosX = target.m_camera.positionX;
	p.cameraPosY = target.m_camera.positionY;
	p.cameraPosZ = target.m_camera.positionZ;
	p.cameraYaw = target.m_camera.yaw;
	p.cameraPitch = target.m_camera.pitch;
	p.cameraLookX = target.m_camera.lookX;
	p.cameraLookY = target.m_camera.lookY;
	p.cameraLookZ = target.m_camera.lookZ;

	Send(&p);
}

void Session::Send_Add_Player_Packet(int c_id)
{
	auto& target = SessionManager::GetInstance()->GetClient(c_id);
	SC_ADD_PLAYER_PACKET p;
	p.size = sizeof(SC_ADD_PLAYER_PACKET);
	p.type = SC_ADD_PLAYER;
	p.id = c_id;
	p.cameraPosX = target.m_camera.positionX;
	p.cameraPosY = target.m_camera.positionY;
	p.cameraPosZ = target.m_camera.positionZ;
	p.cameraYaw = target.m_camera.yaw;
	p.cameraPitch = target.m_camera.pitch;
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