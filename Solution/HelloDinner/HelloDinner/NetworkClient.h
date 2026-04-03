#pragma once

#include "stdafx.h"
#include "NetPlayer.h"

class NetworkClient
{
public:
    // 비-플레이어 이벤트 (매칭 관련)
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

    // 콘솔 창을 열어 IP, 이름을 입력받아 접속
    bool ConnectWithConsole();

    // 패킷 수신 처리 루프 (수신 스레드)
    void RecvThread();

    // 이동 패킷 전송 (키인풋 + 마우스 회전 + 예측 월드행렬)
    void Send_Move(unsigned char keyInput, float mouseYaw, const float* worldMatrix);

    // 접속 해제
    void Disconnect();

    bool IsConnected() const { return m_bConnected; }
    bool IsLoggedIn() const { return m_bLoggedIn; }
    bool IsMatched() const { return m_bMatched; }

    int GetMyId() const { return m_iMyId; }
    int GetRoomId() const { return m_iRoomId; }
    int GetQueueSize() const { return m_iQueueSize; }

    // 플레이어 객체 직접 접근
    NetPlayer& GetPlayer(int id) { return m_players[id]; }
    const NetPlayer& GetPlayer(int id) const { return m_players[id]; }

    // 메인 스레드에서 호출: 모든 플레이어 이벤트를 한 번에 수집
    void PopAllPlayerEvents(std::vector<NetPlayer::Event>& outEvents);

    // 메인 스레드에서 호출: 매칭 관련 이벤트만 수집
    void PopAllMatchEvents(std::vector<NetEvent>& outEvents);

private:
    NetworkClient() = default;
    ~NetworkClient();

    void Send(void* packet, int size);
    void ProcessPacket(char* packet);

    // 콘솔 창 닫기
    void CloseConsole();

private:
    static NetworkClient* s_pInstance;

    SOCKET      m_socket = INVALID_SOCKET;
    bool        m_bConnected = false;
    bool        m_bLoggedIn = false;
    bool        m_bMatched = false;

    int         m_iMyId = -1;
    int         m_iRoomId = -1;
    int         m_iQueueSize = 0;

    float       m_worldMatrix[16] = {};

    char        m_szName[NAME_SIZE] = {};

    NetPlayer   m_players[MAX_USER];

    // 매칭 이벤트 큐
    std::vector<NetEvent>   m_pendingMatchEvents;
    std::mutex              m_matchEventLock;

    std::thread m_recvThread;
};