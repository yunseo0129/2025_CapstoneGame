#pragma once
#include "OverllapedEXP.h"

// 클라이언트 세션을 관리하는 클래스
class Session
{
	OverllapedEXP m_recv_over;

	public:
		mutex		m_s_lock;
		S_STATE		m_state;
		int			m_id;
		SOCKET		m_socket;
		short		x, y, z;
		char		m_name[NAME_SIZE];
		int			m_prev_remain;
		int			m_last_move_time;

	public:
		Session();
		~Session() = default;

		void Recv();

		void Send(void* packet);
		void Send_Login_Info_Packet();
		void Send_Move_Packet(int c_id);
		void Send_Add_Player_Packet(int c_id);
		void Send_Remove_Player_Packet(int c_id);
};

