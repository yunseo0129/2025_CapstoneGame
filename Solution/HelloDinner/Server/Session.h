#pragma once
#include "OverllapedEXP.h"

// 클라이언트 정보를 관리하는 클래스
class Session
{
	OverllapedEXP m_recv_over;

	public:
		mutex		m_s_lock;
		S_STATE		m_state;
		SOCKET		m_socket;

		// Player 정보
		PlayerInfo	m_player;
		WorldMatrixInfo m_worldMatrix;

		int			m_prev_remain;
		int			m_room_id;

		// 타임스탬프 관련
		unsigned int	m_lastClientTimestamp;	// 마지막으로 받은 클라이언트 타임스탬프 (ms)
		unsigned int	m_lastServerTimestamp;	// 해당 패킷을 서버가 수신한 시점의 서버 타임스탬프 (ms)

	public:
		Session();
		~Session() = default;

		// 서버 기준 현재 타임스탬프(ms) 반환
		static unsigned int GetServerTimestamp();

		// 클라이언트 타임스탬프를 수신하여 저장 + 지연시간 추정
		void UpdateTimestamp(unsigned int clientTimestamp);

		void Recv();

		void Send(void* packet);
		void Send_Login_Info_Packet();
		void Send_Move_Packet(int c_id);
		void Send_Add_Player_Packet(int c_id);
		void Send_Remove_Player_Packet(int c_id);
		void Send_Match_Wait_Packet(int queue_size);
		void Send_Match_Success_Packet(int room_id, int player_count, const int* player_ids);
};

