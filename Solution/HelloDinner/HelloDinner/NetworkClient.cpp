#include "NetworkClient.h"
#include <iostream>
#include <cstring>

NetworkClient* NetworkClient::s_pInstance = nullptr;

NetworkClient* NetworkClient::GetInstance()
{
    if (!s_pInstance)
        s_pInstance = new NetworkClient();
    return s_pInstance;
}

void NetworkClient::DestroyInstance()
{
    if (s_pInstance) {
        s_pInstance->Disconnect();
        delete s_pInstance;
        s_pInstance = nullptr;
    }
}

NetworkClient::~NetworkClient()
{
    Disconnect();
}

void NetworkClient::CloseConsole()
{
    fclose(stdin);
    fclose(stdout);
    FreeConsole();
}

bool NetworkClient::ConnectWithConsole()
{
    AllocConsole();
    FILE* fp = nullptr;
    freopen_s(&fp, "CONIN$", "r", stdin);
    freopen_s(&fp, "CONOUT$", "w", stdout);

    std::cout << "========================================\n";
    std::cout << "       HelloDinner - Connect to Server\n";
    std::cout << "========================================\n\n";

    char serverIP[64] = {};
    std::cout << "Server IP: ";
    std::cin.getline(serverIP, sizeof(serverIP));

    std::cout << "Name: ";
    std::cin.getline(m_szName, NAME_SIZE);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "WSAStartup failed.\n";
        CloseConsole();
        return false;
    }

    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == INVALID_SOCKET) {
        std::cout << "Socket creation failed.\n";
        CloseConsole();
        return false;
    }

    SOCKADDR_IN serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(LOBBY_PORT);
    inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);

    std::cout << "Connecting to " << serverIP << ":" << LOBBY_PORT << "...\n";

    if (connect(m_socket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "Connection failed. Error: " << WSAGetLastError() << "\n";
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        CloseConsole();
        return false;
    }

    std::cout << "Connected! Sending login...\n";

    CS_LOGIN_PACKET packet{};
    packet.size = sizeof(CS_LOGIN_PACKET);
    packet.type = CS_LOGIN;
    strcpy_s(packet.name, m_szName);
    Send(&packet, packet.size);
    m_bConnected = true;

    m_recvThread = std::thread(&NetworkClient::RecvThread, this);

    std::cout << "Login sent. Waiting for response...\n";
    return true;
}

// ─────────────────────────────────────────────
// 인스턴스 서버 TCP 연결 (SC_REDIRECT 수신 후 호출)
// ─────────────────────────────────────────────
bool NetworkClient::ConnectToInstance(const char* ip, unsigned short port,
                                      int room_id, const char* auth_token)
{
    m_instanceSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_instanceSocket == INVALID_SOCKET) {
        std::cout << "[Instance] Socket creation failed.\n";
        return false;
    }

    SOCKADDR_IN addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(m_instanceSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cout << "[Instance] Connect failed. Error: " << WSAGetLastError() << "\n";
        closesocket(m_instanceSocket);
        m_instanceSocket = INVALID_SOCKET;
        return false;
    }

    // CS_JOIN_ROOM 패킷 전송
    CS_JOIN_ROOM_PACKET jp{};
    jp.size      = sizeof(CS_JOIN_ROOM_PACKET);
    jp.type      = CS_JOIN_ROOM;
    jp.room_id   = room_id;
    jp.player_id = m_iMyId;
    strcpy_s(jp.name,       m_szName);
    strcpy_s(jp.auth_token, auth_token);
    SendToInstance(&jp, jp.size);

    m_bInGame = true;

    // 인스턴스 수신 스레드 시작
    m_instanceRecvThread = std::thread(&NetworkClient::InstanceRecvThread, this);

    std::cout << "[Instance] Connected to " << ip << ":" << port
              << "  Room " << room_id << "\n";
    return true;
}

