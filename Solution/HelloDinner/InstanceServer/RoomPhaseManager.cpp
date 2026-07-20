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
    int  pending_winner  = -2;   // -2 = 라운드 종료 없음
    bool do_sync         = false;
    bool pending_restart = false; // 결과 대기 만료 → RestartRound 호출 필요

    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        if (!r.active || r.phase == ROOM_PHASE::WAITING)
            return;

        if (r.in_result) {
            // ── 결과 대기 중(전멸 후 5초): 정규 타이머 정지 ──────────
            r.result_timer -= dt;
            if (r.result_timer <= 0.f) {
                r.in_result    = false;
                r.result_timer = 0.f;
                pending_restart = true;
            }
            // pending_restart 체크는 락 해제 후 아래에서 수행
        } else {
            // ── 정규 타이머 처리 ────────────────────────────────────────
            if (r.timer_sec <= 0.f)
                return;  // 이미 만료 → 중복 처리 방지

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
                    do_sync = true;
                }
            }
        }
    }

    // 결과 대기 만료: SHOP(스폰 재선택) 페이즈로 전환 → 선택 후 포물선 발사로 다음 라운드 시작
    if (pending_restart) {
        TransitionTo(room_id, ROOM_PHASE::SHOP);
        return;
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

    // 선택/스코어보드/스폰재선택 페이즈: 팀 테이블 위 시작 지점으로 이동 + 전원 즉시 동기화
    // SHOP 은 최초(SCOREBOARD 후)뿐 아니라 라운드 종료 후 재진입 시에도 동일하게 적용한다.
    if (next == ROOM_PHASE::CHARSELECT || next == ROOM_PHASE::SCOREBOARD ||
        next == ROOM_PHASE::SHOP)
        ApplyTeamSpawnPositions(room_id);

    if (next == ROOM_PHASE::PLAYING)
    {
        // 스폰 위치를 서버 권위 위치에 적용 후 전원에게 즉시 동기화
        ApplySpawnPositions(room_id);
        Broadcast_RoundStart(room_id, round, static_cast<unsigned int>(ROUND_DURATION * 1000));
    }
}

// ── 이벤트 진입점 ────────────────────────────────────────────────────

