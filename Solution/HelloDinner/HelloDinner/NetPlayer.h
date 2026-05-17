#pragma once

#include "stdafx.h"

// 네트워크 플레이어: 서버에서 수신한 상태를 관리하고 이벤트를 발생시킨다.
class NetPlayer
{
public:
    enum class EventType {
        ADDED,
        REMOVED,
        MOVED,
    };

    struct Event {
        EventType       type;
        int             id = -1;
        float           worldMatrix[16] = {};
        unsigned char   keyInput = 0;
        char            name[NAME_SIZE] = {};
    };

public:
    NetPlayer() = default;
    ~NetPlayer() = default;

    // --- 상태 접근 ---
    int             GetId()         const { return m_id; }
    const char*     GetName()       const { return m_name; }
    const float*    GetWorldMatrix()const { return m_worldMatrix; }
    unsigned char   GetKeyInput()   const { return m_keyInput; }
    bool            IsActive()      const { return m_bActive; }

    // --- RecvThread에서 호출: 서버 패킷으로 상태 갱신 + 이벤트 생성 ---
    void OnAdded(int id, const float* worldMatrix, const char* name);
    void OnRemoved();
    void OnMoved(unsigned char keyInput, const float* worldMatrix);

    // --- 메인 스레드에서 호출: 보류 이벤트를 모두 가져온다 ---
    void PopEvents(std::vector<Event>& outEvents);

private:
    void PushEvent(EventType type);

private:
    int             m_id = -1;
    float           m_worldMatrix[16] = {};
    unsigned char   m_keyInput = 0;
    char            m_name[NAME_SIZE] = {};
    bool            m_bActive = false;

    std::vector<Event>  m_pendingEvents;
    std::mutex          m_eventLock;
};