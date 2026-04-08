#pragma once
#include "pch.h"

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

private:
	MatchManager() = default;
	~MatchManager() = default;
	MatchManager(const MatchManager&) = delete;
	MatchManager& operator=(const MatchManager&) = delete;

	void TryMatch();

	mutex			m_queue_lock;
	vector<int>		m_wait_queue;

	// room_id 카운터 (인스턴스에 전달할 고유 번호)
	mutex			m_room_id_lock;
	int				m_next_room_id = 0;
};