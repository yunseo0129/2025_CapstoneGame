#include "LobbyServer.h"

LobbyServer::~LobbyServer()
{
    if (m_listen_socket != INVALID_SOCKET)
        closesocket(m_listen_socket);
    WSACleanup();
}

bool LobbyServer::Initialize()
{
    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 2), &WSAData);
    m_listen_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

    if (m_listen_socket == INVALID_SOCKET) {
        cout << "[Lobby] Socket creation failed.\n";
        return false;
    }

    SOCKADDR_IN server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(LOBBY_PORT);
    server_addr.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(m_listen_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        cout << "[Lobby] Bind failed.\n";
        return false;
    }

    if (listen(m_listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        cout << "[Lobby] Listen failed.\n";
        return false;
    }

    m_h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
    CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_listen_socket), m_h_iocp, 9999, 0);

    if (!m_h_iocp) {
        cout << "[Lobby] IOCP creation failed.\n";
        return false;
    }

    // 인스턴스 서버 내부 통신 리스너 시작
    if (!InstanceManager::GetInstance()->StartInternalListener()) {
        cout << "[Lobby] Internal listener failed.\n";
        return false;
    }

    cout << "[Lobby] Server is running on port " << LOBBY_PORT << endl;
    PrintServerIP();

    return true;
}

void LobbyServer::Run()
{
    // 인스턴스 서버 접속 수락 스레드 시작
    thread internal_th(&InstanceManager::InternalAcceptThread, InstanceManager::GetInstance());
    internal_th.detach();

    // 클라이언트 AcceptEx 시작
    AcceptClient();

    // IOCP 워커 스레드
    vector<thread> worker_threads;
    int num_threads = std::thread::hardware_concurrency();
    for (int i = 0; i < num_threads; ++i)
        worker_threads.emplace_back(&LobbyServer::WorkerThread, this);
    for (auto& th : worker_threads)
        th.join();
}

void LobbyServer::AcceptClient()
{
    m_client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    m_accept_over.m_comp_type = OP_ACCEPT;
    ZeroMemory(&m_accept_over.m_over, sizeof(m_accept_over.m_over));
    int addr_size = sizeof(SOCKADDR_IN);
    AcceptEx(m_listen_socket, m_client_socket, m_accept_over.m_send_buf,
        0, addr_size + 16, addr_size + 16, 0, &m_accept_over.m_over);
}

void LobbyServer::WorkerThread()
{
    auto* sm = SessionManager::GetInstance();

    while (true) {
        DWORD num_bytes;
        ULONG_PTR key;
        WSAOVERLAPPED* over = nullptr;
        BOOL ret = GetQueuedCompletionStatus(m_h_iocp, &num_bytes, &key, &over, INFINITE);
        OverllapedEXP* ex_over = reinterpret_cast<OverllapedEXP*>(over);

        if (FALSE == ret) {
            if (ex_over->m_comp_type == OP_ACCEPT) cout << "Accept Error";
            else {
                cout << "GQCS Error on client[" << key << "]\n";
                sm->Disconnect(static_cast<int>(key));
                if (ex_over->m_comp_type == OP_SEND) delete ex_over;
                continue;
            }
        }

        if ((0 == num_bytes) && ((ex_over->m_comp_type == OP_RECV) || (ex_over->m_comp_type == OP_SEND))) {
            sm->Disconnect(static_cast<int>(key));
            if (ex_over->m_comp_type == OP_SEND) delete ex_over;
            continue;
        }

        switch (ex_over->m_comp_type) {
        case OP_ACCEPT: {
            int client_id = sm->GetNewClientId();
            if (client_id != -1) {
                auto& client = sm->GetClient(client_id);
                {
                    lock_guard<mutex> ll(client.m_s_lock);
                    client.m_state = ST_ALLOC;
                }
                client.m_player.id = client_id;
                client.m_player.name[0] = 0;
                client.m_prev_remain = 0;
                client.m_socket = m_client_socket;
                CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_client_socket),
                    m_h_iocp, client_id, 0);
                client.Recv();
                m_client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
            }
            else {
                cout << "Max user exceeded.\n";
            }
            ZeroMemory(&m_accept_over.m_over, sizeof(m_accept_over.m_over));
            int addr_size = sizeof(SOCKADDR_IN);
            AcceptEx(m_listen_socket, m_client_socket, m_accept_over.m_send_buf,
                0, addr_size + 16, addr_size + 16, 0, &m_accept_over.m_over);
            break;
        }
        case OP_RECV: {
            // 로비에서는 CS_LOGIN, CS_LOGOUT만 처리 (CS_MOVE는 인스턴스에서 처리)
            auto& client = sm->GetClient(static_cast<int>(key));
            int remain_data = num_bytes + client.m_prev_remain;
            char* p = ex_over->m_send_buf;
            while (remain_data > 0) {
                int packet_size = p[0];
                if (packet_size <= remain_data) {
                    sm->ProcessPacket(static_cast<int>(key), p);
                    p = p + packet_size;
                    remain_data = remain_data - packet_size;
                }
                else break;
            }
            client.m_prev_remain = remain_data;
            if (remain_data > 0) {
                memcpy(ex_over->m_send_buf, p, remain_data);
            }
            client.Recv();
            break;
        }
        case OP_SEND:
            delete ex_over;
            break;
        }
    }
}

void LobbyServer::PrintServerIP()
{
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        cout << "gethostname failed.\n";
        return;
    }

    struct addrinfo hints{}, * info = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, nullptr, &hints, &info) != 0) {
        cout << "getaddrinfo failed.\n";
        return;
    }

    for (auto p = info; p != nullptr; p = p->ai_next) {
        char ip[INET_ADDRSTRLEN];
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(p->ai_addr);
        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
        cout << "[Lobby] Server IP: " << ip << endl;
    }

    freeaddrinfo(info);
}