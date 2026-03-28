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
    // 표준 입출력 핸들 해제 후 콘솔 창 닫기
    fclose(stdin);
    fclose(stdout);
    FreeConsole();
}

bool NetworkClient::ConnectWithConsole()
{
    // 콘솔 창 생성
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
    serverAddr.sin_port = htons(PORT_NUM);
    inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);

    std::cout << "Connecting to " << serverIP << ":" << PORT_NUM << "...\n";

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
    m_recvThread.detach();

    std::cout << "Login sent. Waiting for response...\n";

    return true;
}

void NetworkClient::Send(void* packet, int size)
{
    if (m_socket == INVALID_SOCKET) return;
    ::send(m_socket, reinterpret_cast<char*>(packet), size, 0);
}

void NetworkClient::Send_Move(char direction)
{
    CS_MOVE_PACKET p{};
    p.size = sizeof(CS_MOVE_PACKET);
    p.type = CS_MOVE;
    p.direction = direction;
    p.move_time = 0;
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

void NetworkClient::ProcessPacket(char* packet)
{
    switch (packet[1]) {
    case SC_LOGIN_INFO: {
        SC_LOGIN_INFO_PACKET* p = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(packet);
        m_iMyId = p->id;
        m_sMyX = p->x;
        m_sMyY = p->y;
        m_sMyZ = p->z;
        
        std::lock_guard<std::mutex> lk(m_lock);
        m_players[m_iMyId].id = m_iMyId;
        m_players[m_iMyId].x = p->x;
        m_players[m_iMyId].y = p->y;
        m_players[m_iMyId].z = p->z;
        strcpy_s(m_players[m_iMyId].name, m_szName);
        m_players[m_iMyId].active = true;
        
        break;
    }
    case SC_ADD_PLAYER: {
        SC_ADD_PLAYER_PACKET* p = reinterpret_cast<SC_ADD_PLAYER_PACKET*>(packet);
        std::lock_guard<std::mutex> lk(m_lock);
        m_players[p->id].id = p->id;
        m_players[p->id].x = p->x;
        m_players[p->id].y = p->y;
        m_players[p->id].z = p->z;
        strcpy_s(m_players[p->id].name, p->name);
        m_players[p->id].active = true;
        break;
    }
    case SC_REMOVE_PLAYER: {
        SC_REMOVE_PLAYER_PACKET* p = reinterpret_cast<SC_REMOVE_PLAYER_PACKET*>(packet);
        std::lock_guard<std::mutex> lk(m_lock);
        m_players[p->id].active = false;
        break;
    }
    case SC_MOVE_PLAYER: {
        SC_MOVE_PLAYER_PACKET* p = reinterpret_cast<SC_MOVE_PLAYER_PACKET*>(packet);
        std::lock_guard<std::mutex> lk(m_lock);
        m_players[p->id].x = p->x;
        m_players[p->id].y = p->y;
        m_players[p->id].z = p->z;

        if (p->id == m_iMyId) {
            m_sMyX = p->x;
            m_sMyY = p->y;
            m_sMyZ = p->z;
        }
        break;
    }
    }
}

void NetworkClient::Disconnect()
{
    m_bConnected = false;
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}