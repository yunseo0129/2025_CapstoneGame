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
        // Phase 1: 게임 상태머신 이벤트
        PHASE_CHANGE,
        ROUND_START,
        ROUND_END,
        SCORE_UPDATE,
        TIMER_SYNC,
        // Phase 2: 매치 로스터
        ROSTER_INFO,
    };

    struct NetEvent {
        NetEventType    type;
        int             roomId    = -1;
        int             queueSize = 0;
        int             playerIds[ROOM_MAX_PLAYER] = {};

        // PHASE_CHANGE / ROUND_START / ROUND_END 공용
        unsigned char   phase          = 0;
        unsigned char   round          = 0;
        unsigned int    duration_ms    = 0;
        unsigned int    server_time_ms = 0;
        unsigned char   winner_team    = 0;
        unsigned char   score_a        = 0;
        unsigned char   score_b        = 0;

        // SCORE_UPDATE
        unsigned char   player_count   = 0;
        PlayerStatBrief stats[ROOM_MAX_PLAYER] = {};

        // TIMER_SYNC
        unsigned int    time_ms        = 0;

        // ROSTER_INFO
        unsigned char   roster_count   = 0;
        RosterEntry     roster[ROOM_MAX_PLAYER] = {};
    };

public:
    static NetworkClient* GetInstance();
    static void DestroyInstance();

    bool ConnectWithConsole();

    void RecvThread();          // 로비 수신 스레드
    void InstanceRecvThread();  // 인스턴스 서버 수신 스레드

    // 이동 패킷 전송 → 인스턴스 서버로 전송
    void Send_Move(unsigned short keyInput, float mouseYaw, const float* worldMatrix);

    // 캐릭터 선택 완료 통보 (Ready 클릭 시 1회) — 오프라인 모드에서는 무시
    void Send_CharSelect(unsigned char charType);
    // 인게임 페이즈 타이머 만료 → 서버에 다음 페이즈 준비 완료 알림
    void Send_PhaseReady(unsigned char phase);

    // ── 수동 방 관련 송신 (로비 소켓) ──────────────────────────────────
    void Send_CreateRoom();
    void Send_JoinRoomCode(int code);
    void Send_StartGame();
    void Send_LeaveRoom();
    void Send_QuickMatch();
    void Send_SelectSeat(unsigned char team, unsigned char slot);
    void Send_PlayerReady(bool ready);

    void Disconnect();

    bool IsConnected()      const { return m_bConnected; }
    bool IsLoggedIn()       const { return m_bLoggedIn; }
    bool IsMatched()        const { return m_bMatched; }
    bool IsInGame()         const { return m_bInGame || m_bOfflineMode; }
    bool IsOfflineMode()    const { return m_bOfflineMode; }
    bool IsGameStarting()   const { return m_bGameStarting; }
    void EnableOfflineMode()      { m_bOfflineMode = true; }

    int GetMyId()       const { return m_iMyId; }
    int GetRoomId()     const { return m_iRoomId; }
    int GetQueueSize()  const { return m_iQueueSize; }

    // ── 방 상태 조회 (스레드 안전) ──────────────────────────────────────
    struct RoomMember {
        int           id;
        char          name[20];
        unsigned char team  = 0xFF; // 0=RED, 1=BLUE, 0xFF=미선택
        unsigned char slot  = 0;    // 1~3, 0=미선택
        bool          ready = false;
    };
    struct RoomSnapshot {
        int                    code     = 0;
        int                    host_id  = -1;
        std::vector<RoomMember> members;
        bool                   join_pending = false;
        unsigned char          join_result  = 0;  // ROOM_JOIN_RESULT
    };
    RoomSnapshot GetRoomSnapshot();

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

    // ── 수동 방 상태 (recv 스레드가 쓰고, 메인 스레드가 읽음) ───────────
    std::mutex              m_roomLock;
    int                     m_roomCode      = 0;
    int                     m_roomHostId    = -1;
    std::vector<RoomMember> m_roomMembers;
    bool                    m_roomJoinPending = false;
    unsigned char           m_roomJoinResult  = 0;
    bool                    m_bGameStarting   = false;

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