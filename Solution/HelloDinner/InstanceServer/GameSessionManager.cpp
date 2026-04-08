#include "GameSessionManager.h"

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

void GameSessionManager::ProcessPacket(int c_id, char* packet)
{
    switch (packet[1]) {
    case CS_JOIN_ROOM: {
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
            // 기존 방 플레이어에게 신규 플레이어 알림
            for (int pid : room->GetPlayerIds()) {
                if (m_clients[pid].m_state != ST_INGAME) continue;
                m_clients[pid].Send_Add_Player_Packet(c_id, this);
                // 신규 플레이어에게 기존 플레이어 알림
                session.Send_Add_Player_Packet(pid, this);
            }
            // 방의 플레이어 목록에 추가
            vector<int> ids = room->GetPlayerIds();
            ids.push_back(c_id);
            room->Initialize(p->room_id, ids);
        }

        cout << "[Instance] Client [" << c_id << "] joined room " << p->room_id
             << " as " << p->name << endl;
        break;
    }
    case CS_MOVE: {
        CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);

        auto& session = m_clients[c_id];
        session.m_player.keyInput = p->keyInput;

        unsigned int serverNow = GameSession::GetServerTimestamp();
        float fTimeDelta = 0.f;
        if (session.m_lastServerTimestamp != 0) {
            fTimeDelta = (serverNow - session.m_lastServerTimestamp) / 1000.f;
            if (fTimeDelta > 0.1f) fTimeDelta = 0.1f;
        }
        session.UpdateTimestamp(p->timestamp);

        session.m_worldMatrix.CalculateMovement(
            p->keyInput,
            p->mouseYaw,
            session.m_player.speedPerSec,
            session.m_player.rotationPerSec,
            fTimeDelta
        );

        if (!session.m_worldMatrix.IsPositionClose(p->worldMatrix)) {
            cout << "[Instance] Client [" << c_id << "] position mismatch detected." << endl;
        }

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
    case CS_LOGOUT: {
        cout << "[Instance] Client [" << c_id << "] logout requested." << endl;
        Disconnect(c_id);
        break;
    }
    }
}

void GameSessionManager::Disconnect(int c_id)
{
    {
        lock_guard<mutex> ll(m_clients[c_id].m_s_lock);
        if (m_clients[c_id].m_state == ST_FREE) return;
    }

    int room_id = m_clients[c_id].m_room_id;

    if (room_id != -1) {
        auto* room = GetRoom(room_id);
        if (room && room->IsActive()) {
            for (int pid : room->GetPlayerIds()) {
                if (pid == c_id) continue;
                if (m_clients[pid].m_state != ST_INGAME) continue;
                m_clients[pid].Send_Remove_Player_Packet(c_id);
            }
            room->RemovePlayer(c_id);
        }
    }

    closesocket(m_clients[c_id].m_socket);

    lock_guard<mutex> ll(m_clients[c_id].m_s_lock);
    m_clients[c_id].m_state = ST_FREE;
    m_clients[c_id].m_room_id = -1;
}

void GameSessionManager::RegisterPendingRoom(const IS_ROOM_NOTIFY_PACKET& pkt)
{
    // 방 생성 (빈 플레이어 목록으로, 실제 접속 시 추가)
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