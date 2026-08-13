#include "GameSessionManager.h"
#include "RoomPhaseManager.h"
#include <cmath>

// =============================================================================
// 클라이언트 연결 관리
// =============================================================================

int GameSessionManager::GetNewClientId()
{
    for (int i = 0; i < MAX_USER; ++i) {
        lock_guard<mutex> ll{ m_clients[i].m_s_lock };
        if (m_clients[i].m_state == ST_FREE) {
            cout << "[Instance] New Client[" << i << "] Connected." << endl;
            return i;
        }
    }
    return -1;
}

// =============================================================================
// 맵 충돌 로드 + BVH 빌드
// =============================================================================

bool GameSessionManager::Build_MapCollision(const std::string& jsonPath)
{
    m_mapColliders.clear();
    m_mapPtrs.clear();
    m_mapBVH.Clear();

    if (!LoadMapColliders(jsonPath, m_mapColliders)) {
        cout << "[Collision] Failed to load map colliders. Collision disabled." << endl;
        return false;
    }

    m_mapPtrs.reserve(m_mapColliders.size());
    for (auto& col : m_mapColliders)
        m_mapPtrs.push_back(&col);

    m_mapBVH.Build(m_mapPtrs);
    return true;
}

bool GameSessionManager::CheckMove_Player(SServerCollider* me, const XMFLOAT3& move, XMFLOAT3& outSlide)
{
    return ServerCheckMove(me, move, outSlide, nullptr, m_mapBVH, m_mapPtrs);
}

// =============================================================================
// 패킷 처리
// =============================================================================

