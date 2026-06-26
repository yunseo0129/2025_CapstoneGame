#include "RoomPhaseManager.h"
#include "GameSessionManager.h"

void RoomPhaseManager::StartTimerThread()
{
    thread(&RoomPhaseManager::TimerLoop, this).detach();
}

ROOM_PHASE RoomPhaseManager::GetRoomPhase(int room_id)
{
    if (room_id < 0 || room_id >= MAX_ROOM) return ROOM_PHASE::WAITING;
    lock_guard<mutex> lk(m_lock);
    return m_rooms[room_id].phase;
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

// 락 없이 pending 상태를 결정한 뒤, 락 해제 후 실제 전환 수행 (데드락 방지)
void RoomPhaseManager::TickRoom(int room_id, float dt)
{
    int  pending_winner = -2;   // -2 = 라운드 종료 없음
    bool do_sync        = false;

    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        if (!r.active || r.phase == ROOM_PHASE::WAITING || r.timer_sec <= 0.f)
            return;

        r.timer_sec    -= dt;
        r.sync_elapsed += dt;

        // 1초마다 주기 동기화
        if (r.sync_elapsed >= SYNC_INTERVAL) {
            r.sync_elapsed -= SYNC_INTERVAL;
            do_sync = true;
        }

        if (r.timer_sec <= 0.f) {
            r.timer_sec = 0.f;
            if (r.phase == ROOM_PHASE::PLAYING) {
                pending_winner = 0;  // 타임아웃 → 팀A 임시 승
            } else {
                // 선택 페이즈 타이머 만료: SC_TIMER_SYNC(0) 전송 → 클라가 CS_PHASE_READY 전송
                // 다음 TickRoom 호출 시 timer_sec=0 이므로 early-return, 중복 전송 없음
                do_sync = true;
            }
        }
    }

    // PLAYING 타임아웃이 아닌 경우 주기(또는 만료) 동기화 전송
    if (do_sync && pending_winner == -2)
        Broadcast_TimerSync(room_id);

    if (pending_winner >= -1)
        OnRoundEnd(room_id, pending_winner);
}

void RoomPhaseManager::TransitionTo(int room_id, ROOM_PHASE next)
{
    int round = 1;
    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        if (!r.active) return;
        if (r.phase == next) return;  // 중복 전환 방지 (TickRoom + OnPlayerPhaseReady 동시 호출 시)

        r.phase             = next;
        r.sync_elapsed      = 0.f;
        r.phase_ready_count = 0;
        switch (next) {
        case ROOM_PHASE::CHARSELECT: r.timer_sec = CHARSELECT_DURATION; break;
        case ROOM_PHASE::SCOREBOARD: r.timer_sec = SCOREBOARD_DURATION; r.map_loaded_count = 0; break;
        case ROOM_PHASE::SHOP:       r.timer_sec = SHOP_DURATION;       break;
        case ROOM_PHASE::PLAYING:    r.timer_sec = ROUND_DURATION;      break;
        case ROOM_PHASE::GAMEOVER:   r.timer_sec = 0.f; r.active = false; break;
        default: break;
        }
        round = r.round;
    }

    if (next == ROOM_PHASE::CHARSELECT)
        Broadcast_RosterInfo(room_id);

    Broadcast_PhaseChange(room_id, next, round);

    // 페이즈 전환 직후 초기 타이머 값 즉시 전송 (클라가 SC_PHASE_CHANGE 처리 후 바로 표시 가능)
    if (next != ROOM_PHASE::GAMEOVER)
        Broadcast_TimerSync(room_id);

    // 선택/스코어보드 페이즈: 팀 시작 지점(y=25)으로 이동 + 전원 즉시 동기화
    if (next == ROOM_PHASE::CHARSELECT || next == ROOM_PHASE::SCOREBOARD)
        ApplyTeamSpawnPositions(room_id);

    if (next == ROOM_PHASE::PLAYING)
    {
        // 스폰 위치를 서버 권위 위치에 적용 후 전원에게 즉시 동기화
        ApplySpawnPositions(room_id);
        Broadcast_RoundStart(room_id, round, static_cast<unsigned int>(ROUND_DURATION * 1000));
    }
}

// ── 이벤트 진입점 ────────────────────────────────────────────────────

