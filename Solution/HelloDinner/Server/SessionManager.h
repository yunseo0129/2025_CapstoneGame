#pragma once
#include "Session.h"

class SessionManager
{
public:
    static SessionManager* GetInstance()
    {
        static SessionManager instance;
        return &instance;
    }

    Session& GetClient(int id) { return m_clients[id]; }
    array<Session, MAX_USER>& GetClients() { return m_clients; }

    int GetNewClientId();
    void ProcessPacket(int c_id, char* packet);
    void Disconnect(int c_id);

private:
    SessionManager() = default;
    ~SessionManager() = default;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    array<Session, MAX_USER> m_clients;
};