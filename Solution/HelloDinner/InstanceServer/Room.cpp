#include "Room.h"

void Room::Initialize(int room_id, const vector<int>& player_ids)
{
    lock_guard<mutex> ll(m_room_lock);
    m_room_id = room_id;
    m_player_ids = player_ids;
    m_active = true;

    cout << "[Room " << room_id << "] Created with players:";
    for (int id : m_player_ids)
        cout << " " << id;
    cout << endl;
}

void Room::RemovePlayer(int c_id)
{
    lock_guard<mutex> ll(m_room_lock);
    auto it = find(m_player_ids.begin(), m_player_ids.end(), c_id);
    if (it != m_player_ids.end()) {
        m_player_ids.erase(it);
        cout << "[Room " << m_room_id << "] Player " << c_id << " left. Remaining: " << m_player_ids.size() << endl;
    }

    if (m_player_ids.empty()) {
        m_active = false;
        cout << "[Room " << m_room_id << "] Closed." << endl;
    }
}

bool Room::HasPlayer(int c_id) const
{
    lock_guard<mutex> ll(m_room_lock);
    return find(m_player_ids.begin(), m_player_ids.end(), c_id) != m_player_ids.end();
}

vector<int> Room::GetPlayerIds() const
{
    lock_guard<mutex> ll(m_room_lock);
    return m_player_ids;
}