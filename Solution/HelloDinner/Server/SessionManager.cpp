#include "SessionManager.h"

// 새로운 클라이언트가 접속했을 때, 빈 슬롯을 찾아서 그 슬롯의 인덱스(id)를 반환
int SessionManager::GetNewClientId()
{
    for (int i = 0; i < MAX_USER; ++i) {
        lock_guard<mutex> ll{ m_clients[i].m_s_lock };
        if (m_clients[i].m_state == ST_FREE) {
            cout << i << ". New Client Connected." << endl;
            return i;
        }
    }
    return -1;
}

// 클라이언트로부터 받은 패킷을 처리
void SessionManager::ProcessPacket(int c_id, char* packet)
{
    switch (packet[1]) {
    case CS_LOGIN: {
        CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
        strcpy_s(m_clients[c_id].m_name, p->name);
        m_clients[c_id].x = rand() % W_WIDTH;
        m_clients[c_id].y = rand() % W_HEIGHT;
		m_clients[c_id].z = 0;

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
        break;
    }
    case CS_MOVE: {
        CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
        m_clients[c_id].m_last_move_time = p->move_time;
        short x = m_clients[c_id].x;
        short y = m_clients[c_id].y;
        short z = m_clients[c_id].z;

        switch (p->direction) {
        case 0: if (y > 0) y--; break;
        case 1: if (y < W_HEIGHT - 1) y++; break;
        case 2: if (x > 0) x--; break;
        case 3: if (x < W_WIDTH - 1) x++; break;
        }
        m_clients[c_id].x = x;
        m_clients[c_id].y = y;
        m_clients[c_id].z = z;

        for (auto& cl : m_clients) {
            if (cl.m_state != ST_INGAME) continue;
            cl.Send_Move_Packet(c_id);
        }
        break;
    }
    }
}

// 연결 해제
void SessionManager::Disconnect(int c_id)
{
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