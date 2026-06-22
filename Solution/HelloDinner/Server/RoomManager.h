#pragma once
#include "pch.h"

struct WaitingRoom {
    int         code;
    int         host_c_id;
    vector<int> members;
    bool        started = false;
};

class RoomManager
{
public:
    static RoomManager* GetInstance()
    {
        static RoomManager instance;
        return &instance;
    }

    int              CreateRoom(int host_c_id);
    ROOM_JOIN_RESULT JoinRoom(int code, int c_id);
    void             LeaveRoom(int c_id);
    bool             StartRoom(int code, int requester_c_id, vector<int>& out_members);
    bool             StartByHost(int host_c_id, int& out_code, vector<int>& out_members);
    void             BroadcastRoomUpdate(int code);
    // 팀/번호 선택 (team: 0=RED/1=BLUE/0xFF=해제, slot: 1~3/0=해제)
    // 충돌 시(다른 멤버가 동일 슬롯 점유) false 반환
    bool             SelectSeat(int c_id, unsigned char team, unsigned char slot);
    void             SetReady(int c_id, bool ready);
    // c_id의 팀·슬롯 반환. 미선택 시 {0xFF, 0}
    pair<unsigned char, unsigned char> GetSeat(int c_id);

private:
    RoomManager() = default;
    ~RoomManager() = default;
    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;

    int GenerateUniqueCode();

    mutex                           m_lock;
    unordered_map<int, WaitingRoom> m_rooms;        // code → 방
    unordered_map<int, int>         m_player_room;  // c_id → code
    unordered_map<int, pair<unsigned char, unsigned char>> m_seat_map; // c_id → {team, slot}
    unordered_map<int, bool> m_ready_map; // c_id → 준비 여부
};
