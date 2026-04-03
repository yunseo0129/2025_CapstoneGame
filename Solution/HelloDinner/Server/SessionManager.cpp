#include "SessionManager.h"
#include "MatchManager.h"

// 새로운 클라이언트가 접속했을 때, 빈 슬롯을 찾아서 그 슬롯의 인덱스(id)를 반환
int SessionManager::GetNewClientId()
{
    for (int i = 0; i < MAX_USER; ++i) {
        lock_guard<mutex> ll{ m_clients[i].m_s_lock };
        if (m_clients[i].m_state == ST_FREE) {
            cout << "New Client[" << i << "] Connected." << endl;
            return i;
        }
    }
    return -1;
}

// 클라이언트로부터 받은 패킷을 처리
void SessionManager::ProcessPacket(int c_id, char* packet)
{
    switch (packet[1]) {
        // 로그인 패킷 처리 → 매칭 대기큐에 진입
    case CS_LOGIN: {
        CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
        strcpy_s(m_clients[c_id].m_player.name, p->name);

        // 초기 위치 설정

        m_clients[c_id].Send_Login_Info_Packet();
        {
            lock_guard<mutex> ll{ m_clients[c_id].m_s_lock };
            m_clients[c_id].m_state = ST_LOBBY;
        }

        // 매칭 대기큐에 등록
        MatchManager::GetInstance()->EnqueuePlayer(c_id);
        cout << "Client [" << c_id << "] logged in as " << p->name << "." << endl;
        break;
    }
        // 이동 패킷 처리 (같은 방 플레이어에게만 전송)
    case CS_MOVE: {
        CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);

        // 클라이언트가 보낸 데이터를 세션에 저장
        m_clients[c_id].m_player.keyInput = p->keyInput;

		// Todo: 움직임 로직 수정 필요 (현재는 단순히 키 입력에 따라 위치 변경)

        int room_id = m_clients[c_id].m_room_id;
        if (room_id == -1) break;

        auto* room = MatchManager::GetInstance()->GetRoom(room_id);
        if (!room || !room->IsActive()) break;

        for (int pid : room->GetPlayerIds()) {
            if (m_clients[pid].m_state != ST_INGAME) continue;
            m_clients[pid].Send_Move_Packet(c_id);
        }
        break;
    }
        // 로그아웃 패킷 처리
    case CS_LOGOUT: {
        cout << "Client [" << c_id << "] logout requested." << endl;
        Disconnect(c_id);
        break;
    }
    }
}

// 연결 해제
void SessionManager::Disconnect(int c_id)
{
    {
        lock_guard<mutex> ll(m_clients[c_id].m_s_lock);
        if (m_clients[c_id].m_state == ST_FREE) return;
    }

    int room_id = m_clients[c_id].m_room_id;

    // 매칭 대기큐에서 제거
    MatchManager::GetInstance()->DequeuePlayer(c_id);

    // 방에서 제거 및 같은 방 플레이어에게 알림
    if (room_id != -1) {
        auto* room = MatchManager::GetInstance()->GetRoom(room_id);
        if (room && room->IsActive()) {
            for (int pid : room->GetPlayerIds()) {
                if (pid == c_id) continue;
                if (m_clients[pid].m_state != ST_INGAME) continue;
                m_clients[pid].Send_Remove_Player_Packet(c_id);
            }
            room->RemovePlayer(c_id);
        }
    }

    closesocket(m_clients[c_id].m_socket);

    lock_guard<mutex> ll(m_clients[c_id].m_s_lock);
    m_clients[c_id].m_state = ST_FREE;
    m_clients[c_id].m_room_id = -1;
}