void RoomPhaseManager::OnPlayerDied(int room_id)
{
    // 1) PLAYING 페이즈 확인 + roster 스냅샷 (m_lock → 세션락 중첩 금지, 스냅샷 패턴 사용)
    PlayerRosterEntry roster_snap[ROOM_MAX_PLAYER] = {};
    int roster_count = 0;
    {
        lock_guard<mutex> lk(m_lock);
        auto& r = m_rooms[room_id];
        // 결과 대기 중이면 중복 라운드 종료 방지
        if (!r.active || r.phase != ROOM_PHASE::PLAYING || r.in_result) return;
        roster_count = r.roster_count;
        for (int i = 0; i < roster_count; ++i)
            roster_snap[i] = r.roster[i];
    }

    // 2) 세션별 생사 집계 (팀별 total/alive)
    auto* gsm  = GameSessionManager::GetInstance();
    auto* room = gsm->GetRoom(room_id);
    if (!room || !room->IsActive()) return;

    int total[2] = {};  // total[t] = 팀 t 소속 인원 수
    int alive[2] = {};  // alive[t] = 팀 t 생존 인원 수

    for (int pid : room->GetPlayerIds())
    {
        auto& s = gsm->GetClient(pid);
        lock_guard<mutex> lk(s.m_s_lock);
        if (s.m_state != ST_INGAME) continue;

        // roster 스냅샷에서 이 플레이어의 팀 조회
        int team = -1;
        for (int i = 0; i < roster_count; ++i) {
            if (roster_snap[i].player_id == s.m_lobby_player_id) {
                team = roster_snap[i].team;
                break;
            }
        }
        if (team != 0 && team != 1) continue;

        ++total[team];
        if (s.m_bAlive) ++alive[team];
    }

    // 3) 전멸 여부 판정
    bool wipeA = (total[0] > 0 && alive[0] == 0);  // 팀A 전멸 → 팀B 승
    bool wipeB = (total[1] > 0 && alive[1] == 0);  // 팀B 전멸 → 팀A 승

    if (!wipeA && !wipeB) return;  // 아직 전멸 없음

    // 두 팀 동시 전멸: 무승부(-1), 단독 전멸: 생존 팀 승
    int winner = (wipeA && wipeB) ? -1 : (wipeA ? 1 : 0);

    cout << "[Round] Room " << room_id
         << " team-wipe detected. winner=" << winner
         << " (alive A=" << alive[0] << "/" << total[0]
         << ", B=" << alive[1] << "/" << total[1] << ")" << endl;

    OnRoundEnd(room_id, winner);
}

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
    r.in_result         = false;
    r.result_timer      = 0.f;
    r.expected          = pkt.player_count;
    r.joined            = 0;
    r.active            = true;
    r.roster_count      = pkt.player_count;

    // 팀 카운터 (폴백 배정용)
    int team_count[2] = {};  // 팀 0/1 에 이미 배정된 인원 수

    for (int i = 0; i < pkt.player_count && i < ROOM_MAX_PLAYER; ++i) {
        r.roster[i].player_id = pkt.player_ids[i];
        strcpy_s(r.roster[i].name, sizeof(r.roster[i].name), pkt.player_names[i]);

        unsigned char team = pkt.player_teams[i];
        unsigned char slot = pkt.player_slots[i];

        // 팀이 0xFF(미선택)이면 인덱스 기반 폴백 배정 (1v1→0/1, 2v2→0/1/0/1 균형)
        if (team != 0 && team != 1) {
            team = static_cast<unsigned char>(i % 2);
            cout << "[Room] player_id=" << pkt.player_ids[i]
                 << " team was 0xFF, fallback team=" << (int)team << endl;
        }

        // slot이 0(미선택)이면 해당 팀 내 순번으로 보정 (1부터 시작)
        if (slot == 0) {
            slot = static_cast<unsigned char>(team_count[team] + 1);
            if (slot > 3) slot = 3;  // 최대 3
        }

        r.roster[i].team = team;
        r.roster[i].slot = slot;

        if (team == 0 || team == 1)
            ++team_count[team];
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
        if (!game_over) {
            ++r.round;
            // 전멸로 인한 라운드 종료: 결과 대기 5초 진입 (페이즈는 PLAYING 유지)
            r.in_result    = true;
            r.result_timer = RESULT_DURATION;
        }
    }

    unsigned char wt = (winner_team == 0 || winner_team == 1)
                     ? static_cast<unsigned char>(winner_team) : 2u;
    Broadcast_RoundEnd(room_id, wt, score_a, score_b);
    Broadcast_ScoreUpdate(room_id);

    if (game_over)
        TransitionTo(room_id, ROOM_PHASE::GAMEOVER);
    // else: 결과 대기 상태(in_result=true), TickRoom이 5초 후 SHOP(스폰 재선택) 페이즈로 전환
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

    vector<int> ids = room->GetPlayerIds();

    // roster 스냅샷: m_lock → 세션락 중첩 금지, ApplyTeamSpawnPositions 패턴 동일.
    // 폴백(미선택) 시 팀/슬롯 기반 바닥 기본 스폰을 계산하기 위해 필요.
    PlayerRosterEntry roster_snap[ROOM_MAX_PLAYER] = {};
    int roster_count = 0;
    {
        lock_guard<mutex> lk(m_lock);
        auto& r      = m_rooms[room_id];
        roster_count = r.roster_count;
        for (int i = 0; i < roster_count; ++i)
            roster_snap[i] = r.roster[i];
    }

    // 각 플레이어 위치를 선택 스폰 좌표로 세팅
    for (int pid : ids)
    {
        auto& s = gsm->GetClient(pid);
        lock_guard<mutex> lk(s.m_s_lock);
        if (s.m_state != ST_INGAME) continue;

        const float* sp = s.m_spawnPos;
        // m_spawnPos 가 설정돼 있으면 그 좌표 사용
        const bool bSelected = (sp[0] != 0.f || sp[1] != 0.f || sp[2] != 0.f);
        if (bSelected)
        {
            s.m_worldMatrix.SetPosition(sp[0], sp[1], sp[2]);
        }
        else
        {
            // CS_SPAWN_SELECT / CS_PHASE_READY IOCP 처리 순서 레이스 대비 폴백:
            // 스테이징 테이블(Bar/Shelf_floor_4) XZ 를 그대로 사용하면 클라 포물선 착지 후
            // Apply_ServerCorrection 이 스테이징 테이블로 당겨가는 버그가 발생한다.
            // 클라 MATCH_SETUP::Get_SpawnSpot() 와 동일한 바닥 기본 좌표를 사용:
            //   x = 5*slot - 5,  y = GROUND_HEIGHT,  z = (team==0 ? 50 : -60)
            int team   = 0;
            int number = 1;
            for (int i = 0; i < roster_count; ++i)
            {
                if (roster_snap[i].player_id == s.m_lobby_player_id)
                {
                    team   = roster_snap[i].team;
                    number = roster_snap[i].slot;
                    if (number < 1) number = 1;
                    if (number > 3) number = 3;
                    break;
                }
            }
            if (team < 0 || team > 1) team = 0;

            float fx = 5.f * number - 5.f;
            float fy = WorldMatrixInfo::GROUND_HEIGHT;
            float fz = (team == 0) ? 50.f : -60.f;
            s.m_worldMatrix.SetPosition(fx, fy, fz);
        }

        // 낙하 모멘텀 제거
        s.m_player.fVerticalVelocity = 0.f;
        s.m_player.bIsGrounded       = true;
        // 라운드 시작: HP/생사 리셋
        s.m_hp     = GameSession::MAX_HP;
        s.m_bAlive = true;
    }

    // 즉시 브로드캐스트를 하지 않는다.
    // 클라이언트는 PLAYING 진입 시 Apply_SpawnLaunch 로 팀 테이블→목표 포물선 연출을 자체 처리하며,
    // CS_MOVE 는 회전만 서버에 복사(GameSessionManager.cpp memcpy 12float)한다.
    // 착지 후 Apply_ServerCorrection 이 서버 권위 위치로 수렴하므로 브로드캐스트가 불필요하다.
    // (CS_SPAWN_SELECT 가 PLAYING 전환 후 늦게 도착한 경우: GameSessionManager.cpp CS_SPAWN_SELECT
    //  핸들러에서 즉시 권위 위치를 보정 + 브로드캐스트하는 레이스 패치가 처리한다.)

    cout << "[Phase] Room " << room_id << " spawn positions applied (broadcast deferred)." << endl;
}

