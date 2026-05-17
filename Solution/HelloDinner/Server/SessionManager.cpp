#include "SessionManager.h"
#include "MatchManager.h"

// 새로운 클라이언트가 접속했을 때, 빈 슬롯을 찾아서 그 슬롯의 인덱스(id)를 반환
int SessionManager::GetNewClientId()
{
    for (int i = 0; i < MAX_USER; ++i) {
        lock_guard<mutex> ll{ m_clients[i].m_s_lock };
        if (m_clients[i].m_state == ST_FREE) {
            cout << "[Lobby] New Client[" << i << "] Connected." << endl;
            return i;
        }
    }
    return -1;
}

// 로비에서는 로그인/로그아웃만 처리 (게임 로직은 InstanceServer에서 처리)
void SessionManager::ProcessPacket(int c_id, char* packet)
{
    switch (packet[1]) {
    // 로그인 패킷 처리 → 매칭 대기큐에 진입
    case CS_LOGIN: {
        CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
        strcpy_s(m_clients[c_id].m_player.name, p->name);

        m_clients[c_id].Send_Login_Info_Packet();
        {
            lock_guard<mutex> ll{ m_clients[c_id].m_s_lock };
            m_clients[c_id].m_state = ST_LOBBY;
        }

        // 매칭 대기큐에 등록
        MatchManager::GetInstance()->EnqueuePlayer(c_id);
        cout << "[Lobby] Client [" << c_id << "] logged in as " << p->name << "." << endl;
        break;
    }
    // 로그아웃 패킷 처리
    case CS_LOGOUT: {
        cout << "[Lobby] Client [" << c_id << "] logout requested." << endl;
        Disconnect(c_id);
        break;
    }
    default: {
        // CS_MOVE 등 게임 패킷이 로비에 도착한 경우 무시
        cout << "[Lobby] Unexpected packet type " << static_cast<int>(packet[1])
             << " from client [" << c_id << "], ignoring." << endl;
        break;
    }
    }
}

// 연결 해제 (로비에서는 매칭 큐 제거만 처리)
void SessionManager::Disconnect(int c_id)
{
    {
        lock_guard<mutex> ll(m_clients[c_id].m_s_lock);
        if (m_clients[c_id].m_state == ST_FREE) return;
    }

    // 매칭 대기큐에서 제거
    MatchManager::GetInstance()->DequeuePlayer(c_id);

    closesocket(m_clients[c_id].m_socket);

    lock_guard<mutex> ll(m_clients[c_id].m_s_lock);
    m_clients[c_id].m_state = ST_FREE;
    m_clients[c_id].m_room_id = -1;
}