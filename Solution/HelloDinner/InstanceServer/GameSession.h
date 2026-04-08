#pragma once
#include "OverllapedEXP.h"

class GameSessionManager;

// 인스턴스 서버의 클라이언트 세션
class GameSession
{
    OverllapedEXP m_recv_over;

public:
    mutex       m_s_lock;
    S_STATE     m_state;
    SOCKET      m_socket;

    PlayerInfo      m_player;
    WorldMatrixInfo m_worldMatrix;

    int         m_prev_remain;
    int         m_room_id;
    int         m_lobby_player_id;  // 로비에서 부여받은 원래 id

    unsigned int    m_lastClientTimestamp;
    unsigned int    m_lastServerTimestamp;

public:
    GameSession();
    ~GameSession() = default;

    static unsigned int GetServerTimestamp();
    void UpdateTimestamp(unsigned int clientTimestamp);

    void Recv();
    void Send(void* packet);
    void Send_Add_Player_Packet(int c_id, GameSessionManager* gsm);
    void Send_Remove_Player_Packet(int c_id);
    void Send_Move_Packet(int c_id, GameSessionManager* gsm);
};