void RoomPhaseManager::ApplyTeamSpawnPositions(int room_id)
{
    auto* gsm  = GameSessionManager::GetInstance();
    auto* room = gsm->GetRoom(room_id);
    if (!room || !room->IsActive()) return;

    vector<int> ids = room->GetPlayerIds();

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

    // 각 플레이어를 원형테이블 양 옆 팀 테이블 위 팀/슬롯 정렬 지점으로 세팅.
    // 클라 Defines.h::MATCH_SETUP::Get_TableSpot() 와 수치 완전 일치 유지 필요:
    //   SLOT_SPACING_X=8.0
    //   RED(0)=Bar 앞쪽:          BASE_X=4.50   TOP_Y=23.15  Z=-65.14
    //   BLUE(1)=Shelf_floor_4 뒤: BASE_X=8.10   TOP_Y=22.52  Z= 53.87
    //   x = BASE_X[team] + (number-2)*8   (slot1→-3.5/0.1, slot2→4.5/8.1, slot3→12.5/16.1)
    static constexpr float SLOT_SPACING_X    =   8.0f;
    static constexpr float TABLE_BASE_X[2]   = {  4.50f,   8.10f };
    static constexpr float TABLE_TOP_Y[2]    = { 23.15f,  22.52f };
    static constexpr float TABLE_BASE_Z[2]   = { -65.14f, 53.87f };

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
                if (number > 3) number = 3;
                break;
            }
        }
        if (team < 0 || team > 1) team = 0;

        float x = TABLE_BASE_X[team] + (number - 2) * SLOT_SPACING_X;
        float y = TABLE_TOP_Y[team];
        float z = TABLE_BASE_Z[team];
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

    cout << "[Phase] Room " << room_id << " table staging applied (RED Bar y=23.15 / BLUE SF4 y=22.52)." << endl;
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

// ── CS_PHASE_READY 수신: 전원 준비 완료 시 다음 페이즈로 전환 ──────────

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