void GameSessionManager::ProcessPacket(int c_id, char* packet)
{
    const int sz = static_cast<unsigned char>(packet[0]);
    switch (packet[1]) {

    case CS_JOIN_ROOM: {
        if (sz < (int)sizeof(CS_JOIN_ROOM_PACKET)) break;
        CS_JOIN_ROOM_PACKET* p = reinterpret_cast<CS_JOIN_ROOM_PACKET*>(packet);

        // 인증 토큰 확인
        if (!AuthenticateJoin(p->room_id, p->auth_token)) {
            cout << "[Instance] Auth failed for client " << c_id
                 << " room " << p->room_id << endl;
            Disconnect(c_id);
            break;
        }

        auto& session = m_clients[c_id];
        strcpy_s(session.m_player.name, p->name);
        session.m_lobby_player_id = p->player_id;
        session.m_room_id = p->room_id;
        {
            lock_guard<mutex> ll(session.m_s_lock);
            session.m_state = ST_INGAME;
        }

        // 방에 플레이어 추가
        auto* room = GetRoom(p->room_id);
        if (room && room->IsActive()) {
            // 기존 룸 플레이어에게 새 플레이어 알림
            for (int pid : room->GetPlayerIds()) {
                if (m_clients[pid].m_state != ST_INGAME) continue;
                m_clients[pid].Send_Add_Player_Packet(c_id, this);
                // 새 플레이어에게 기존 플레이어 알림
                session.Send_Add_Player_Packet(pid, this);
            }
            // 새 플레이어 목록에 추가
            vector<int> ids = room->GetPlayerIds();
            ids.push_back(c_id);
            room->Initialize(p->room_id, ids);
        }

        // 본인에게 초기 위치 전송 (클라이언트가 첫 snap에 사용)
        session.Send_Move_Packet(c_id, this);

        // 페이즈 매니저에 입장 완료 알림 → 전원 입장 시 CHARSELECT로 전환
        RoomPhaseManager::GetInstance()->OnPlayerJoined(p->room_id);

        cout << "[Instance] Client [" << c_id << "] joined room " << p->room_id
             << " as " << p->name << endl;
        break;
    }

    case CS_MOVE: {
        if (sz < (int)sizeof(CS_MOVE_PACKET)) break;
        CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
        auto& session = m_clients[c_id];

        // 클라이언트 타임스탬프 차이를 fTimeDelta로 사용
        float fTimeDelta = 0.f;
        if (session.m_lastClientTimestamp != 0) {
            unsigned int delta = p->timestamp - session.m_lastClientTimestamp;
            fTimeDelta = delta / 1000.f;
            if (fTimeDelta > 0.1f) fTimeDelta = 0.1f;
        }

        // 라이징 에지: 이번 패킷에서 새로 눌린 키
        unsigned short risingEdge = (~session.m_player.keyInput) & p->keyInput;

        session.UpdateTimestamp(p->timestamp);

        {
        lock_guard<mutex> lk(session.m_s_lock);
        session.m_player.prevKeyInput = session.m_player.keyInput;
        session.m_player.keyInput     = p->keyInput;

        // 클라이언트 회전(Right/Up/Look, m[0..11])은 항상 복사 — 위치는 페이즈별 처리
        memcpy(session.m_worldMatrix.m, p->worldMatrix, sizeof(float) * 12);

        ROOM_PHASE curPhase = RoomPhaseManager::GetInstance()->GetRoomPhase(session.m_room_id);

        // CHARSELECT 페이즈: 자유 이동 허용 — 위치도 클라 권위로 복사
        if (curPhase == ROOM_PHASE::CHARSELECT) {
            session.m_worldMatrix.m[12] = p->worldMatrix[12];
            session.m_worldMatrix.m[13] = p->worldMatrix[13];
            session.m_worldMatrix.m[14] = p->worldMatrix[14];
        }

        // PLAYING 페이즈에서만 물리+충돌 적용
        // 그 외(SCOREBOARD/SHOP) 페이즈는 동결 (ApplyTeamSpawnPositions 위치 유지)
        if (curPhase == ROOM_PHASE::PLAYING) {

            // ─────────────────────────────────────────────────────────────────
            // 서버 권위 물리 + 맵 이동 충돌
            // 클라이언트 CPlayer_1rd::Resolve_Movement + CheckMove 와 동일 흐름
            // ─────────────────────────────────────────────────────────────────
            auto& wm = session.m_worldMatrix;
            auto& pi = session.m_player;
            const unsigned short ki = p->keyInput;

            const bool bSpace = (ki & KEY_SPACE) != 0;
            const bool bCtrl  = (ki & KEY_CTRL)  != 0;
            const bool bShift = (ki & KEY_SHIFT) != 0;
            const bool bW = (ki & KEY_W) != 0, bS = (ki & KEY_S) != 0;
            const bool bA = (ki & KEY_A) != 0, bD = (ki & KEY_D) != 0;

            // 1. 점프 (ApplyPlayerPhysics 와 동일)
            if (bSpace && pi.bIsGrounded) {
                pi.fVerticalVelocity = WorldMatrixInfo::PHYS_JUMP_SPEED;
                pi.bIsGrounded       = false;
            }

            // 2. 중력
            pi.fVerticalVelocity += WorldMatrixInfo::PHYS_GRAVITY * fTimeDelta;
            if (pi.fVerticalVelocity < -WorldMatrixInfo::PHYS_TERMINAL_VEL)
                pi.fVerticalVelocity = -WorldMatrixInfo::PHYS_TERMINAL_VEL;
            const float fVerticalDelta = pi.fVerticalVelocity * fTimeDelta;

            // 3. 속도 배율 (걷기4 / 웅크리기2 / 달리기6)
            const float speed = pi.speedPerSec * (bCtrl ? 2.f : (bShift ? 6.f : 4.f));

            // 4. 수평 XZ 이동 방향 (ApplyPlayerPhysics 동일)
            float lx = wm.m[8], lz = wm.m[10];
            const float lenL = sqrtf(lx * lx + lz * lz);
            if (lenL > 1e-4f) { lx /= lenL; lz /= lenL; }
            float rx = wm.m[0], rz = wm.m[2];
            const float lenR = sqrtf(rx * rx + rz * rz);
            if (lenR > 1e-4f) { rx /= lenR; rz /= lenR; }

            const float fLook  = (bW ? 1.f : 0.f) - (bS ? 1.f : 0.f);
            const float fRight = (bD ? 1.f : 0.f) - (bA ? 1.f : 0.f);
            float dirX = lx * fLook + rx * fRight;
            float dirZ = lz * fLook + rz * fRight;
            const float dirLen = sqrtf(dirX * dirX + dirZ * dirZ);
            float hX = 0.f, hZ = 0.f;
            if (dirLen > 1e-4f) {
                hX = (dirX / dirLen) * speed * fTimeDelta;
                hZ = (dirZ / dirLen) * speed * fTimeDelta;
            }

            // 5. 수평+수직 합산 이동 벡터
            XMFLOAT3 vMove = { hX, fVerticalDelta, hZ };

            // 6. 플레이어 sphere 콜라이더 월드 위치 갱신 (현재 위치 기준)
            session.UpdateColliderPositions();

            // 7. 각 sphere 에 대해 CheckMove 슬라이드 누적
            //    (Resolve_Movement:289-299 와 동일)
            XMFLOAT3 vSlide = vMove;
            if (!m_mapPtrs.empty()) {  // 맵 콜라이더 로드된 경우에만
                for (int si = 0; si < 2; ++si) {
                    XMFLOAT3 slideOut = vSlide;
                    CheckMove_Player(&session.m_mapSpheres[si], vSlide, slideOut);
                    vSlide = slideOut;
                }
            }

            // 8. 착지/천장 판정 (Resolve_Movement:309-323 과 동일)
            pi.bIsGrounded = false;
            if (fVerticalDelta < -1e-5f && vSlide.y > fVerticalDelta + 1e-4f) {
                // 내려가려 했는데 막힘 → 착지
                pi.bIsGrounded       = true;
                pi.fVerticalVelocity = 0.f;
            }
            else if (fVerticalDelta > 1e-5f && vSlide.y < fVerticalDelta - 1e-4f) {
                // 올라가려 했는데 막힘 → 천장
                pi.fVerticalVelocity = 0.f;
            }

            // 9. 위치 반영
            wm.m[12] += vSlide.x;
            wm.m[13] += vSlide.y;
            wm.m[14] += vSlide.z;

            // 10. 안전 바닥 클램프 (맵 지오메트리 누락 대비 fallback)
            if (wm.m[13] < WorldMatrixInfo::GROUND_HEIGHT) {
                wm.m[13] = WorldMatrixInfo::GROUND_HEIGHT;
                if (pi.fVerticalVelocity < 0.f) {
                    pi.fVerticalVelocity = 0.f;
                    pi.bIsGrounded       = true;
                }
            }
        }
        } // lock_guard<mutex> lk(session.m_s_lock)

        // 액션 키 라이징 에지 처리
        if (risingEdge & KEY_R)
            cout << "[Instance] Client [" << c_id << "] interact (R)\n";
        if (risingEdge & KEY_MOUSE_LB)
            cout << "[Instance] Client [" << c_id << "] attack (MOUSE_LB)\n";

        int room_id = session.m_room_id;
        if (room_id == -1) break;

        auto* room = GetRoom(room_id);
        if (!room || !room->IsActive()) break;

        for (int pid : room->GetPlayerIds()) {
            if (m_clients[pid].m_state != ST_INGAME) continue;
            m_clients[pid].Send_Move_Packet(c_id, this);
        }
        break;
    }

    case CS_CHAR_SELECT: {
        if (sz < (int)sizeof(CS_CHAR_SELECT_PACKET)) break;
        CS_CHAR_SELECT_PACKET* p = reinterpret_cast<CS_CHAR_SELECT_PACKET*>(packet);
        m_clients[c_id].m_iCharType = p->char_type;

        int room_id = m_clients[c_id].m_room_id;
        if (room_id != -1) {
            auto* room = GetRoom(room_id);
            if (room && room->IsActive()) {
                SC_CHAR_SELECT_PACKET pkt{};
                pkt.size      = sizeof(SC_CHAR_SELECT_PACKET);
                pkt.type      = SC_CHAR_SELECT;
                pkt.player_id = m_clients[c_id].m_lobby_player_id;
                pkt.char_type = p->char_type;
                for (int pid : room->GetPlayerIds()) {
                    if (m_clients[pid].m_state != ST_INGAME) continue;
                    m_clients[pid].Send(&pkt);
                }
            }
        }
        cout << "[Instance] Client [" << c_id << "] selected char " << (int)p->char_type << endl;
        break;
    }

    case CS_SPAWN_SELECT: {
        if (sz < (int)sizeof(CS_SPAWN_SELECT_PACKET)) break;
        CS_SPAWN_SELECT_PACKET* p = reinterpret_cast<CS_SPAWN_SELECT_PACKET*>(packet);
        memcpy(m_clients[c_id].m_spawnPos, p->world_pos, sizeof(float) * 3);

        int room_id = m_clients[c_id].m_room_id;
        if (room_id != -1) {
            auto* room = GetRoom(room_id);
            if (room && room->IsActive()) {
                // SC_SPAWN_SELECT 에코: 방 전원에게 선택 좌표 브로드캐스트
                SC_SPAWN_SELECT_PACKET pkt{};
                pkt.size      = sizeof(SC_SPAWN_SELECT_PACKET);
                pkt.type      = SC_SPAWN_SELECT;
                pkt.player_id = m_clients[c_id].m_lobby_player_id;
                memcpy(pkt.world_pos, p->world_pos, sizeof(float) * 3);
                for (int pid : room->GetPlayerIds()) {
                    if (m_clients[pid].m_state != ST_INGAME) continue;
                    m_clients[pid].Send(&pkt);
                }

                // ── IOCP 레이스 패치 ─────────────────────────────────────────
                // CS_SPAWN_SELECT / CS_PHASE_READY 는 같은 TCP 스트림에서 순서대로 전송되지만,
                // 다중 IOCP 워커 스레드가 각각 다른 recv 완료를 처리할 때 순서가 뒤바뀔 수 있다.
                // ready 가 먼저 처리돼 PLAYING 으로 전환된 뒤 select 가 뒤늦게 도착하면
                // ApplySpawnPositions 는 이미 m_spawnPos==(0,0,0) 폴백으로 위치를 세팅한 상태.
                // 이 경우 선택 좌표를 즉시 권위 위치에 반영하고 전원에게 브로드캐스트해
                // 클라 착지 수렴(Apply_ServerCorrection)이 올바른 지점을 향하도록 정정한다.
                ROOM_PHASE cur_phase = RoomPhaseManager::GetInstance()->GetRoomPhase(room_id);
                if (cur_phase == ROOM_PHASE::PLAYING)
                {
                    {
                        lock_guard<mutex> lk(m_clients[c_id].m_s_lock);
                        float px = p->world_pos[0];
                        float py = p->world_pos[1];
                        if (py < WorldMatrixInfo::GROUND_HEIGHT)
                            py = WorldMatrixInfo::GROUND_HEIGHT;
                        float pz = p->world_pos[2];
                        m_clients[c_id].m_worldMatrix.SetPosition(px, py, pz);
                        m_clients[c_id].m_player.fVerticalVelocity = 0.f;
                        m_clients[c_id].m_player.bIsGrounded       = true;
                    }
                    // 갱신된 권위 위치를 방 전원에게 즉시 전파
                    for (int pid : room->GetPlayerIds()) {
                        if (m_clients[pid].m_state != ST_INGAME) continue;
                        m_clients[pid].Send_Move_Packet(c_id, this);
                    }
                    cout << "[Instance] Client [" << c_id << "] spawn pos race-patched mid-PLAYING" << endl;
                }
            }
        }
        cout << "[Instance] Client [" << c_id << "] spawn pos ("
             << p->world_pos[0] << ", " << p->world_pos[1] << ", " << p->world_pos[2] << ")" << endl;
        break;
    }

    case CS_PHASE_READY: {
        if (sz < (int)sizeof(CS_PHASE_READY_PACKET)) break;
        CS_PHASE_READY_PACKET* p = reinterpret_cast<CS_PHASE_READY_PACKET*>(packet);
        int room_id = m_clients[c_id].m_room_id;
        if (room_id != -1)
            RoomPhaseManager::GetInstance()->OnPlayerPhaseReady(room_id, p->phase);
        break;
    }

    case CS_MAP_LOADED: {
        if (sz < (int)sizeof(CS_MAP_LOADED_PACKET)) break;
        CS_MAP_LOADED_PACKET* p = reinterpret_cast<CS_MAP_LOADED_PACKET*>(packet);
        int room_id = m_clients[c_id].m_room_id;
        if (room_id != -1)
            RoomPhaseManager::GetInstance()->OnMapLoaded(room_id, p->slot);
        break;
    }

    case CS_HIT: {
        if (sz < (int)sizeof(CS_HIT_PACKET)) break;
        CS_HIT_PACKET* p = reinterpret_cast<CS_HIT_PACKET*>(packet);

        GameSession& shooter = m_clients[c_id];
        if (shooter.m_state != ST_INGAME) break;
        int room_id = shooter.m_room_id;
        auto* room = GetRoom(room_id);
        if (!room || !room->IsActive()) break;
        if (RoomPhaseManager::GetInstance()->GetRoomPhase(room_id) != ROOM_PHASE::PLAYING) break;

        // victim 세션 탐색 (로비 ID 기준)
        int victim_cid = -1;
        for (int pid : room->GetPlayerIds()) {
            if (m_clients[pid].m_lobby_player_id == p->victim_id) {
                victim_cid = pid; break;
            }
        }
        if (victim_cid < 0) break;

        GameSession& victim = m_clients[victim_cid];
        if (!victim.m_bAlive) break;

        // 거리 합리성 (최대 150 유닛 — 맵 스케일 × 50 적용)
        XMFLOAT3 sPos = { shooter.m_worldMatrix.m[12], shooter.m_worldMatrix.m[13], shooter.m_worldMatrix.m[14] };
        XMFLOAT3 vPos = { victim.m_worldMatrix.m[12],  victim.m_worldMatrix.m[13],  victim.m_worldMatrix.m[14] };
        float dist = XMVectorGetX(XMVector3Length(
            XMVectorSubtract(XMLoadFloat3(&vPos), XMLoadFloat3(&sPos))));
        if (dist > 150.f) break;

        // LOS: 벽에 가로막히면 거부
        if (!CheckLOS(sPos, vPos)) break;

        // 데미지 테이블 (Collision_Manager.cpp:235-267 동일)
        static constexpr int s_dmg[10] = { 50, 20, 20, 20, 20, 15, 15, 15, 15, 30 };
        int part = (p->part_num < 10) ? p->part_num : 9;
        bool died = false;
        {
            lock_guard<mutex> lk(victim.m_s_lock);
            victim.m_hp -= s_dmg[part];
            died = (victim.m_hp <= 0);
            if (died) { victim.m_hp = 0; victim.m_bAlive = false; }
        }

        // SC_HIT 브로드캐스트 (이펙트 위치 포함)
        SC_HIT_PACKET hit{};
        hit.size       = sizeof(SC_HIT_PACKET);
        hit.type       = SC_HIT;
        hit.shooter_id = shooter.m_lobby_player_id;
        hit.victim_id  = p->victim_id;
        hit.victim_hp  = static_cast<short>(victim.m_hp);
        hit.part_num   = p->part_num;
        hit.hit_pos[0] = p->hit_pos[0];
        hit.hit_pos[1] = p->hit_pos[1];
        hit.hit_pos[2] = p->hit_pos[2];
        for (int pid : room->GetPlayerIds()) {
            if (m_clients[pid].m_state == ST_INGAME)
                m_clients[pid].Send(&hit);
        }

        if (died) {
            SC_DEATH_PACKET death{};
            death.size      = sizeof(SC_DEATH_PACKET);
            death.type      = SC_DEATH;
            death.victim_id = p->victim_id;
            death.killer_id = shooter.m_lobby_player_id;
            for (int pid : room->GetPlayerIds()) {
                if (m_clients[pid].m_state == ST_INGAME)
                    m_clients[pid].Send(&death);
            }
            cout << "[Hit] Player " << shooter.m_lobby_player_id
                 << " killed player " << p->victim_id << endl;

            // 팀 전멸 여부 확인 → 전멸 시 라운드 종료
            RoomPhaseManager::GetInstance()->OnPlayerDied(room_id);
        }
        break;
    }

    case CS_LOGOUT: {
        cout << "[Instance] Client [" << c_id << "] logout requested." << endl;
        Disconnect(c_id);
        break;
    }

    case CS_WALL_HIT: {
        if (sz < (int)sizeof(CS_WALL_HIT_PACKET)) break;
        CS_WALL_HIT_PACKET* p = reinterpret_cast<CS_WALL_HIT_PACKET*>(packet);

        GameSession& s = m_clients[c_id];
        if (s.m_state != ST_INGAME) break;
        int room_id = s.m_room_id;
        auto* room = GetRoom(room_id);
        if (!room || !room->IsActive()) break;
        if (RoomPhaseManager::GetInstance()->GetRoomPhase(room_id) != ROOM_PHASE::PLAYING) break;

        cout << "[WallHit] Player " << s.m_lobby_player_id
             << " hit wall_id=" << p->wall_id << endl;

        SC_WALL_BREAK_PACKET wb{};
        wb.size       = sizeof(SC_WALL_BREAK_PACKET);
        wb.type       = SC_WALL_BREAK;
        wb.wall_id    = p->wall_id;
        wb.breaker_id = s.m_lobby_player_id;
        for (int pid : room->GetPlayerIds())
            if (m_clients[pid].m_state == ST_INGAME)
                m_clients[pid].Send(&wb);
        break;
    }
    }
}

