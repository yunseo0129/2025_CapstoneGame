#include "RoomManager.h"
#include "SessionManager.h"
#include "MatchManager.h"

int RoomManager::GenerateUniqueCode()
{
    int code;
    do {
        code = 1000 + (rand() % 9000);
    } while (m_rooms.count(code));
    return code;
}

int RoomManager::CreateRoom(int host_c_id)
{
    lock_guard<mutex> ll(m_lock);

    // 이미 방에 있으면 먼저 나감
    auto it = m_player_room.find(host_c_id);
    if (it != m_player_room.end()) {
        int old_code = it->second;
        auto& room = m_rooms[old_code];
        room.members.erase(
            remove(room.members.begin(), room.members.end(), host_c_id),
            room.members.end());
        if (room.members.empty())
            m_rooms.erase(old_code);
        m_player_room.erase(it);
    }

    m_seat_map.erase(host_c_id);
    m_ready_map.erase(host_c_id);

    int code = GenerateUniqueCode();
    WaitingRoom room;
    room.code       = code;
    room.host_c_id  = host_c_id;
    room.members.push_back(host_c_id);

    m_rooms[code]           = move(room);
    m_player_room[host_c_id] = code;

    cout << "[Room] Created room " << code << " by client [" << host_c_id << "]\n";
    return code;
}

ROOM_JOIN_RESULT RoomManager::JoinRoom(int code, int c_id)
{
    lock_guard<mutex> ll(m_lock);

    auto rit = m_rooms.find(code);
    if (rit == m_rooms.end())
        return RJR_NOT_FOUND;

    WaitingRoom& room = rit->second;
    if (room.started)
        return RJR_STARTED;
    if ((int)room.members.size() >= ROOM_MAX_PLAYER)
        return RJR_FULL;

    // 이미 다른 방에 있으면 나감
    auto pit = m_player_room.find(c_id);
    if (pit != m_player_room.end()) {
        int old_code = pit->second;
        if (old_code == code) return RJR_OK; // 이미 이 방
        auto& old_room = m_rooms[old_code];
        old_room.members.erase(
            remove(old_room.members.begin(), old_room.members.end(), c_id),
            old_room.members.end());
        if (old_room.members.empty())
            m_rooms.erase(old_code);
        m_seat_map.erase(c_id);
        m_ready_map.erase(c_id);
    }

    room.members.push_back(c_id);
    m_player_room[c_id] = code;

    cout << "[Room] Client [" << c_id << "] joined room " << code
         << " (" << room.members.size() << "/" << ROOM_MAX_PLAYER << ")\n";
    return RJR_OK;
}

void RoomManager::LeaveRoom(int c_id)
{
    lock_guard<mutex> ll(m_lock);

    auto pit = m_player_room.find(c_id);
    if (pit == m_player_room.end()) return;

    int code = pit->second;
    m_player_room.erase(pit);
    m_seat_map.erase(c_id);
    m_ready_map.erase(c_id);

    auto rit = m_rooms.find(code);
    if (rit == m_rooms.end()) return;

    WaitingRoom& room = rit->second;
    room.members.erase(
        remove(room.members.begin(), room.members.end(), c_id),
        room.members.end());

    cout << "[Room] Client [" << c_id << "] left room " << code << "\n";

    if (room.members.empty()) {
        m_rooms.erase(rit);
        return;
    }

    // 방장이 나간 경우 다음 멤버에게 방장 이전
    if (room.host_c_id == c_id)
        room.host_c_id = room.members.front();

    int broadcast_code = code;
    vector<int> members_copy = room.members;
    int host_id = room.host_c_id;

    auto* sm = SessionManager::GetInstance();
    SC_ROOM_UPDATE_PACKET pkt{};
    pkt.size         = sizeof(SC_ROOM_UPDATE_PACKET);
    pkt.type         = SC_ROOM_UPDATE;
    pkt.code         = broadcast_code;
    pkt.host_id      = host_id;
    pkt.member_count = (unsigned char)members_copy.size();
    memset(pkt.member_ids,   -1,   sizeof(pkt.member_ids));
    memset(pkt.member_names, 0,    sizeof(pkt.member_names));
    memset(pkt.member_teams, 0xFF, sizeof(pkt.member_teams));
    memset(pkt.member_slots, 0,    sizeof(pkt.member_slots));
    memset(pkt.member_ready, 0,    sizeof(pkt.member_ready));
    for (int i = 0; i < (int)members_copy.size(); ++i) {
        int mid = members_copy[i];
        pkt.member_ids[i] = mid;
        strcpy_s(pkt.member_names[i], sm->GetClient(mid).m_player.name);
        auto sit = m_seat_map.find(mid);
        if (sit != m_seat_map.end()) {
            pkt.member_teams[i] = sit->second.first;
            pkt.member_slots[i] = sit->second.second;
        }
        auto rit2 = m_ready_map.find(mid);
        if (rit2 != m_ready_map.end() && rit2->second)
            pkt.member_ready[i] = 1;
    }
    for (int id : members_copy)
        sm->GetClient(id).Send(&pkt);
}

