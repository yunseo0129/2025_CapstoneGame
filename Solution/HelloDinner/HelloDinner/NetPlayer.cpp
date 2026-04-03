#include "stdafx.h"
#include "NetPlayer.h"

void NetPlayer::OnAdded(int id, const float* worldMatrix, const char* name)
{
    m_id = id;
    memcpy(m_worldMatrix, worldMatrix, sizeof(float) * 16);
    strcpy_s(m_name, name);
    m_bActive = true;

    PushEvent(EventType::ADDED);
}

void NetPlayer::OnRemoved()
{
    m_bActive = false;
    PushEvent(EventType::REMOVED);
}

void NetPlayer::OnMoved(unsigned char keyInput, const float* worldMatrix)
{
    memcpy(m_worldMatrix, worldMatrix, sizeof(float) * 16);
    m_keyInput = keyInput;

    PushEvent(EventType::MOVED);
}

void NetPlayer::PopEvents(std::vector<Event>& outEvents)
{
    std::lock_guard<std::mutex> lk(m_eventLock);
    outEvents.insert(outEvents.end(), m_pendingEvents.begin(), m_pendingEvents.end());
    m_pendingEvents.clear();
}

void NetPlayer::PushEvent(EventType type)
{
    Event evt{};
    evt.type = type;
    evt.id = m_id;
    evt.keyInput = m_keyInput;
    memcpy(evt.worldMatrix, m_worldMatrix, sizeof(float) * 16);
    strcpy_s(evt.name, m_name);

    std::lock_guard<std::mutex> lk(m_eventLock);
    m_pendingEvents.push_back(evt);
}