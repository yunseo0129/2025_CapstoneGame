#pragma once
#include "SessionManager.h"
#include "MatchManager.h"
#include "InstanceManager.h"

class LobbyServer
{
public:
    LobbyServer() = default;
    ~LobbyServer();

    bool Initialize();
    void Run();

private:
    void WorkerThread();
    void AcceptClient();
    void PrintServerIP();

    SOCKET          m_listen_socket = INVALID_SOCKET;
    SOCKET          m_client_socket = INVALID_SOCKET;
    HANDLE          m_h_iocp = nullptr;
    OverllapedEXP   m_accept_over;
};