void NetworkClient::Send(void* packet, int size)
{
    if (m_socket == INVALID_SOCKET) return;
    ::send(m_socket, reinterpret_cast<char*>(packet), size, 0);
}

void NetworkClient::SendToInstance(void* packet, int size)
{
    if (m_instanceSocket == INVALID_SOCKET) return;
    ::send(m_instanceSocket, reinterpret_cast<char*>(packet), size, 0);
}

// ─────────────────────────────────────────────
// 수동 방 관련 송신 (로비 소켓)
// ─────────────────────────────────────────────
void NetworkClient::Send_CreateRoom()
{
    CS_CREATE_ROOM_PACKET p{};
    p.size = sizeof(CS_CREATE_ROOM_PACKET);
    p.type = CS_CREATE_ROOM;
    Send(&p, p.size);
}

void NetworkClient::Send_JoinRoomCode(int code)
{
    CS_JOIN_ROOM_CODE_PACKET p{};
    p.size = sizeof(CS_JOIN_ROOM_CODE_PACKET);
    p.type = CS_JOIN_ROOM_CODE;
    p.code = code;
    Send(&p, p.size);
    std::lock_guard<std::mutex> lk(m_roomLock);
    m_roomJoinPending = true;
    m_roomJoinResult  = 0;
}

void NetworkClient::Send_StartGame()
{
    CS_START_GAME_PACKET p{};
    p.size = sizeof(CS_START_GAME_PACKET);
    p.type = CS_START_GAME;
    Send(&p, p.size);
}

void NetworkClient::Send_LeaveRoom()
{
    CS_LEAVE_ROOM_PACKET p{};
    p.size = sizeof(CS_LEAVE_ROOM_PACKET);
    p.type = CS_LEAVE_ROOM;
    Send(&p, p.size);
    std::lock_guard<std::mutex> lk(m_roomLock);
    m_roomCode    = 0;
    m_roomHostId  = -1;
    m_roomMembers.clear();
}

void NetworkClient::Send_QuickMatch()
{
    CS_QUICK_MATCH_PACKET p{};
    p.size = sizeof(CS_QUICK_MATCH_PACKET);
    p.type = CS_QUICK_MATCH;
    Send(&p, p.size);
}

void NetworkClient::Send_SelectSeat(unsigned char team, unsigned char slot)
{
    CS_SELECT_SEAT_PACKET p{};
    p.size = sizeof(CS_SELECT_SEAT_PACKET);
    p.type = CS_SELECT_SEAT;
    p.team = team;
    p.slot = slot;
    Send(&p, p.size);
}

void NetworkClient::Send_PlayerReady(bool ready)
{
    CS_PLAYER_READY_PACKET p{};
    p.size  = sizeof(CS_PLAYER_READY_PACKET);
    p.type  = CS_PLAYER_READY;
    p.ready = ready ? 1 : 0;
    Send(&p, p.size);
}

NetworkClient::RoomSnapshot NetworkClient::GetRoomSnapshot()
{
    std::lock_guard<std::mutex> lk(m_roomLock);
    RoomSnapshot snap;
    snap.code         = m_roomCode;
    snap.host_id      = m_roomHostId;
    snap.members      = m_roomMembers;
    snap.join_pending = m_roomJoinPending;
    snap.join_result  = m_roomJoinResult;
    return snap;
}

// ─────────────────────────────────────────────
// 캐릭터 선택 완료 → 인스턴스 서버로 전송 (Ready 클릭 시 1회)
// ─────────────────────────────────────────────
void NetworkClient::Send_CharSelect(unsigned char charType)
{
    if (m_bOfflineMode || m_instanceSocket == INVALID_SOCKET) return;

    CS_CHAR_SELECT_PACKET p{};
    p.size      = sizeof(CS_CHAR_SELECT_PACKET);
    p.type      = CS_CHAR_SELECT;
    p.char_type = charType;
    SendToInstance(&p, p.size);
}

