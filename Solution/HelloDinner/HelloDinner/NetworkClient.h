#pragma once

#include "stdafx.h"

class NetworkClient
{
public:
    // 네트워크 이벤트 종류
    enum class NetEventType {
        PLAYER_ADD,
        PLAYER_REMOVE,
        PLAYER_MOVE,
        MATCH_WAIT,        // 매칭 대기 중
        MATCH_SUCCESS,     // 매칭 성공
    };

    // 메인 스레드에서 처리할 이벤트 구조체
    struct NetEvent {
        NetEventType	type;
        int				id = -1;
        _float3			position = {};
        _float3			rotation = {};
        char			name[NAME_SIZE] = {};
        int				roomId = -1;
        int				queueSize = 0;
        int				playerIds[ROOM_MAX_PLAYER] = {};
    };

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
    bool IsMatched() const { return m_bMatched; }

    int GetMyId() const { return m_iMyId; }
    int GetRoomId() const { return m_iRoomId; }
    int GetQueueSize() const { return m_iQueueSize; }

    // 다른 플레이어 정보
    struct PlayerInfo {
        int		    id = -1;
        _float3	    position = {};
        _float3     rotation = {};
        char	    name[NAME_SIZE] = {};
        bool	    active = false;
    };

    const PlayerInfo& GetPlayer(int id) const { return m_players[id]; }

    // 메인 스레드에서 호출: 대기 중인 이벤트를 꺼내감
    void PopAllEvents(std::vector<NetEvent>& outEvents);

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
    bool		m_bMatched = false;

    int			m_iMyId = -1;
    int			m_iRoomId = -1;
    int			m_iQueueSize = 0;

    _float3	    m_vMyPosition = {};
    _float3     m_vMyRotation = {};

    char		m_szName[NAME_SIZE] = {};

    PlayerInfo	m_players[MAX_USER] = {};

    // 이벤트 큐 (RecvThread에서 push, 메인 스레드에서 pop)
    std::vector<NetEvent>	m_pendingEvents;
    std::mutex				m_eventLock;

    std::thread	m_recvThread;
    std::mutex	m_lock;
};