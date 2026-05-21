#pragma once

#include <unordered_set>
#include "stdafx.h"
#include "NetPlayer.h"

class NetworkClient
{
public:
    enum class NetEventType {
        MATCH_WAIT,
        MATCH_SUCCESS,
    };

    struct NetEvent {
        NetEventType    type;
        int             roomId = -1;
        int             queueSize = 0;
        int             playerIds[ROOM_MAX_PLAYER] = {};
    };

public:
    static NetworkClient* GetInstance();
    static void DestroyInstance();

    bool ConnectWithConsole();

    void RecvThread();          // 로비 수신 스레드
    void InstanceRecvThread();  // 인스턴스 서버 수신 스레드

    // 이동 패킷 전송 → 인스턴스 서버로 전송
    void Send_Move(unsigned char keyInput, float mouseYaw, const float* worldMatrix);

    void Disconnect();

    bool IsConnected()   const { return m_bConnected; }
    bool IsLoggedIn()    const { return m_bLoggedIn; }
    bool IsMatched()     const { return m_bMatched; }
    bool IsInGame()      const { return m_bInGame || m_bOfflineMode; }
    bool IsOfflineMode() const { return m_bOfflineMode; }
    void EnableOfflineMode()   { m_bOfflineMode = true; }

    int GetMyId()       const { return m_iMyId; }
    int GetRoomId()     const { return m_iRoomId; }
    int GetQueueSize()  const { return m_iQueueSize; }

    NetPlayer& GetPlayer(int id)            { return m_players[id]; }
    const NetPlayer& GetPlayer(int id) const{ return m_players[id]; }

    void PopAllPlayerEvents(std::vector<NetPlayer::Event>& outEvents);
    void PopAllMatchEvents(std::vector<NetEvent>& outEvents);

private:
    NetworkClient() = default;
    ~NetworkClient();

    void Send(void* packet, int size);
    void SendToInstance(void* packet, int size);    // ← 추가
    void ProcessLobbyPacket(char* packet);          // 로비 패킷 처리
    void ProcessInstancePacket(char* packet);       // 인스턴스 패킷 처리

    bool ConnectToInstance(const char* ip, unsigned short port,
                           int room_id, const char* auth_token); // ← 추가

    void CloseConsole();

private:
    static NetworkClient* s_pInstance;

    // 로비 TCP 소켓
    SOCKET  m_socket         = INVALID_SOCKET;
    bool    m_bConnected     = false;
    bool    m_bLoggedIn      = false;
    bool    m_bMatched       = false;

    // 인스턴스 서버 TCP 소켓
    SOCKET  m_instanceSocket = INVALID_SOCKET;
    bool    m_bInGame        = false;
    bool    m_bOfflineMode   = false;
    char    m_szAuthToken[32] = {};

    int     m_iMyId      = -1;
    int     m_iRoomId    = -1;
    int     m_iQueueSize = 0;

    float   m_worldMatrix[16] = {};
    char    m_szName[NAME_SIZE] = {};

    NetPlayer m_players[MAX_USER];

    std::vector<NetEvent>       m_pendingMatchEvents;
    std::mutex                  m_matchEventLock;

    std::unordered_set<int>     m_activePlayerIds;
    std::mutex                  m_activePlayerLock;

    std::thread m_recvThread;
    std::thread m_instanceRecvThread;  // ← 추가

    float m_fSendInterval = 1.f / 20.f;
    float m_fSendTimer    = 0.f;

public:
    bool CanSendMove(float fTimeDelta) {
        m_fSendTimer += fTimeDelta;
        if (m_fSendTimer >= m_fSendInterval) {
            m_fSendTimer = 0.f;
            return true;
        }
        return false;
    }
};