// ─────────────────────────────────────────────
// 이동 패킷 → 인스턴스 서버로 전송
// ─────────────────────────────────────────────
void NetworkClient::Send_Move(unsigned short keyInput, float mouseYaw, const float* worldMatrix)
{
    if (!m_bInGame || m_instanceSocket == INVALID_SOCKET) return;

    CS_MOVE_PACKET p{};
    p.size     = sizeof(CS_MOVE_PACKET);
    p.type     = CS_MOVE;
    p.keyInput = keyInput;
    p.mouseYaw = mouseYaw;
    auto now   = std::chrono::steady_clock::now();
    p.timestamp = static_cast<unsigned int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    memcpy(p.worldMatrix, worldMatrix, sizeof(float) * 16);

    SendToInstance(&p, p.size);  // ← 로비가 아닌 인스턴스 서버로
}

// ─────────────────────────────────────────────
// 로비 수신 스레드 (TCP)
// ─────────────────────────────────────────────
void NetworkClient::RecvThread()
{
    char recvBuf[BUF_SIZE] = {};
    int prevRemain = 0;

    while (m_bConnected) {
        int recvLen = recv(m_socket, recvBuf + prevRemain, BUF_SIZE - prevRemain, 0);
        if (recvLen <= 0) {
            m_bConnected = false;
            break;
        }

        int totalData = recvLen + prevRemain;
        char* p = recvBuf;

        while (totalData > 0) {
            int packetSize = static_cast<unsigned char>(p[0]);
            if (packetSize > totalData) break;
            ProcessLobbyPacket(p);
            p += packetSize;
            totalData -= packetSize;
        }

        prevRemain = totalData;
        if (prevRemain > 0)
            memmove(recvBuf, p, prevRemain);
    }
}

// ─────────────────────────────────────────────
// 인스턴스 수신 스레드 (TCP)
// ─────────────────────────────────────────────
void NetworkClient::InstanceRecvThread()
{
    char recvBuf[BUF_SIZE] = {};
    int prevRemain = 0;

    while (m_bInGame) {
        int recvLen = recv(m_instanceSocket, recvBuf + prevRemain, BUF_SIZE - prevRemain, 0);
        if (recvLen <= 0) {
            std::cout << "[Instance] Connection lost.\n";
            m_bInGame = false;
            break;
        }

        int totalData = recvLen + prevRemain;
        char* p = recvBuf;

        while (totalData > 0) {
            int packetSize = static_cast<unsigned char>(p[0]);
            if (packetSize > totalData) break;
            ProcessInstancePacket(p);
            p += packetSize;
            totalData -= packetSize;
        }

        prevRemain = totalData;
        if (prevRemain > 0)
            memmove(recvBuf, p, prevRemain);
    }
}

