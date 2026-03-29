#include "MatchManager.h"
#include "SessionManager.h"

void MatchManager::EnqueuePlayer(int c_id)
{
	lock_guard<mutex> ll(m_queue_lock);
	m_wait_queue.push_back(c_id);

	auto* sm = SessionManager::GetInstance();
	int queue_size = static_cast<int>(m_wait_queue.size());

	cout << "[Match] Player " << c_id << " (" << sm->GetClient(c_id).m_player.name
		<< ") entered queue. Waiting: " << queue_size << "/" << ROOM_MAX_PLAYER << endl;

	// 대기 중인 모든 플레이어에게 대기 상태 알림
	for (int id : m_wait_queue)
		sm->GetClient(id).Send_Match_Wait_Packet(queue_size);

	TryMatch();
}

void MatchManager::DequeuePlayer(int c_id)
{
	lock_guard<mutex> ll(m_queue_lock);
	auto it = find(m_wait_queue.begin(), m_wait_queue.end(), c_id);
	if (it != m_wait_queue.end()) {
		m_wait_queue.erase(it);
		cout << "[Match] Player " << c_id << " left queue. Waiting: " << m_wait_queue.size() << endl;
	}
}

void MatchManager::TryMatch()
{
	// m_queue_lock은 호출자가 이미 잡고 있는 상태
	if (static_cast<int>(m_wait_queue.size()) < ROOM_MAX_PLAYER)
		return;

	// 대기큐에서 ROOM_MAX_PLAYER명 추출
	vector<int> matched_players(m_wait_queue.begin(), m_wait_queue.begin() + ROOM_MAX_PLAYER);
	m_wait_queue.erase(m_wait_queue.begin(), m_wait_queue.begin() + ROOM_MAX_PLAYER);

	// 방 생성
	int room_id = GetNewRoomId();
	if (room_id == -1) {
		cout << "[Match] No available room slot!\n";
		// 다시 큐에 넣기
		m_wait_queue.insert(m_wait_queue.begin(), matched_players.begin(), matched_players.end());
		return;
	}

	m_rooms[room_id].Initialize(room_id, matched_players);

	auto* sm = SessionManager::GetInstance();
	int player_ids_arr[ROOM_MAX_PLAYER]{};
	for (int i = 0; i < ROOM_MAX_PLAYER; ++i)
		player_ids_arr[i] = matched_players[i];

	cout << "========================================" << endl;
	cout << "  [MATCH SUCCESS] Room " << room_id << endl;
	cout << "  Players:";

	// 매칭된 플레이어 상태 전환 및 알림
	for (int i = 0; i < ROOM_MAX_PLAYER; ++i) {
		int pid = matched_players[i];
		auto& client = sm->GetClient(pid);
		{
			lock_guard<mutex> ll(client.m_s_lock);
			client.m_state = ST_INGAME;
			client.m_room_id = room_id;
		}
		cout << " " << pid << "(" << client.m_player.name << ")";
		client.Send_Match_Success_Packet(room_id, ROOM_MAX_PLAYER, player_ids_arr);
	}
	cout << endl;
	cout << "========================================" << endl;

	// 같은 방 플레이어끼리 서로 ADD_PLAYER
	for (int i = 0; i < ROOM_MAX_PLAYER; ++i) {
		for (int j = 0; j < ROOM_MAX_PLAYER; ++j) {
			if (i == j) continue;
			sm->GetClient(matched_players[i]).Send_Add_Player_Packet(matched_players[j]);
		}
	}
}

int MatchManager::GetNewRoomId()
{
	lock_guard<mutex> ll(m_room_lock);
	for (int i = 0; i < MAX_ROOM; ++i) {
		int id = (m_next_room_id + i) % MAX_ROOM;
		if (!m_rooms[id].IsActive()) {
			m_next_room_id = (id + 1) % MAX_ROOM;
			return id;
		}
	}
	return -1;
}

Room* MatchManager::GetRoom(int room_id)
{
	if (room_id < 0 || room_id >= MAX_ROOM) return nullptr;
	return &m_rooms[room_id];
}