bool GameSessionManager::CheckLOS(const XMFLOAT3& from, const XMFLOAT3& to) const
{
    if (m_mapPtrs.empty()) return true;

    XMVECTOR vFrom = XMLoadFloat3(&from);
    XMVECTOR vTo   = XMLoadFloat3(&to);
    XMVECTOR vDir  = XMVector3Normalize(XMVectorSubtract(vTo, vFrom));
    float maxDist  = XMVectorGetX(XMVector3Length(XMVectorSubtract(vTo, vFrom)));

    for (const SServerCollider* col : m_mapPtrs) {
        if (!col->enable) continue;
        float hitDist = 0.f;
        if (col->IntersectsRay(vFrom, vDir, hitDist) && hitDist < maxDist)
            return false;
    }
    return true;
}

// =============================================================================
// 연결 해제
// =============================================================================

void GameSessionManager::Disconnect(int c_id)
{
    int room_id = -1;
    {
        lock_guard<mutex> ll(m_clients[c_id].m_s_lock);
        if (m_clients[c_id].m_state == ST_FREE) return;
        // 잠금 안에서 즉시 ST_FREE로 전환해 다른 스레드의 이중 진입을 차단
        m_clients[c_id].m_state = ST_FREE;
        room_id = m_clients[c_id].m_room_id;
        m_clients[c_id].m_room_id = -1;
    }

    if (room_id != -1) {
        auto* room = GetRoom(room_id);
        if (room && room->IsActive()) {
            for (int pid : room->GetPlayerIds()) {
                if (pid == c_id) continue;
                if (m_clients[pid].m_state != ST_INGAME) continue;
                m_clients[pid].Send_Remove_Player_Packet(c_id, this);
            }
            room->RemovePlayer(c_id);
        }
        RoomPhaseManager::GetInstance()->OnPlayerLeft(room_id);
    }

    closesocket(m_clients[c_id].m_socket);
}