// ─────────────────────────────────────────────
// 로비 패킷 처리 (SC_REDIRECT 포함)
// ─────────────────────────────────────────────
void NetworkClient::ProcessLobbyPacket(char* packet)
{
    switch (packet[1]) {
    case SC_LOGIN_INFO: {
        SC_LOGIN_INFO_PACKET* p = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(packet);
        m_iMyId = p->id;
        memcpy(m_worldMatrix, p->worldMatrix, sizeof(float) * 16);
        m_players[m_iMyId].OnAdded(m_iMyId, p->worldMatrix, m_szName);
        {
            std::lock_guard<std::mutex> lk(m_activePlayerLock);
            m_activePlayerIds.insert(m_iMyId);
        }
        m_bLoggedIn = true;
        break;
    }
    case SC_MATCH_WAIT: {
        SC_MATCH_WAIT_PACKET* p = reinterpret_cast<SC_MATCH_WAIT_PACKET*>(packet);
        m_iQueueSize = p->queue_size;

        NetEvent evt{};
        evt.type      = NetEventType::MATCH_WAIT;
        evt.queueSize = p->queue_size;

        std::lock_guard<std::mutex> lk(m_matchEventLock);
        m_pendingMatchEvents.push_back(evt);
        break;
    }
    case SC_MATCH_SUCCESS: {
        SC_MATCH_SUCCESS_PACKET* p = reinterpret_cast<SC_MATCH_SUCCESS_PACKET*>(packet);
        m_iRoomId  = p->room_id;
        m_bMatched = true;

        NetEvent evt{};
        evt.type  = NetEventType::MATCH_SUCCESS;
        evt.roomId = p->room_id;
        memcpy(evt.playerIds, p->player_ids, sizeof(int) * ROOM_MAX_PLAYER);

        std::lock_guard<std::mutex> lk(m_matchEventLock);
        m_pendingMatchEvents.push_back(evt);
        break;
    }
    case SC_REDIRECT: {
        SC_REDIRECT_PACKET* p = reinterpret_cast<SC_REDIRECT_PACKET*>(packet);
        std::cout << "[Lobby] Redirect → Instance " << p->ip << ":" << p->port
                  << "  Room " << p->room_id << "\n";
        ConnectToInstance(p->ip, p->port, p->room_id, p->auth_token);
        {
            std::lock_guard<std::mutex> lk(m_roomLock);
            m_bGameStarting = true;
        }
        break;
    }
    case SC_ROOM_CREATED: {
        SC_ROOM_CREATED_PACKET* p = reinterpret_cast<SC_ROOM_CREATED_PACKET*>(packet);
        std::lock_guard<std::mutex> lk(m_roomLock);
        m_roomCode   = p->code;
        m_roomHostId = m_iMyId;
        m_roomMembers.clear();
        RoomMember me{};
        me.id = m_iMyId;
        strcpy_s(me.name, m_szName);
        m_roomMembers.push_back(me);
        std::cout << "[Lobby] Room created: " << p->code << "\n";
        break;
    }
    case SC_ROOM_JOIN_RESULT: {
        SC_ROOM_JOIN_RESULT_PACKET* p = reinterpret_cast<SC_ROOM_JOIN_RESULT_PACKET*>(packet);
        std::lock_guard<std::mutex> lk(m_roomLock);
        m_roomJoinResult  = p->result;
        m_roomJoinPending = false;
        if (p->result == RJR_OK)
            m_roomCode = p->code;
        std::cout << "[Lobby] Join result: " << (int)p->result << "\n";
        break;
    }
    case SC_ROOM_UPDATE: {
        SC_ROOM_UPDATE_PACKET* p = reinterpret_cast<SC_ROOM_UPDATE_PACKET*>(packet);
        std::lock_guard<std::mutex> lk(m_roomLock);
        m_roomCode    = p->code;
        m_roomHostId  = p->host_id;
        m_roomMembers.clear();
        for (int i = 0; i < (int)p->member_count && i < ROOM_MAX_PLAYER; ++i) {
            RoomMember m{};
            m.id    = p->member_ids[i];
            strcpy_s(m.name, p->member_names[i]);
            m.team  = p->member_teams[i];
            m.slot  = p->member_slots[i];
            m.ready = (p->member_ready[i] != 0);
            m_roomMembers.push_back(m);
        }
        break;
    }
    }
}

