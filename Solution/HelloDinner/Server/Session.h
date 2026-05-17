#pragma once
#include "OverllapedEXP.h"

// 클라이언트 세션을 관리하는 클래스 (로비 전용)
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

		static unsigned int GetServerTimestamp();
		void UpdateTimestamp(unsigned int clientTimestamp);

		void Recv();
		void Send(void* packet);

		// 로비에서 사용하는 패킷만 유지
		void Send_Login_Info_Packet();
		void Send_Match_Wait_Packet(int queue_size);
};

