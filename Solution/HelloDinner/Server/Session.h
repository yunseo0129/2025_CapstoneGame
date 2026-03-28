#pragma once
#include "OverllapedEXP.h"

// 클라이언트 정보를 관리하는 클래스
class Session
{
	OverllapedEXP m_recv_over;

	public:
		mutex		m_s_lock;
		S_STATE		m_state;
		int			m_id;
		SOCKET		m_socket;

		// Player 정보
		float		m_Positionx, m_Positiony, m_Positionz;
		float		m_Rotationx, m_Rotationy, m_Rotationz;
		char		m_name[NAME_SIZE];

		int			m_prev_remain;
		int			m_room_id;
	public:
		Session();
		~Session() = default;

		void Recv();

		void Send(void* packet);
		void Send_Login_Info_Packet();
		void Send_Move_Packet(int c_id);
		void Send_Add_Player_Packet(int c_id);
		void Send_Remove_Player_Packet(int c_id);
		void Send_Match_Wait_Packet(int queue_size);
		void Send_Match_Success_Packet(int room_id, int player_count, const int* player_ids);
};