// =============================================================================
// 방 / 인증
// =============================================================================

void GameSessionManager::RegisterPendingRoom(const IS_ROOM_NOTIFY_PACKET& pkt)
{
    // 방 초기화 (플레이어 없는 빈 방으로 등록)
    vector<int> empty_ids;
    {
        lock_guard<mutex> ll(m_room_lock);
        m_rooms[pkt.room_id].Initialize(pkt.room_id, empty_ids);
    }

    // 인증 토큰 저장
    {
        lock_guard<mutex> ll(m_pending_lock);
        m_pending_tokens[pkt.room_id] = string(pkt.auth_token);
    }

    // 페이즈 매니저에 방 등록
    RoomPhaseManager::GetInstance()->OnRoomRegistered(pkt);

    cout << "[Instance] Room " << pkt.room_id << " registered. Expecting "
         << pkt.player_count << " players." << endl;
}

bool GameSessionManager::AuthenticateJoin(int room_id, const char* auth_token)
{
    lock_guard<mutex> ll(m_pending_lock);
    auto it = m_pending_tokens.find(room_id);
    if (it == m_pending_tokens.end()) return false;
    return it->second == string(auth_token);
}

Room* GameSessionManager::GetRoom(int room_id)
{
    if (room_id < 0 || room_id >= MAX_ROOM) return nullptr;
    return &m_rooms[room_id];
}

int GameSessionManager::GetActiveRoomCount() const
{
    int count = 0;
    for (auto& room : m_rooms) {
        if (room.IsActive()) ++count;
    }
    return count;
}

int GameSessionManager::GetActivePlayerCount() const
{
    int count = 0;
    for (auto& client : m_clients) {
        if (client.m_state == ST_INGAME) ++count;
    }
    return count;
}
