#pragma once
#include "pch.h"

class Room
{
public:
	Room() = default;
	~Room() = default;

	void Initialize(int room_id, const vector<int>& player_ids);
	void RemovePlayer(int c_id);

	bool IsActive() const { return m_active; }
	bool HasPlayer(int c_id) const;
	int  GetRoomId() const { return m_room_id; }
	const vector<int>& GetPlayerIds() const { return m_player_ids; }

private:
	int				m_room_id = -1;
	bool			m_active = false;
	mutex			m_room_lock;
	vector<int>		m_player_ids;
};