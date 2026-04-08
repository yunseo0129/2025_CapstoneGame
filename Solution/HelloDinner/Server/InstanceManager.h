#pragma once
#include "InstanceInfo.h"

class InstanceManager
{
public:
    static InstanceManager* GetInstance()
    {
        static InstanceManager instance;
        return &instance;
    }

    // 인스턴스 서버의 내부 접속을 받기 위한 리스닝 시작
    bool StartInternalListener();

    // 내부 통신 수신 스레드 (인스턴스 서버 등록 + Heartbeat)
    void InternalAcceptThread();
    void InternalRecvThread(int inst_id);

    // 등록된 인스턴스 서버 중 가장 여유로운 서버 선택
    InstanceInfo* SelectBestInstance();

    // 인스턴스 서버 등록
    void RegisterInstance(int id, const char* ip, unsigned short port, SOCKET sock);

    // Heartbeat 수신 처리
    void UpdateHeartbeat(const IS_HEARTBEAT_PACKET& pkt);

    // 매칭 완료 시 인스턴스 서버에 방 정보 전달
    void NotifyRoomToInstance(InstanceInfo* inst, const IS_ROOM_NOTIFY_PACKET& pkt);

    // 인스턴스 정보 조회
    InstanceInfo* GetInstanceInfo(int id);

private:
    InstanceManager() = default;
    ~InstanceManager() = default;
    InstanceManager(const InstanceManager&) = delete;
    InstanceManager& operator=(const InstanceManager&) = delete;

    mutex m_lock;
    array<InstanceInfo, MAX_INSTANCE_SERVERS> m_instances;
    SOCKET m_internal_listen = INVALID_SOCKET;
};