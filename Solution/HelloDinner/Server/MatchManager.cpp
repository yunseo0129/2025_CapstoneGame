#include "MatchManager.h"
#include "SessionManager.h"
#include "InstanceManager.h"
#include "RoomManager.h"

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
	constexpr int MIN_MATCH_PLAYERS = 2;
	int queueSize = static_cast<int>(m_wait_queue.size());
	if (queueSize < MIN_MATCH_PLAYERS)
		return;

	int matchCount = min(queueSize, ROOM_MAX_PLAYER);
	vector<int> matched_players(m_wait_queue.begin(), m_wait_queue.begin() + matchCount);
	m_wait_queue.erase(m_wait_queue.begin(), m_wait_queue.begin() + matchCount);

	LaunchRoom(matched_players);
}

void MatchManager::LaunchRoom(const vector<int>& players)
{
	if (players.empty()) return;

	auto* bestInst = InstanceManager::GetInstance()->SelectBestInstance();
	if (!bestInst) {
		cout << "[Match] No available instance server! Waiting...\n";
		return;
	}

	int room_id;
	{
		lock_guard<mutex> ll(m_room_id_lock);
		room_id = m_next_room_id++;
	}

	auto* sm = SessionManager::GetInstance();
	int playerCount = (int)players.size();

	cout << "========================================" << endl;
	cout << "  [LAUNCH ROOM] Room " << room_id
		<< " -> Instance #" << bestInst->id
		<< " (" << bestInst->ip << ":" << bestInst->port << ")" << endl;
	cout << "  Players:";

	char auth_token[32] = {};
	snprintf(auth_token, sizeof(auth_token), "TK_%d_%d", room_id,
		static_cast<int>(steady_clock::now().time_since_epoch().count() % 100000));

	IS_ROOM_NOTIFY_PACKET notify;
	notify.size = sizeof(IS_ROOM_NOTIFY_PACKET);
	notify.type = IS_ROOM_NOTIFY;
	notify.room_id = room_id;
	notify.player_count = playerCount;
	strcpy_s(notify.auth_token, sizeof(notify.auth_token), auth_token);
	memset(notify.player_ids, -1, sizeof(notify.player_ids));
	memset(notify.player_names, 0, sizeof(notify.player_names));
	memset(notify.player_teams, 0xFF, sizeof(notify.player_teams));
	memset(notify.player_slots, 0,    sizeof(notify.player_slots));

	auto* rm = RoomManager::GetInstance();
	for (int i = 0; i < playerCount; ++i) {
		notify.player_ids[i] = players[i];
		strcpy_s(notify.player_names[i], sizeof(notify.player_names[i]),
			sm->GetClient(players[i]).m_player.name);
		auto [team, slot] = rm->GetSeat(players[i]);
		notify.player_teams[i] = team;
		notify.player_slots[i] = slot;
	}

	InstanceManager::GetInstance()->NotifyRoomToInstance(bestInst, notify);

	for (int i = 0; i < playerCount; ++i) {
		int pid = players[i];
		auto& client = sm->GetClient(pid);
		{
			lock_guard<mutex> ll(client.m_s_lock);
			client.m_state   = ST_INGAME;
			client.m_room_id = room_id;
		}
		cout << " " << pid << "(" << client.m_player.name << ")";

		SC_REDIRECT_PACKET rp;
		rp.size = sizeof(SC_REDIRECT_PACKET);
		rp.type = SC_REDIRECT;
		strcpy_s(rp.ip, sizeof(rp.ip), bestInst->ip);
		rp.port    = bestInst->port;
		rp.room_id = room_id;
		strcpy_s(rp.auth_token, sizeof(rp.auth_token), auth_token);
		client.Send(&rp);
	}
	cout << endl;
	cout << "========================================" << endl;
}