#include "InstanceServer.h"

// 로컬 머신의 실제 IPv4 주소를 조회하여 buf에 저장
bool GetLocalIP(char* buf, int buf_size)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return false;

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        WSACleanup();
        return false;
    }

    struct addrinfo hints{}, *info = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, nullptr, &hints, &info) != 0 || !info) {
        WSACleanup();
        return false;
    }

    sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(info->ai_addr);
    inet_ntop(AF_INET, &addr->sin_addr, buf, buf_size);
    freeaddrinfo(info);
    WSACleanup();
    return true;
}

int main(int argc, char* argv[])
{
    // 사용법: InstanceServer.exe <instance_id> <port> <lobby_ip>

	// 인스턴스 ID, 포트, 로비 IP는 명령행 인자로 전달받음 (기본값 존재)
    int instance_id = 0;
    unsigned short port = INSTANCE_PORT_BASE;

    // 기본 로비 IP: 같은 머신의 실제 IP를 자동 탐지
    char default_lobby_ip[16] = "127.0.0.1";
    GetLocalIP(default_lobby_ip, sizeof(default_lobby_ip));
    const char* lobby_ip = default_lobby_ip;

    if (argc >= 2) instance_id = atoi(argv[1]);
    if (argc >= 3) port = static_cast<unsigned short>(atoi(argv[2]));
    if (argc >= 4) lobby_ip = argv[3];

    cout << "========================================" << endl;
    cout << "     INSTANCE SERVER #" << instance_id << endl;
    cout << "========================================" << endl;
    cout << "     Lobby IP: " << lobby_ip << endl;

    InstanceServer server;

    if (!server.Initialize(instance_id, port, lobby_ip))
        return 1;

    server.Run();
}