bool RoomManager::StartRoom(int code, int requester_c_id, vector<int>& out_members)
{
    lock_guard<mutex> ll(m_lock);

    auto rit = m_rooms.find(code);
    if (rit == m_rooms.end()) return false;

    WaitingRoom& room = rit->second;
    if (room.host_c_id != requester_c_id) return false;
    if (room.members.empty()) return false;
    if (room.started) return false;

    room.started = true;
    out_members  = room.members;

    // 맵에서 제거 (인스턴스 핸드오프 후 불필요)
    for (int id : room.members)
        m_player_room.erase(id);
    m_rooms.erase(rit);

    cout << "[Room] Room " << code << " started with " << out_members.size() << " players\n";
    return true;
}

bool RoomManager::StartByHost(int host_c_id, int& out_code, vector<int>& out_members)
{
    lock_guard<mutex> ll(m_lock);

    auto pit = m_player_room.find(host_c_id);
    if (pit == m_player_room.end()) return false;

    int code = pit->second;
    auto rit = m_rooms.find(code);
    if (rit == m_rooms.end()) return false;

    WaitingRoom& room = rit->second;
    if (room.host_c_id != host_c_id) return false;
    if (room.started) return false;

    // 비방장 멤버 전원이 준비완료 상태여야 시작 가능
    for (int id : room.members) {
        if (id == host_c_id) continue;
        auto rit2 = m_ready_map.find(id);
        if (rit2 == m_ready_map.end() || !rit2->second) {
            cout << "[Room] Start denied: client [" << id << "] is not ready.\n";
            return false;
        }
    }

    room.started  = true;
    out_code      = code;
    out_members   = room.members;

    for (int id : room.members) {
        m_player_room.erase(id);
        m_seat_map.erase(id);
        m_ready_map.erase(id);
    }
    m_rooms.erase(rit);

    cout << "[Room] Room " << code << " started with " << out_members.size() << " players\n";
    return true;
}

void RoomManager::BroadcastRoomUpdate(int code)
{
    lock_guard<mutex> ll(m_lock);

    auto rit = m_rooms.find(code);
    if (rit == m_rooms.end()) return;

    const WaitingRoom& room = rit->second;
    auto* sm = SessionManager::GetInstance();

    SC_ROOM_UPDATE_PACKET pkt{};
    pkt.size         = sizeof(SC_ROOM_UPDATE_PACKET);
    pkt.type         = SC_ROOM_UPDATE;
    pkt.code         = code;
    pkt.host_id      = room.host_c_id;
    pkt.member_count = (unsigned char)room.members.size();
    memset(pkt.member_ids,   -1,   sizeof(pkt.member_ids));
    memset(pkt.member_names, 0,    sizeof(pkt.member_names));
    memset(pkt.member_teams, 0xFF, sizeof(pkt.member_teams));
    memset(pkt.member_slots, 0,    sizeof(pkt.member_slots));
    memset(pkt.member_ready, 0,    sizeof(pkt.member_ready));
    for (int i = 0; i < (int)room.members.size(); ++i) {
        int mid = room.members[i];
        pkt.member_ids[i] = mid;
        strcpy_s(pkt.member_names[i], sm->GetClient(mid).m_player.name);
        auto sit = m_seat_map.find(mid);
        if (sit != m_seat_map.end()) {
            pkt.member_teams[i] = sit->second.first;
            pkt.member_slots[i] = sit->second.second;
        }
        auto rit = m_ready_map.find(mid);
        if (rit != m_ready_map.end() && rit->second)
            pkt.member_ready[i] = 1;
    }
    for (int id : room.members)
        sm->GetClient(id).Send(&pkt);
}

