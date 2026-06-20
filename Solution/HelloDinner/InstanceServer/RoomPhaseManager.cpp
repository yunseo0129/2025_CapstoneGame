#include "RoomPhaseManager.h"
#include "GameSessionManager.h"

void RoomPhaseManager::StartTimerThread()
{
    thread(&RoomPhaseManager::TimerLoop, this).detach();
}

// 50ms 주기(20Hz)로 모든 룸의 타이머를 갱신
void RoomPhaseManager::TimerLoop()
{
    auto prev = steady_clock::now();
    while (true) {
        this_thread::sleep_for(chrono::milliseconds(50));
        auto now = steady_clock::now();
        float dt = duration_cast<chrono::microseconds>(now - prev).count() / 1'000'000.f;
        prev = now;

        for (int i = 0; i < MAX_ROOM; ++i)
            TickRoom(i, dt);
    }
}

// 락 없이 pending 전환을 결정한 뒤, 락 해제 후 실제 전환 수행 (데드락 방지)
void RoomPhaseManager::TickRoom(int room_id, float dt)
{
    ROOM_PHASE pending_phase  = ROOM_PHASE::WAITING;  // WAITING = 전환 없음
    int        pending_winner = -2;                    // -2 = 라운드 종료 없음

    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        if (!r.active || r.phase == ROOM_PHASE::WAITING || r.timer_sec <= 0.f)
            return;

        r.timer_sec -= dt;
        if (r.timer_sec > 0.f) return;

        switch (r.phase) {
        case ROOM_PHASE::CHARSELECT: pending_phase  = ROOM_PHASE::SCOREBOARD; break;
        case ROOM_PHASE::SCOREBOARD: pending_phase  = ROOM_PHASE::SHOP;       break;
        case ROOM_PHASE::SHOP:       pending_phase  = ROOM_PHASE::PLAYING;    break;
        case ROOM_PHASE::PLAYING:    pending_winner = 0;                      break; // 타임아웃 → 팀A 임시 승
        default: break;
        }
    }

    if (pending_winner >= -1)
        OnRoundEnd(room_id, pending_winner);
    else if (pending_phase != ROOM_PHASE::WAITING)
        TransitionTo(room_id, pending_phase);
}

void RoomPhaseManager::TransitionTo(int room_id, ROOM_PHASE next)
{
    int round = 1;
    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        if (!r.active) return;

        r.phase = next;
        switch (next) {
        case ROOM_PHASE::CHARSELECT: r.timer_sec = CHARSELECT_DURATION; break;
        case ROOM_PHASE::SCOREBOARD: r.timer_sec = SCOREBOARD_DURATION; break;
        case ROOM_PHASE::SHOP:       r.timer_sec = SHOP_DURATION;       break;
        case ROOM_PHASE::PLAYING:    r.timer_sec = ROUND_DURATION;      break;
        case ROOM_PHASE::GAMEOVER:   r.timer_sec = 0.f; r.active = false; break;
        default: break;
        }
        round = r.round;
    }

    Broadcast_PhaseChange(room_id, next, round);

    if (next == ROOM_PHASE::PLAYING)
        Broadcast_RoundStart(room_id, round, static_cast<unsigned int>(ROUND_DURATION * 1000));
}

// ── 이벤트 진입점 ────────────────────────────────────────────────────

void RoomPhaseManager::OnRoomRegistered(int room_id, int player_count)
{
    lock_guard<mutex> lk(m_lock);
    auto& r       = m_rooms[room_id];
    r.phase       = ROOM_PHASE::WAITING;
    r.round       = 1;
    r.score[0]    = r.score[1] = 0;
    r.timer_sec   = 0.f;
    r.expected    = player_count;
    r.joined      = 0;
    r.active      = true;
}

void RoomPhaseManager::OnPlayerJoined(int room_id)
{
    bool all_joined = false;
    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        if (!r.active || r.phase != ROOM_PHASE::WAITING) return;
        ++r.joined;
        all_joined = (r.joined >= r.expected);
    }
    if (all_joined)
        TransitionTo(room_id, ROOM_PHASE::CHARSELECT);
}

void RoomPhaseManager::OnPlayerLeft(int room_id)
{
    lock_guard<mutex> lk(m_lock);
    auto& r = m_rooms[room_id];
    if (r.active && r.joined > 0) --r.joined;
}

