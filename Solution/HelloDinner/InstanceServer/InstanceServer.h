#pragma once
#include "GameSessionManager.h"

class InstanceServer
{
public:
    InstanceServer() = default;
    ~InstanceServer();

    bool Initialize(int instance_id, unsigned short port, const char* lobby_ip);
    void Run();

private:
    void WorkerThread();
    void AcceptClient();
    void PrintServerIP();

    // 로비 서버에 등록 + Heartbeat 전송
    bool ConnectToLobby(const char* lobby_ip);
    void HeartbeatThread();

    // 로비로부터 방 정보 수신
    void LobbyRecvThread();

    int             m_instance_id = -1;
    unsigned short  m_port = 0;

    SOCKET          m_listen_socket = INVALID_SOCKET;
    SOCKET          m_client_socket = INVALID_SOCKET;
    SOCKET          m_lobby_socket = INVALID_SOCKET;   // 로비와의 내부 통신
    HANDLE          m_h_iocp = nullptr;
    OverllapedEXP   m_accept_over;

    char            m_my_ip[16] = {};
};