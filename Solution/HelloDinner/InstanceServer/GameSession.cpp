#include "GameSession.h"
#include "GameSessionManager.h"

GameSession::GameSession()
{
    m_player = {};
    m_worldMatrix = WorldMatrixInfo{};
    m_worldMatrix.SetPosition(10.f, WorldMatrixInfo::GROUND_HEIGHT, 0.f);  // 초기 위치 (식탁 윗면 높이)
    m_socket = 0;
    m_state = ST_FREE;
    m_prev_remain = 0;
    m_room_id = -1;
    m_lobby_player_id = -1;
    m_lastClientTimestamp = 0;
    m_lastServerTimestamp = 0;

    // 맵 이동 충돌용 sphere 초기화
    // 파라미터: Player_1rd::Ready_Components ~785-809 에서 발/몸통 sphere 와 동일
    //   발 sphere : localOffset.y=0.41, radius=0.41
    //   몸통 sphere: localOffset.y=1.23, radius=0.41
    const float px = 10.f, py = WorldMatrixInfo::GROUND_HEIGHT, pz = 0.f;
    m_mapSpheres[0] = SServerCollider::FromSphere({ px, py + 0.41f, pz }, 0.41f);
    m_mapSpheres[1] = SServerCollider::FromSphere({ px, py + 1.23f, pz }, 0.41f);
}

void GameSession::UpdateColliderPositions()
{
    // m_worldMatrix.m[12..14] = 플레이어 월드 발 위치
    // 발 sphere: y + 0.41, 몸통 sphere: y + 1.23
    const float px = m_worldMatrix.m[12];
    const float py = m_worldMatrix.m[13];
    const float pz = m_worldMatrix.m[14];
    m_mapSpheres[0].sphere.Center = { px, py + 0.41f, pz };
    m_mapSpheres[1].sphere.Center = { px, py + 1.23f, pz };
}

unsigned int GameSession::GetServerTimestamp()
{
    auto now = steady_clock::now();
    return static_cast<unsigned int>(
        duration_cast<milliseconds>(now.time_since_epoch()).count());
}

void GameSession::UpdateTimestamp(unsigned int clientTimestamp)
{
    m_lastClientTimestamp = clientTimestamp;
    m_lastServerTimestamp = GetServerTimestamp();
}

void GameSession::Recv()
{
    DWORD recv_flag = 0;
    memset(&m_recv_over.m_over, 0, sizeof(m_recv_over.m_over));
    m_recv_over.m_wsabuf.len = BUF_SIZE - m_prev_remain;
    m_recv_over.m_wsabuf.buf = m_recv_over.m_send_buf + m_prev_remain;

    WSARecv(m_socket, &m_recv_over.m_wsabuf, 1, 0, &recv_flag,
        &m_recv_over.m_over, 0);
}

void GameSession::Send(void* packet)
{
    OverllapedEXP* sdata = new OverllapedEXP{ reinterpret_cast<char*>(packet) };
    WSASend(m_socket, &sdata->m_wsabuf, 1, 0, 0, &sdata->m_over, 0);
}

void GameSession::Send_Add_Player_Packet(int c_id, GameSessionManager* gsm)
{
    auto& target = gsm->GetClient(c_id);
    SC_ADD_PLAYER_PACKET p{};
    p.size = sizeof(SC_ADD_PLAYER_PACKET);
    p.type = SC_ADD_PLAYER;
    p.id = target.m_lobby_player_id;  // 로비 ID 기준으로 통일
    memcpy(p.worldMatrix, target.m_worldMatrix.m, sizeof(float) * 16);
    strcpy_s(p.name, target.m_player.name);
    Send(&p);
}

void GameSession::Send_Remove_Player_Packet(int c_id, GameSessionManager* gsm)
{
    auto& target = gsm->GetClient(c_id);
    SC_REMOVE_PLAYER_PACKET p;
    p.size = sizeof(SC_REMOVE_PLAYER_PACKET);
    p.type = SC_REMOVE_PLAYER;
    p.id = target.m_lobby_player_id;  // 로비 ID 기준으로 통일
    Send(&p);
}

void GameSession::Send_Move_Packet(int c_id, GameSessionManager* gsm)
{
    auto& target = gsm->GetClient(c_id);
    SC_MOVE_PLAYER_PACKET p;
    p.size = sizeof(SC_MOVE_PLAYER_PACKET);
    p.type = SC_MOVE_PLAYER;
    p.id = target.m_lobby_player_id;  // 로비 ID 기준으로 통일
    p.keyInput = target.m_player.keyInput;
    p.timestamp = target.m_lastServerTimestamp;
    memcpy(p.worldMatrix, target.m_worldMatrix.m, sizeof(float) * 16);
    Send(&p);
}