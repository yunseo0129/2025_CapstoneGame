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

    // 서버 IP 입력
    char serverIP[64] = {};
    std::cout << "Server IP: ";
    std::cin.getline(serverIP, sizeof(serverIP));

    // 이름 입력
    std::cout << "Name: ";
    std::cin.getline(m_szName, NAME_SIZE);

    // Winsock 초기화
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "WSAStartup failed.\n";
        CloseConsole();
        return false;
    }

    // 소켓 생성
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == INVALID_SOCKET) {
        std::cout << "Socket creation failed.\n";
        CloseConsole();
        return false;
    }

    // 서버 연결
    SOCKADDR_IN serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(LOBBY_PORT);
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

    // 로그인 패킷 전송
    CS_LOGIN_PACKET packet{};
    packet.size = sizeof(CS_LOGIN_PACKET);
    packet.type = CS_LOGIN;
    strcpy_s(packet.name, m_szName);
    Send(&packet, packet.size);
    m_bConnected = true;

    // 수신 스레드 시작
    m_recvThread = std::thread(&NetworkClient::RecvThread, this);

    std::cout << "Login sent. Waiting for response...\n";

    return true;
}

void NetworkClient::Send(void* packet, int size)
{
    if (m_socket == INVALID_SOCKET) return;
    ::send(m_socket, reinterpret_cast<char*>(packet), size, 0);
}

void NetworkClient::Send_Move(unsigned char keyInput, float mouseYaw, const float* worldMatrix)
{
    CS_MOVE_PACKET p{};
    p.size = sizeof(CS_MOVE_PACKET);
    p.type = CS_MOVE;
    p.keyInput = keyInput;
    p.mouseYaw = mouseYaw;
    auto now = std::chrono::steady_clock::now();
    p.timestamp = static_cast<unsigned int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    memcpy(p.worldMatrix, worldMatrix, sizeof(float) * 16);
    Send(&p, p.size);
}

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

            ProcessPacket(p);
            p += packetSize;
            totalData -= packetSize;
        }

        prevRemain = totalData;
        if (prevRemain > 0)
            memmove(recvBuf, p, prevRemain);
    }
}

// 서버에서 받은 패킷을 처리
void NetworkClient::ProcessPacket(char* packet)
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
    case SC_MATCH_WAIT: {
        SC_MATCH_WAIT_PACKET* p = reinterpret_cast<SC_MATCH_WAIT_PACKET*>(packet);
        m_iQueueSize = p->queue_size;

        NetEvent evt{};
        evt.type = NetEventType::MATCH_WAIT;
        evt.queueSize = p->queue_size;

        std::lock_guard<std::mutex> lk(m_matchEventLock);
        m_pendingMatchEvents.push_back(evt);
        break;
    }
    case SC_MATCH_SUCCESS: {
        SC_MATCH_SUCCESS_PACKET* p = reinterpret_cast<SC_MATCH_SUCCESS_PACKET*>(packet);
        m_iRoomId = p->room_id;
        m_bMatched = true;

        NetEvent evt{};
        evt.type = NetEventType::MATCH_SUCCESS;
        evt.roomId = p->room_id;
        memcpy(evt.playerIds, p->player_ids, sizeof(int) * ROOM_MAX_PLAYER);

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
    for (int id : ids) {
        m_players[id].PopEvents(outEvents);
    }
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
    m_bLoggedIn = false;
    m_bMatched = false;
    m_iRoomId = -1;
    m_iQueueSize = 0;
    // 소켓 닫기 → RecvThread의 recv()가 에러 리턴하며 루프 탈출
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    // RecvThread가 완전히 종료될 때까지 대기
    if (m_recvThread.joinable())
        m_recvThread.join();
}