// ─────────────────────────────────────────────
// 인스턴스 패킷 처리 (SC_ADD_PLAYER / SC_MOVE_PLAYER 등)
// ─────────────────────────────────────────────
void NetworkClient::ProcessInstancePacket(char* packet)
{
    switch (packet[1]) {
    case SC_ADD_PLAYER: {
        SC_ADD_PLAYER_PACKET* p = reinterpret_cast<SC_ADD_PLAYER_PACKET*>(packet);
        m_players[p->id].OnAdded(p->id, p->worldMatrix, p->name);
        {
            std::lock_guard<std::mutex> lk(m_activePlayerLock);
            m_activePlayerIds.insert(p->id);
        }
        break;
    }
    case SC_REMOVE_PLAYER: {
        SC_REMOVE_PLAYER_PACKET* p = reinterpret_cast<SC_REMOVE_PLAYER_PACKET*>(packet);
        m_players[p->id].OnRemoved();
        {
            std::lock_guard<std::mutex> lk(m_activePlayerLock);
            m_activePlayerIds.erase(p->id);
        }
        break;
    }
    case SC_MOVE_PLAYER: {
        SC_MOVE_PLAYER_PACKET* p = reinterpret_cast<SC_MOVE_PLAYER_PACKET*>(packet);
        m_players[p->id].OnMoved(p->keyInput, p->worldMatrix);
        break;
    }
    // ── Phase 1: 게임 상태머신 패킷 ──────────────────────────────────
    case SC_PHASE_CHANGE: {
        SC_PHASE_CHANGE_PACKET* p = reinterpret_cast<SC_PHASE_CHANGE_PACKET*>(packet);
        NetEvent evt{};
        evt.type  = NetEventType::PHASE_CHANGE;
        evt.phase = p->phase;
        evt.round = p->round;
        std::lock_guard<std::mutex> lk(m_matchEventLock);
        m_pendingMatchEvents.push_back(evt);
        break;
    }
    case SC_ROUND_START: {
        SC_ROUND_START_PACKET* p = reinterpret_cast<SC_ROUND_START_PACKET*>(packet);
        NetEvent evt{};
        evt.type           = NetEventType::ROUND_START;
        evt.round          = p->round;
        evt.duration_ms    = p->duration_ms;
        evt.server_time_ms = p->server_time_ms;
        std::lock_guard<std::mutex> lk(m_matchEventLock);
        m_pendingMatchEvents.push_back(evt);
        break;
    }
    case SC_ROUND_END: {
        SC_ROUND_END_PACKET* p = reinterpret_cast<SC_ROUND_END_PACKET*>(packet);
        NetEvent evt{};
        evt.type        = NetEventType::ROUND_END;
        evt.winner_team = p->winner_team;
        evt.score_a     = p->score_a;
        evt.score_b     = p->score_b;
        std::lock_guard<std::mutex> lk(m_matchEventLock);
        m_pendingMatchEvents.push_back(evt);
        break;
    }
    case SC_SCORE_UPDATE: {
        SC_SCORE_UPDATE_PACKET* p = reinterpret_cast<SC_SCORE_UPDATE_PACKET*>(packet);
        NetEvent evt{};
        evt.type         = NetEventType::SCORE_UPDATE;
        evt.score_a      = p->score_a;
        evt.score_b      = p->score_b;
        evt.player_count = p->player_count;
        memcpy(evt.stats, p->stats, sizeof(PlayerStatBrief) * ROOM_MAX_PLAYER);
        std::lock_guard<std::mutex> lk(m_matchEventLock);
        m_pendingMatchEvents.push_back(evt);
        break;
    }
    }
}

void NetworkClient::PopAllPlayerEvents(std::vector<NetPlayer::Event>& outEvents)
{
    std::vector<int> ids;
    {
        std::lock_guard<std::mutex> lk(m_activePlayerLock);
        ids.assign(m_activePlayerIds.begin(), m_activePlayerIds.end());
    }
    for (int id : ids)
        m_players[id].PopEvents(outEvents);
}

void NetworkClient::PopAllMatchEvents(std::vector<NetEvent>& outEvents)
{
    std::lock_guard<std::mutex> lk(m_matchEventLock);
    outEvents.swap(m_pendingMatchEvents);
    m_pendingMatchEvents.clear();
}

void NetworkClient::Disconnect()
{
    m_bConnected = false;
    m_bLoggedIn  = false;
    m_bMatched   = false;
    m_bInGame    = false;
    m_iRoomId    = -1;
    m_iQueueSize = 0;

    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    if (m_instanceSocket != INVALID_SOCKET) {
        closesocket(m_instanceSocket);
        m_instanceSocket = INVALID_SOCKET;
    }

    if (m_recvThread.joinable())
        m_recvThread.join();
    if (m_instanceRecvThread.joinable())
        m_instanceRecvThread.join();
}