void RoomPhaseManager::OnRoomRegistered(const IS_ROOM_NOTIFY_PACKET& pkt)
{
    lock_guard<mutex> lk(m_lock);
    auto& r             = m_rooms[pkt.room_id];
    r.phase             = ROOM_PHASE::WAITING;
    r.round             = 1;
    r.score[0]          = r.score[1] = 0;
    r.timer_sec         = 0.f;
    r.sync_elapsed      = 0.f;
    r.phase_ready_count = 0;
    r.map_loaded_count  = 0;
    r.expected          = pkt.player_count;
    r.joined            = 0;
    r.active            = true;
    r.roster_count      = pkt.player_count;
    for (int i = 0; i < pkt.player_count && i < ROOM_MAX_PLAYER; ++i) {
        r.roster[i].player_id = pkt.player_ids[i];
        strcpy_s(r.roster[i].name, sizeof(r.roster[i].name), pkt.player_names[i]);
        r.roster[i].team = pkt.player_teams[i];
        r.roster[i].slot = pkt.player_slots[i];
    }
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

void RoomPhaseManager::Broadcast_RosterInfo(int room_id)
{
    SC_ROSTER_INFO_PACKET pkt{};
    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        pkt.size         = sizeof(pkt);
        pkt.type         = SC_ROSTER_INFO;
        pkt.player_count = static_cast<unsigned char>(r.roster_count);
        for (int i = 0; i < r.roster_count && i < ROOM_MAX_PLAYER; ++i) {
            pkt.players[i].player_id = r.roster[i].player_id;
            strcpy_s(pkt.players[i].name, sizeof(pkt.players[i].name), r.roster[i].name);
            pkt.players[i].team = r.roster[i].team;
            pkt.players[i].slot = r.roster[i].slot;
        }
    }
    BroadcastToRoom(room_id, &pkt);
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

void RoomPhaseManager::ApplySpawnPositions(int room_id)
{
    auto* gsm  = GameSessionManager::GetInstance();
    auto* room = gsm->GetRoom(room_id);
    if (!room || !room->IsActive()) return;

    const vector<int>& ids = room->GetPlayerIds();

    // 1) 각 플레이어 위치를 선택 스폰 좌표로 세팅
    for (int pid : ids)
    {
        auto& s = gsm->GetClient(pid);
        lock_guard<mutex> lk(s.m_s_lock);
        if (s.m_state != ST_INGAME) continue;

        const float* sp = s.m_spawnPos;
        // m_spawnPos가 미선택 상태(0,0,0)이면 GROUND_HEIGHT만 보정
        const bool bSelected = (sp[0] != 0.f || sp[1] != 0.f || sp[2] != 0.f);
        if (bSelected)
            s.m_worldMatrix.SetPosition(sp[0], sp[1], sp[2]);
        else
            s.m_worldMatrix.SetPosition(s.m_worldMatrix.m[12],
                                        WorldMatrixInfo::GROUND_HEIGHT,
                                        s.m_worldMatrix.m[14]);

        // 낙하 모멘텀 제거 (스폰 직후 중력으로 즉시 아래로 밀리는 것 방지)
        s.m_player.fVerticalVelocity = 0.f;
        s.m_player.bIsGrounded       = true;
        // 라운드 시작: HP/생사 리셋
        s.m_hp     = GameSession::MAX_HP;
        s.m_bAlive = true;
    }

    // 즉시 브로드캐스트를 하지 않는다.
    // 클라이언트는 PLAYING 진입 시 Apply_SpawnLaunch로 싱크대→식탁 포물선 연출을 자체 처리하며,
    // 발사 중 보내는 CS_MOVE 위치를 서버가 채택(GameSessionManager.cpp memcpy)하므로
    // 권위 위치도 포물선을 따라가다 착지(식탁, GROUND_HEIGHT)에서 수렴한다.
    // 즉시 전송하면 클라가 PLAYING 진입 전에 식탁 위치를 받아 텔레포트처럼 보이는 부작용이 발생한다.

    cout << "[Phase] Room " << room_id << " spawn positions applied (broadcast deferred to CS_MOVE)." << endl;
}

void RoomPhaseManager::ApplyTeamSpawnPositions(int room_id)
{
    auto* gsm  = GameSessionManager::GetInstance();
    auto* room = gsm->GetRoom(room_id);
    if (!room || !room->IsActive()) return;

    const vector<int>& ids = room->GetPlayerIds();

    // roster 스냅샷을 먼저 읽는다 (세션 락과 m_lock 중첩 금지 → 데드락 방지)
    PlayerRosterEntry roster_snap[ROOM_MAX_PLAYER] = {};
    int roster_count = 0;
    {
        lock_guard<mutex> lk(m_lock);
        auto& r    = m_rooms[room_id];
        roster_count = r.roster_count;
        for (int i = 0; i < roster_count; ++i)
            roster_snap[i] = r.roster[i];
    }

    // 각 플레이어를 팀 시작 지점으로 세팅
    // 공식(클라 Defines.h::Get_SpawnSpot 과 동일):
    //   x = 5*number-5,  y = 25,  z = (team==0 RED)?50:-60
    for (int pid : ids)
    {
        auto& s = gsm->GetClient(pid);
        lock_guard<mutex> lk(s.m_s_lock);
        if (s.m_state != ST_INGAME) continue;

        int team   = 0;
        int number = 1;
        for (int i = 0; i < roster_count; ++i)
        {
            if (roster_snap[i].player_id == s.m_lobby_player_id)
            {
                team   = roster_snap[i].team;
                number = roster_snap[i].slot;
                if (number < 1) number = 1;
                break;
            }
        }

        float x = 5.f * number - 5.f;
        float y = 25.f;
        float z = (team == 0) ? 50.f : -60.f;
        s.m_worldMatrix.SetPosition(x, y, z);
        s.m_player.fVerticalVelocity = 0.f;
        s.m_player.bIsGrounded       = true;
    }

    // 전원에게 즉시 동기화 (선택 페이즈는 발사 연출이 없으므로 즉시 브로드캐스트가 올바름)
    for (int sender_pid : ids)
    {
        if (gsm->GetClient(sender_pid).m_state != ST_INGAME) continue;
        for (int receiver_pid : ids)
        {
            if (gsm->GetClient(receiver_pid).m_state != ST_INGAME) continue;
            gsm->GetClient(receiver_pid).Send_Move_Packet(sender_pid, gsm);
        }
    }

    cout << "[Phase] Room " << room_id << " team spawn positions applied (y=25)." << endl;
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

// ── 타이머 동기화 ──────────────────────────────────────────────────────

void RoomPhaseManager::Broadcast_TimerSync(int room_id)
{
    unsigned int time_ms = 0;
    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        if (!r.active || r.phase == ROOM_PHASE::WAITING) return;
        time_ms = static_cast<unsigned int>(r.timer_sec * 1000.f);
    }
    SC_TIMER_SYNC_PACKET pkt{};
    pkt.size    = sizeof(pkt);
    pkt.type    = SC_TIMER_SYNC;
    pkt.time_ms = time_ms;
    BroadcastToRoom(room_id, &pkt);
}

// ── CS_MAP_LOADED 수신: 전원 완료 시 SCOREBOARD → SHOP 전환 ──────────

void RoomPhaseManager::OnMapLoaded(int room_id, unsigned char slot)
{
    bool all_loaded = false;
    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        if (!r.active || r.phase != ROOM_PHASE::SCOREBOARD) return;
        ++r.map_loaded_count;
        all_loaded = (r.map_loaded_count >= r.joined);
    }
    Broadcast_MapLoaded(room_id, slot);
    if (all_loaded)
        TransitionTo(room_id, ROOM_PHASE::SHOP);
}

