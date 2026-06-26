#pragma once
#include "GameSession.h"
#include "Room.h"
#include "ServerCollision.h"
#include "MapDataLoader.h"

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

    // ─── 맵 이동 충돌 ─────────────────────────────────────────────────────
    // jsonPath: MapData.json 경로
    // 서버 기동 시 1회 호출 → 정적 맵 콜라이더 로드 + BVH 빌드
    bool Build_MapCollision(const std::string& jsonPath);

    // CheckMove 래퍼: me sphere 에 대해 맵 BVH/콜라이더와 충돌 해소
    bool CheckMove_Player(SServerCollider* me, const XMFLOAT3& move, XMFLOAT3& outSlide);

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

    // ─── 맵 충돌 데이터 ────────────────────────────────────────────────────
    // m_mapColliders: LoadMapColliders 가 생성한 SServerCollider 소유 배열
    // m_mapPtrs    : Build 에 넘길 포인터 목록 (m_mapColliders 원소 주소)
    // m_mapBVH     : 정적 맵 BVH
    std::vector<SServerCollider>  m_mapColliders;
    std::vector<SServerCollider*> m_mapPtrs;
    SSVBH                         m_mapBVH;
};
