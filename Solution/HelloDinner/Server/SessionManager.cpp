#include "SessionManager.h"

// 새로운 클라이언트가 접속했을 때, 빈 슬롯을 찾아서 그 슬롯의 인덱스(id)를 반환
int SessionManager::GetNewClientId()
{
    for (int i = 0; i < MAX_USER; ++i) {
        lock_guard<mutex> ll{ m_clients[i].m_s_lock };
        if (m_clients[i].m_state == ST_FREE) {
            cout << " New Client[" << i << "] Connected." << endl;
            return i;
        }
    }
    return -1;
}

// 클라이언트로부터 받은 패킷을 처리
void SessionManager::ProcessPacket(int c_id, char* packet)
{
    switch (packet[1]) {
        // 로그인 패킷 처리
    case CS_LOGIN: {
        CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
        strcpy_s(m_clients[c_id].m_name, p->name);

        // 임의의 위치로 설정
        m_clients[c_id].m_Positionx = c_id * 100;
        m_clients[c_id].m_Positiony = 0;
        m_clients[c_id].m_Positionz = 0;
        m_clients[c_id].m_Rotationx = 0;
        m_clients[c_id].m_Rotationy = 0;
        m_clients[c_id].m_Rotationz = 0;

        m_clients[c_id].Send_Login_Info_Packet();
        {
            lock_guard<mutex> ll{ m_clients[c_id].m_s_lock };
            m_clients[c_id].m_state = ST_INGAME;
        }
        for (auto& pl : m_clients) {
            {
                lock_guard<mutex> ll(pl.m_s_lock);
                if (ST_INGAME != pl.m_state) continue;
            }
            if (pl.m_id == c_id) continue;
            pl.Send_Add_Player_Packet(c_id);
            m_clients[c_id].Send_Add_Player_Packet(pl.m_id);
        }
		std::cout << "Client [" << c_id << "] logged in as " << p->name << "." << std::endl;
        break;
    }
        // 이동 패킷 처리
    case CS_MOVE: {
        CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
        
        // Todo: 움직임 로직

        for (auto& cl : m_clients) {
            if (cl.m_state != ST_INGAME) continue;
            cl.Send_Move_Packet(c_id);
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

    for (auto& pl : m_clients) {
        {
            lock_guard<mutex> ll(pl.m_s_lock);
            if (ST_INGAME != pl.m_state) continue;
        }
        if (pl.m_id == c_id) continue; 
        pl.Send_Remove_Player_Packet(c_id);
    }
    closesocket(m_clients[c_id].m_socket);

    lock_guard<mutex> ll(m_clients[c_id].m_s_lock);
    m_clients[c_id].m_state = ST_FREE;
}