void RoomPhaseManager::OnRoundEnd(int room_id, int winner_team)
{
    unsigned char score_a, score_b;
    bool game_over = false;

    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        if (!r.active) return;

        if      (winner_team == 0) ++r.score[0];
        else if (winner_team == 1) ++r.score[1];

        score_a   = static_cast<unsigned char>(r.score[0]);
        score_b   = static_cast<unsigned char>(r.score[1]);
        game_over = (r.round >= MAX_ROUND_COUNT);
        if (!game_over) ++r.round;
    }

    unsigned char wt = (winner_team == 0 || winner_team == 1)
                     ? static_cast<unsigned char>(winner_team) : 2u;
    Broadcast_RoundEnd(room_id, wt, score_a, score_b);
    Broadcast_ScoreUpdate(room_id);

    TransitionTo(room_id, game_over ? ROOM_PHASE::GAMEOVER : ROOM_PHASE::SCOREBOARD);
}

// ── 브로드캐스트 헬퍼 ────────────────────────────────────────────────

namespace {
    void BroadcastToRoom(int room_id, void* pkt)
    {
        auto* gsm  = GameSessionManager::GetInstance();
        auto* room = gsm->GetRoom(room_id);
        if (!room || !room->IsActive()) return;

        for (int pid : room->GetPlayerIds()) {
            auto& session = gsm->GetClient(pid);
            lock_guard<mutex> lk(session.m_s_lock);
            if (session.m_state == ST_INGAME)
                session.Send(pkt);
        }
    }
}

void RoomPhaseManager::Broadcast_PhaseChange(int room_id, ROOM_PHASE phase, int round)
{
    SC_PHASE_CHANGE_PACKET pkt{};
    pkt.size  = sizeof(pkt);
    pkt.type  = SC_PHASE_CHANGE;
    pkt.phase = static_cast<unsigned char>(phase);
    pkt.round = static_cast<unsigned char>(round);
    BroadcastToRoom(room_id, &pkt);

    cout << "[Phase] Room " << room_id
         << " phase=" << (int)pkt.phase
         << " round=" << round << endl;
}

void RoomPhaseManager::Broadcast_RoundStart(int room_id, int round, unsigned int duration_ms)
{
    SC_ROUND_START_PACKET pkt{};
    pkt.size           = sizeof(pkt);
    pkt.type           = SC_ROUND_START;
    pkt.round          = static_cast<unsigned char>(round);
    pkt.duration_ms    = duration_ms;
    pkt.server_time_ms = GameSession::GetServerTimestamp();
    BroadcastToRoom(room_id, &pkt);
}

void RoomPhaseManager::Broadcast_RoundEnd(int room_id, unsigned char winner_team,
                                           unsigned char score_a, unsigned char score_b)
{
    SC_ROUND_END_PACKET pkt{};
    pkt.size        = sizeof(pkt);
    pkt.type        = SC_ROUND_END;
    pkt.winner_team = winner_team;
    pkt.score_a     = score_a;
    pkt.score_b     = score_b;
    BroadcastToRoom(room_id, &pkt);
}

void RoomPhaseManager::Broadcast_ScoreUpdate(int room_id)
{
    auto* gsm  = GameSessionManager::GetInstance();
    auto* room = gsm->GetRoom(room_id);
    if (!room || !room->IsActive()) return;

    const auto& ids = room->GetPlayerIds();

    unsigned char score_a, score_b;
    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        score_a = static_cast<unsigned char>(r.score[0]);
        score_b = static_cast<unsigned char>(r.score[1]);
    }

    SC_SCORE_UPDATE_PACKET pkt{};
    pkt.size         = sizeof(pkt);
    pkt.type         = SC_SCORE_UPDATE;
    pkt.score_a      = score_a;
    pkt.score_b      = score_b;
    pkt.player_count = static_cast<unsigned char>(ids.size());

    for (int i = 0; i < (int)ids.size() && i < ROOM_MAX_PLAYER; ++i) {
        auto& s              = gsm->GetClient(ids[i]);
        pkt.stats[i].player_id = s.m_lobby_player_id;
        pkt.stats[i].kills     = 0;   // Phase 3에서 실제 값으로 교체
        pkt.stats[i].deaths    = 0;
        pkt.stats[i].assists   = 0;
        pkt.stats[i].money     = 800;
    }

    BroadcastToRoom(room_id, &pkt);
}
