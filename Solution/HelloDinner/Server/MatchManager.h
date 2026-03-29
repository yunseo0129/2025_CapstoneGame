#pragma once
#include "Room.h"

class MatchManager
{
public:
	static MatchManager* GetInstance()
	{
		static MatchManager instance;
		return &instance;
	}

	void EnqueuePlayer(int c_id);
	void DequeuePlayer(int c_id);

	Room* GetRoom(int room_id);

private:
	MatchManager() = default;
	~MatchManager() = default;
	MatchManager(const MatchManager&) = delete;
	MatchManager& operator=(const MatchManager&) = delete;

	void TryMatch();
	int  GetNewRoomId();

	mutex			m_queue_lock;
	vector<int>		m_wait_queue;

	mutex			m_room_lock;
	array<Room, MAX_ROOM> m_rooms;
	int				m_next_room_id = 0;
};