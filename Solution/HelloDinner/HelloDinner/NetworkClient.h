#pragma once

#include "stdafx.h"

class NetworkClient
{
public:
    static NetworkClient* GetInstance();
    static void DestroyInstance();

    // 콘솔 창을 띄우고 IP, 이름을 입력받아 접속
    bool ConnectWithConsole();

    // 패킷 수신 처리 루프 (별도 스레드)
    void RecvThread();

    // 이동 패킷 전송
    void Send_Move(char direction);

    // 접속 해제
    void Disconnect();

    bool IsConnected() const { return m_bConnected; }
    bool IsLoggedIn() const { return m_bLoggedIn; }

    int GetMyId() const { return m_iMyId; }
    short GetMyX() const { return m_sMyX; }
    short GetMyY() const { return m_sMyY; }
    short GetMyZ() const { return m_sMyZ; }

    // 다른 플레이어 정보
    struct PlayerInfo {
        int		id = -1;
        short	x = 0, y = 0, z = 0;
        char	name[NAME_SIZE] = {};
        bool	active = false;
    };

    const PlayerInfo& GetPlayer(int id) const { return m_players[id]; }

private:
    NetworkClient() = default;
    ~NetworkClient();

    void Send(void* packet, int size);
    void ProcessPacket(char* packet);

    // 콘솔 창 닫기
    void CloseConsole();

private:
    static NetworkClient* s_pInstance;

    SOCKET		m_socket = INVALID_SOCKET;
    bool		m_bConnected = false;
    bool		m_bLoggedIn = false;

    int			m_iMyId = -1;
    short		m_sMyX = 0;
    short		m_sMyY = 0;
    short		m_sMyZ = 0;
    char		m_szName[NAME_SIZE] = {};

    PlayerInfo	m_players[MAX_USER] = {};

    std::thread	m_recvThread;
    std::mutex	m_lock;
};