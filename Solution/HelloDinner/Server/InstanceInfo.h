#pragma once
#include "pch.h"

// 인스턴스 서버 1개의 상태 정보
struct InstanceInfo
{
    int             id = -1;
    char            ip[16] = {};
    unsigned short  port = 0;
    SOCKET          socket = INVALID_SOCKET;  // 로비↔인스턴스 내부 통신 소켓

    // 부하 정보 (Heartbeat로 갱신)
    int             current_rooms = 0;
    int             current_players = 0;
    float           cpu_usage = 0.f;
    bool            alive = false;

    steady_clock::time_point last_heartbeat;

    // 부하 점수 (낮을수록 여유)
    float GetLoadScore() const
    {
        // 가중치: 플레이어 수 60%, CPU 사용률 40%
        float playerLoad = static_cast<float>(current_players) / MAX_USER;
        float cpuLoad = cpu_usage / 100.f;
        return playerLoad * 0.6f + cpuLoad * 0.4f;
    }
};