bool RoomManager::SelectSeat(int c_id, unsigned char team, unsigned char slot)
{
    lock_guard<mutex> ll(m_lock);

    auto pit = m_player_room.find(c_id);
    if (pit == m_player_room.end()) return false;

    int code = pit->second;

    if (team == 0xFF || slot == 0) {
        // 선택 해제
        m_seat_map.erase(c_id);
    } else {
        // 같은 방의 다른 멤버가 이미 같은 {team, slot}을 점유하면 거부
        auto rit = m_rooms.find(code);
        if (rit != m_rooms.end()) {
            for (int mid : rit->second.members) {
                if (mid == c_id) continue;
                auto sit = m_seat_map.find(mid);
                if (sit != m_seat_map.end() &&
                    sit->second.first == team && sit->second.second == slot)
                    return false;
            }
        }
        m_seat_map[c_id] = { team, slot };
    }

    // 방 전체에 갱신 브로드캐스트
    auto rit = m_rooms.find(code);
    if (rit == m_rooms.end()) return true;

    const WaitingRoom& room = rit->second;
    auto* sm = SessionManager::GetInstance();

    SC_ROOM_UPDATE_PACKET pkt{};
    pkt.size         = sizeof(SC_ROOM_UPDATE_PACKET);
    pkt.type         = SC_ROOM_UPDATE;
    pkt.code         = code;
    pkt.host_id      = room.host_c_id;
    pkt.member_count = (unsigned char)room.members.size();
    memset(pkt.member_ids,   -1,   sizeof(pkt.member_ids));
    memset(pkt.member_names, 0,    sizeof(pkt.member_names));
    memset(pkt.member_teams, 0xFF, sizeof(pkt.member_teams));
    memset(pkt.member_slots, 0,    sizeof(pkt.member_slots));
    memset(pkt.member_ready, 0,    sizeof(pkt.member_ready));
    for (int i = 0; i < (int)room.members.size(); ++i) {
        int mid = room.members[i];
        pkt.member_ids[i] = mid;
        strcpy_s(pkt.member_names[i], sm->GetClient(mid).m_player.name);
        auto sit = m_seat_map.find(mid);
        if (sit != m_seat_map.end()) {
            pkt.member_teams[i] = sit->second.first;
            pkt.member_slots[i] = sit->second.second;
        }
        auto rit2 = m_ready_map.find(mid);
        if (rit2 != m_ready_map.end() && rit2->second)
            pkt.member_ready[i] = 1;
    }
    for (int id : room.members)
        sm->GetClient(id).Send(&pkt);

    return true;
}

void RoomManager::SetReady(int c_id, bool ready)
{
    lock_guard<mutex> ll(m_lock);

    auto pit = m_player_room.find(c_id);
    if (pit == m_player_room.end()) return;

    int code = pit->second;
    if (ready)
        m_ready_map[c_id] = true;
    else
        m_ready_map.erase(c_id);

    cout << "[Room] Client [" << c_id << "] " << (ready ? "ready" : "unready") << " in room " << code << "\n";

    auto rit = m_rooms.find(code);
    if (rit == m_rooms.end()) return;

    const WaitingRoom& room = rit->second;
    auto* sm = SessionManager::GetInstance();

    SC_ROOM_UPDATE_PACKET pkt{};
    pkt.size         = sizeof(SC_ROOM_UPDATE_PACKET);
    pkt.type         = SC_ROOM_UPDATE;
    pkt.code         = code;
    pkt.host_id      = room.host_c_id;
    pkt.member_count = (unsigned char)room.members.size();
    memset(pkt.member_ids,   -1,   sizeof(pkt.member_ids));
    memset(pkt.member_names, 0,    sizeof(pkt.member_names));
    memset(pkt.member_teams, 0xFF, sizeof(pkt.member_teams));
    memset(pkt.member_slots, 0,    sizeof(pkt.member_slots));
    memset(pkt.member_ready, 0,    sizeof(pkt.member_ready));
    for (int i = 0; i < (int)room.members.size(); ++i) {
        int mid = room.members[i];
        pkt.member_ids[i] = mid;
        strcpy_s(pkt.member_names[i], sm->GetClient(mid).m_player.name);
        auto sit = m_seat_map.find(mid);
        if (sit != m_seat_map.end()) {
            pkt.member_teams[i] = sit->second.first;
            pkt.member_slots[i] = sit->second.second;
        }
        auto rit2 = m_ready_map.find(mid);
        if (rit2 != m_ready_map.end() && rit2->second)
            pkt.member_ready[i] = 1;
    }
    for (int id : room.members)
        sm->GetClient(id).Send(&pkt);
}
