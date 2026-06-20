#include "SessionManager.h"
#include "MatchManager.h"
#include "RoomManager.h"

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

// 로비 패킷 처리 — 로그인/로그아웃 + 수동 방 + 빠른 매칭
void SessionManager::ProcessPacket(int c_id, char* packet)
{
    switch (packet[1]) {
    case CS_LOGIN: {
        CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
        strcpy_s(m_clients[c_id].m_player.name, p->name);

        m_clients[c_id].Send_Login_Info_Packet();
        {
            lock_guard<mutex> ll{ m_clients[c_id].m_s_lock };
            m_clients[c_id].m_state = ST_LOBBY;
        }
        // 자동 매칭 큐 진입 제거 — CS_QUICK_MATCH로 옵트인
        cout << "[Lobby] Client [" << c_id << "] logged in as " << p->name << "." << endl;
        break;
    }
    case CS_LOGOUT: {
        cout << "[Lobby] Client [" << c_id << "] logout requested." << endl;
        Disconnect(c_id);
        break;
    }
    case CS_QUICK_MATCH: {
        cout << "[Lobby] Client [" << c_id << "] entered quick match queue." << endl;
        MatchManager::GetInstance()->EnqueuePlayer(c_id);
        break;
    }
    case CS_CREATE_ROOM: {
        int code = RoomManager::GetInstance()->CreateRoom(c_id);

        SC_ROOM_CREATED_PACKET rp{};
        rp.size = sizeof(SC_ROOM_CREATED_PACKET);
        rp.type = SC_ROOM_CREATED;
        rp.code = code;
        m_clients[c_id].Send(&rp);

        RoomManager::GetInstance()->BroadcastRoomUpdate(code);
        break;
    }
    case CS_JOIN_ROOM_CODE: {
        CS_JOIN_ROOM_CODE_PACKET* p = reinterpret_cast<CS_JOIN_ROOM_CODE_PACKET*>(packet);
        ROOM_JOIN_RESULT result = RoomManager::GetInstance()->JoinRoom(p->code, c_id);

        SC_ROOM_JOIN_RESULT_PACKET rp{};
        rp.size   = sizeof(SC_ROOM_JOIN_RESULT_PACKET);
        rp.type   = SC_ROOM_JOIN_RESULT;
        rp.result = (unsigned char)result;
        rp.code   = (result == RJR_OK) ? p->code : 0;
        m_clients[c_id].Send(&rp);

        if (result == RJR_OK)
            RoomManager::GetInstance()->BroadcastRoomUpdate(p->code);
        break;
    }
    case CS_LEAVE_ROOM: {
        // RoomManager::LeaveRoom이 내부에서 남은 멤버에게 브로드캐스트
        RoomManager::GetInstance()->LeaveRoom(c_id);
        break;
    }
    case CS_SELECT_SEAT: {
        CS_SELECT_SEAT_PACKET* p = reinterpret_cast<CS_SELECT_SEAT_PACKET*>(packet);
        RoomManager::GetInstance()->SelectSeat(c_id, p->team, p->slot);
        break;
    }
    case CS_START_GAME: {
        // 방장 c_id의 방 코드를 역참조하기 위해 RoomManager 내부에서 처리
        // StartRoom은 내부에서 host 검증 후 멤버 목록 반환
        // 코드를 알려면 RoomManager에 별도 조회 메서드가 필요하므로
        // LaunchRoom은 RoomManager::StartRoom이 성공해야 호출
        // ── RoomManager에 GetCodeOf(c_id) 없이 처리하기 위해 직접 로직 ──
        // SessionManager는 코드를 모르므로 RoomManager에 StartByHost 위임
        vector<int> members;
        int code = -1;
        if (RoomManager::GetInstance()->StartByHost(c_id, code, members)) {
            MatchManager::GetInstance()->LaunchRoom(members);
        }
        break;
    }
    default: {
        cout << "[Lobby] Unexpected packet type " << static_cast<int>(packet[1])
             << " from client [" << c_id << "], ignoring." << endl;
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

    MatchManager::GetInstance()->DequeuePlayer(c_id);
    RoomManager::GetInstance()->LeaveRoom(c_id);

    closesocket(m_clients[c_id].m_socket);

    lock_guard<mutex> ll(m_clients[c_id].m_s_lock);
    m_clients[c_id].m_state   = ST_FREE;
    m_clients[c_id].m_room_id = -1;
}
