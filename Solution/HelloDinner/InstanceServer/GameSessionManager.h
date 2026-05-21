#pragma once
#include "GameSession.h"
#include "../Server/Room.h"

// 인스턴스 서버에서 사용할 세션+룸 관리자
class GameSessionManager
{
public:
    static GameSessionManager* GetInstance()
    {
        static GameSessionManager instance;
        return &instance;
    }

    GameSession& GetClient(int id) { return m_clients[id]; }

    int GetNewClientId();
    void ProcessPacket(int c_id, char* packet);
    void Disconnect(int c_id);

    // 로비에서 전달받은 방 정보를 등록 (인증 토큰 포함)
    void RegisterPendingRoom(const IS_ROOM_NOTIFY_PACKET& pkt);

    // CS_JOIN_ROOM 시 인증 확인
    bool AuthenticateJoin(int room_id, const char* auth_token);

    Room* GetRoom(int room_id);

    // 현재 활성 방 수와 플레이어 수 집계
    int GetActiveRoomCount() const;
    int GetActivePlayerCount() const;

private:
    GameSessionManager() = default;
    ~GameSessionManager() = default;
    GameSessionManager(const GameSessionManager&) = delete;
    GameSessionManager& operator=(const GameSessionManager&) = delete;

    array<GameSession, MAX_USER> m_clients;

    mutex m_room_lock;
    array<Room, MAX_ROOM> m_rooms;

    // 방별 인증 토큰 (room_id → auth_token)
    mutex m_pending_lock;
    unordered_map<int, string> m_pending_tokens;
};