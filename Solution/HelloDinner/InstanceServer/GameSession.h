#pragma once
#include "OverllapedEXP.h"

class GameSessionManager;

// �ν��Ͻ� ������ Ŭ���̾�Ʈ ����
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
    int         m_lobby_player_id;  // 로비에서 부여받은 글로벌 id
    int         m_iCharType = 0;    // 0=Pig, 1=Chick, 2=Blank (CS_CHAR_SELECT 수신 시 갱신)
    float       m_spawnPos[3] = {0.f, 0.f, 0.f}; // 스폰 위치 선택 (CS_SPAWN_SELECT 수신 시 갱신)

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
    void Send_Remove_Player_Packet(int c_id, GameSessionManager* gsm);
    void Send_Move_Packet(int c_id, GameSessionManager* gsm);
};