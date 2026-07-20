#pragma once
#include "pch.h"

// 서버 권위 페이즈 번호 (SC_PHASE_CHANGE_PACKET.phase 와 1:1 대응)
enum class ROOM_PHASE : unsigned char {
    CHARSELECT = 0,
    SCOREBOARD = 1,
    SHOP       = 2,
    PLAYING    = 3,
    GAMEOVER   = 4,
    WAITING    = 0xFF   // 플레이어 입장 대기 중 (클라에 송신 안 함)
};

struct PlayerRosterEntry {
    int           player_id = -1;
    char          name[NAME_SIZE] = {};
    unsigned char team = 0xFF;  // 0=RED, 1=BLUE, 0xFF=미선택
    unsigned char slot = 0;     // 1~3, 0=미선택
};

struct RoomPhaseState {
    ROOM_PHASE  phase             = ROOM_PHASE::WAITING;
    int         round             = 1;
    int         score[2]          = {};      // [0]=팀A, [1]=팀B 라운드 승수
    float       timer_sec         = 0.f;     // 현재 페이즈 남은 시간
    float       sync_elapsed      = 0.f;     // 마지막 SC_TIMER_SYNC 전송 후 경과 시간
    int         phase_ready_count = 0;       // CS_PHASE_READY 수신 인원 (이번 페이즈)
    int         map_loaded_count  = 0;       // CS_MAP_LOADED 수신 인원 (이번 SCOREBOARD)
    int         expected          = 0;       // 입장 예정 인원 (IS_ROOM_NOTIFY.player_count)
    int         joined            = 0;       // CS_JOIN_ROOM 완료 인원
    bool        active            = false;
    // ── 라운드 결과 대기 (전멸 감지 후 5초간 PLAYING 페이즈 유지) ──
    bool        in_result         = false;   // 결과 대기 중 플래그
    float       result_timer      = 0.f;     // 결과 대기 남은 시간
    int         roster_count      = 0;
    PlayerRosterEntry roster[ROOM_MAX_PLAYER];
};

// 인스턴스 서버 전체 룸의 페이즈/타이머를 단일 스레드로 관리하는 싱글톤
class RoomPhaseManager {
public:
    static RoomPhaseManager* GetInstance() {
        static RoomPhaseManager inst;
        return &inst;
    }

    // InstanceServer::Run() 에서 1회 호출
    void StartTimerThread();

    // 이벤트 진입점
    void OnRoomRegistered(const IS_ROOM_NOTIFY_PACKET& pkt);
    void OnPlayerJoined(int room_id);
    void OnPlayerLeft(int room_id);
    void OnRoundEnd(int room_id, int winner_team);       // Phase 3: 서버 히트 판정 후 호출
    void OnPlayerDied(int room_id);                      // 사망 발생 시 팀 전멸 여부 검사
    void OnPlayerPhaseReady(int room_id, unsigned char phase); // 클라 타이머 만료 알림
    void OnMapLoaded(int room_id, unsigned char slot);         // 클라 맵 로드 완료 알림

    // CS_MOVE 핸들러에서 현재 페이즈 조회 (물리 게이트용)
    ROOM_PHASE GetRoomPhase(int room_id);

private:
    RoomPhaseManager() = default;
    RoomPhaseManager(const RoomPhaseManager&) = delete;
    RoomPhaseManager& operator=(const RoomPhaseManager&) = delete;

    void TimerLoop();
    void TickRoom(int room_id, float dt);
    void TransitionTo(int room_id, ROOM_PHASE next);

    void Broadcast_PhaseChange(int room_id, ROOM_PHASE phase, int round);
    void Broadcast_TimerSync(int room_id);
    void Broadcast_RoundStart(int room_id, int round, unsigned int duration_ms);
    void Broadcast_RoundEnd(int room_id, unsigned char winner_team,
                            unsigned char score_a, unsigned char score_b);
    void Broadcast_ScoreUpdate(int room_id);
    void Broadcast_RosterInfo(int room_id);
    void Broadcast_MapLoaded(int room_id, unsigned char slot);

    // PLAYING 진입 시 각 플레이어를 선택 스폰 위치로 순간이동 + 전원에게 동기화
    void ApplySpawnPositions(int room_id);
    // CHARSELECT/SCOREBOARD 진입 시 팀 시작 지점(y=25)으로 이동 + 즉시 브로드캐스트
    void ApplyTeamSpawnPositions(int room_id);

    array<RoomPhaseState, MAX_ROOM> m_rooms;
    mutex m_lock;

    static constexpr float CHARSELECT_DURATION = 30.f;
    static constexpr float SCOREBOARD_DURATION  = 8.f;
    static constexpr float SHOP_DURATION        = 15.f;
    static constexpr float ROUND_DURATION       = 100.f;
    static constexpr float RESULT_DURATION      = 5.f;  // 전멸 후 결과 대기 시간(초)
    static constexpr int   MAX_ROUND_COUNT      = 10;
    static constexpr float SYNC_INTERVAL        = 1.f;  // SC_TIMER_SYNC 전송 주기(초)
};
