#pragma once
#include "SessionManager.h"

class IOCPServer
{
public:
    IOCPServer() = default;
    ~IOCPServer();

    bool Initialize();
    void Run();

private:
    void WorkerThread();
    void AcceptClient();
    void PrintServerIP();

    SOCKET			m_listen_socket = INVALID_SOCKET;
    SOCKET			m_client_socket = INVALID_SOCKET;
    HANDLE			m_h_iocp = nullptr;
    OverllapedEXP	m_accept_over;
};