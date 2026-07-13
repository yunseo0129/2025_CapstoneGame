#pragma once

#include <atomic>
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
        // Phase 3: 맵 로드 완료
        MAP_LOADED,
        // Phase 4: 캐릭터 선택 릴레이
        CHAR_SELECT,
        // Phase 5: 스폰 위치 선택 릴레이
        SPAWN_SELECT,
        // Phase 3: 히트 판정
        HIT,
        DEATH,
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

        // MAP_LOADED
        unsigned char   map_slot       = 0;

        // CHAR_SELECT
        int             char_select_player_id = -1;
        unsigned char   char_select_type      = 0;

        // SPAWN_SELECT
        int             spawn_select_player_id = -1;
        float           spawn_x = 0.f;
        float           spawn_y = 0.f;
        float           spawn_z = 0.f;

        // HIT / DEATH
        int             hit_shooter_id   = -1;
        int             hit_victim_id    = -1;
        short           hit_victim_hp    = 0;
        unsigned char   hit_part_num     = 0;
        float           hit_pos[3]       = {};
        int             death_killer_id  = -1;
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
    // 스폰 위치 선택 통보 (SHOP 종료 시 1회) — 오프라인 모드에서는 무시
    void Send_SpawnSelect(const float* worldPos);
    // 인게임 페이즈 타이머 만료 → 서버에 다음 페이즈 준비 완료 알림
    void Send_PhaseReady(unsigned char phase);
    // 맵 로드 완료 → 인스턴스 서버로 통보
    void Send_MapLoaded(unsigned char slot);
    // 히트 신고 → 인스턴스 서버로 전송 (서버가 LOS 검증 후 SC_HIT 브로드캐스트)
    void Send_Hit(int victim_id, unsigned char part_num, const float hit_pos[3]);

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
    SOCKET               m_socket         = INVALID_SOCKET;
    std::atomic<bool>    m_bConnected     = false;
    std::atomic<bool>    m_bLoggedIn      = false;
    std::atomic<bool>    m_bMatched       = false;

    // 인스턴스 서버 TCP 소켓
    SOCKET               m_instanceSocket = INVALID_SOCKET;
    std::atomic<bool>    m_bInGame        = false;
    bool                 m_bOfflineMode   = false;
    char                 m_szAuthToken[32] = {};

    std::atomic<int>     m_iMyId      = -1;
    int                  m_iRoomId    = -1;
    int                  m_iQueueSize = 0;

    float   m_worldMatrix[16] = {};
    char    m_szName[NAME_SIZE] = {};

    // ── 수동 방 상태 (recv 스레드가 쓰고, 메인 스레드가 읽음) ───────────
    std::mutex              m_roomLock;
    int                     m_roomCode      = 0;
    int                     m_roomHostId    = -1;
    std::vector<RoomMember> m_roomMembers;
    bool                    m_roomJoinPending = false;
    unsigned char           m_roomJoinResult  = 0;
    std::atomic<bool>       m_bGameStarting   = false;

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