void RoomPhaseManager::Broadcast_MapLoaded(int room_id, unsigned char slot)
{
    SC_MAP_LOADED_PACKET pkt{};
    pkt.size = sizeof(pkt);
    pkt.type = SC_MAP_LOADED;
    pkt.slot = slot;
    BroadcastToRoom(room_id, &pkt);
}

// ── CS_PHASE_READY 수신: 전원 준비 완료 시 다음 페이즈로 ��환 ───────��

void RoomPhaseManager::OnPlayerPhaseReady(int room_id, unsigned char phase_byte)
{
    ROOM_PHASE pending = ROOM_PHASE::WAITING;
    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        if (!r.active) return;
        if (static_cast<unsigned char>(r.phase) != phase_byte) return;
        if (r.phase == ROOM_PHASE::PLAYING || r.phase == ROOM_PHASE::GAMEOVER) return;

        ++r.phase_ready_count;
        if (r.phase_ready_count < r.joined) return;

        switch (r.phase) {
        case ROOM_PHASE::CHARSELECT: pending = ROOM_PHASE::SCOREBOARD; break;
        case ROOM_PHASE::SCOREBOARD: pending = ROOM_PHASE::SHOP;       break;
        case ROOM_PHASE::SHOP:       pending = ROOM_PHASE::PLAYING;    break;
        default: break;
        }
    }
    if (pending != ROOM_PHASE::WAITING)
        TransitionTo(room_id, pending);
}
