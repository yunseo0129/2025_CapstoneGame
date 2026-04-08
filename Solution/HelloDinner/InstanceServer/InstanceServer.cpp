#include "InstanceServer.h"

InstanceServer::~InstanceServer()
{
    if (m_listen_socket != INVALID_SOCKET)
        closesocket(m_listen_socket);
    if (m_lobby_socket != INVALID_SOCKET)
        closesocket(m_lobby_socket);
    WSACleanup();
}

bool InstanceServer::Initialize(int instance_id, unsigned short port, const char* lobby_ip)
{
    m_instance_id = instance_id;
    m_port = port;

    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 2), &WSAData);

    // 자신의 IP 조회
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    struct addrinfo hints{}, * info = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hostname, nullptr, &hints, &info) == 0 && info) {
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(info->ai_addr);
        inet_ntop(AF_INET, &addr->sin_addr, m_my_ip, sizeof(m_my_ip));
        freeaddrinfo(info);
    }
    else {
        strcpy_s(m_my_ip, "127.0.0.1");
    }

    // 클라이언트용 리스닝 소켓
    m_listen_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (m_listen_socket == INVALID_SOCKET) {
        cout << "[Instance #" << m_instance_id << "] Socket creation failed.\n";
        return false;
    }

    SOCKADDR_IN server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(m_port);
    server_addr.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(m_listen_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        cout << "[Instance #" << m_instance_id << "] Bind failed.\n";
        return false;
    }

    if (listen(m_listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        cout << "[Instance #" << m_instance_id << "] Listen failed.\n";
        return false;
    }

    m_h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
    CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_listen_socket), m_h_iocp, 9999, 0);

    if (!m_h_iocp) {
        cout << "[Instance #" << m_instance_id << "] IOCP creation failed.\n";
        return false;
    }

    // 로비 서버에 접속 및 등록
    if (!ConnectToLobby(lobby_ip)) {
        cout << "[Instance #" << m_instance_id << "] Failed to connect to Lobby.\n";
        return false;
    }

    cout << "[Instance #" << m_instance_id << "] Running on port " << m_port << endl;
    PrintServerIP();

    return true;
}

bool InstanceServer::ConnectToLobby(const char* lobby_ip)
{
    m_lobby_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_lobby_socket == INVALID_SOCKET) return false;

    SOCKADDR_IN lobby_addr;
    memset(&lobby_addr, 0, sizeof(lobby_addr));
    lobby_addr.sin_family = AF_INET;
    lobby_addr.sin_port = htons(INTERNAL_PORT);
    inet_pton(AF_INET, lobby_ip, &lobby_addr.sin_addr);

    if (connect(m_lobby_socket, reinterpret_cast<sockaddr*>(&lobby_addr), sizeof(lobby_addr)) == SOCKET_ERROR) {
        cout << "[Instance #" << m_instance_id << "] Connect to lobby failed.\n";
        closesocket(m_lobby_socket);
        m_lobby_socket = INVALID_SOCKET;
        return false;
    }

    // IS_REGISTER 패킷 전송
    IS_REGISTER_PACKET rp;
    rp.size = sizeof(IS_REGISTER_PACKET);
    rp.type = IS_REGISTER;
    rp.instance_id = m_instance_id;
    strcpy_s(rp.ip, m_my_ip);
    rp.port = m_port;

    send(m_lobby_socket, reinterpret_cast<char*>(&rp), rp.size, 0);

    cout << "[Instance #" << m_instance_id << "] Registered with Lobby ("
         << lobby_ip << ":" << INTERNAL_PORT << ")" << endl;
    return true;
}

void InstanceServer::Run()
{
    // Heartbeat 스레드
    thread hb_th(&InstanceServer::HeartbeatThread, this);
    hb_th.detach();

    // 로비 수신 스레드 (방 정보 수신)
    thread lobby_recv_th(&InstanceServer::LobbyRecvThread, this);
    lobby_recv_th.detach();

    // 클라이언트 AcceptEx 시작
    AcceptClient();

    // IOCP 워커 스레드
    vector<thread> worker_threads;
    int num_threads = std::thread::hardware_concurrency();
    for (int i = 0; i < num_threads; ++i)
        worker_threads.emplace_back(&InstanceServer::WorkerThread, this);
    for (auto& th : worker_threads)
        th.join();
}

void InstanceServer::HeartbeatThread()
{
    auto* gsm = GameSessionManager::GetInstance();

    while (true) {
        this_thread::sleep_for(chrono::seconds(3));

        if (m_lobby_socket == INVALID_SOCKET) continue;

        IS_HEARTBEAT_PACKET hb;
        hb.size = sizeof(IS_HEARTBEAT_PACKET);
        hb.type = IS_HEARTBEAT;
        hb.instance_id = m_instance_id;
        hb.current_rooms = gsm->GetActiveRoomCount();
        hb.current_players = gsm->GetActivePlayerCount();
        hb.cpu_usage = 0.f;  // TODO: 실제 CPU 사용률 측정

        int sent = send(m_lobby_socket, reinterpret_cast<char*>(&hb), hb.size, 0);
        if (sent == SOCKET_ERROR) {
            cout << "[Instance #" << m_instance_id << "] Heartbeat send failed.\n";
        }
    }
}

void InstanceServer::LobbyRecvThread()
{
    char buf[BUF_SIZE] = {};

    while (true) {
        if (m_lobby_socket == INVALID_SOCKET) {
            this_thread::sleep_for(chrono::seconds(1));
            continue;
        }

        int received = recv(m_lobby_socket, buf, BUF_SIZE, 0);
        if (received <= 0) {
            cout << "[Instance #" << m_instance_id << "] Lobby connection lost.\n";
            m_lobby_socket = INVALID_SOCKET;
            continue;
        }

        char* p = buf;
        int remain = received;
        while (remain > 0) {
            int pkt_size = static_cast<unsigned char>(p[0]);
            if (pkt_size > remain) break;

            if (p[1] == IS_ROOM_NOTIFY) {
                IS_ROOM_NOTIFY_PACKET* rn = reinterpret_cast<IS_ROOM_NOTIFY_PACKET*>(p);
                GameSessionManager::GetInstance()->RegisterPendingRoom(*rn);
            }

            p += pkt_size;
            remain -= pkt_size;
        }
    }
}

void InstanceServer::AcceptClient()
{
    m_client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    m_accept_over.m_comp_type = OP_ACCEPT;
    ZeroMemory(&m_accept_over.m_over, sizeof(m_accept_over.m_over));
    int addr_size = sizeof(SOCKADDR_IN);
    AcceptEx(m_listen_socket, m_client_socket, m_accept_over.m_send_buf,
        0, addr_size + 16, addr_size + 16, 0, &m_accept_over.m_over);
}

void InstanceServer::WorkerThread()
{
    auto* gsm = GameSessionManager::GetInstance();

    while (true) {
        DWORD num_bytes;
        ULONG_PTR key;
        WSAOVERLAPPED* over = nullptr;
        BOOL ret = GetQueuedCompletionStatus(m_h_iocp, &num_bytes, &key, &over, INFINITE);
        OverllapedEXP* ex_over = reinterpret_cast<OverllapedEXP*>(over);

        if (FALSE == ret) {
            if (ex_over->m_comp_type == OP_ACCEPT) cout << "Accept Error";
            else {
                cout << "[Instance] GQCS Error on client[" << key << "]\n";
                gsm->Disconnect(static_cast<int>(key));
                if (ex_over->m_comp_type == OP_SEND) delete ex_over;
                continue;
            }
        }

        if ((0 == num_bytes) && ((ex_over->m_comp_type == OP_RECV) || (ex_over->m_comp_type == OP_SEND))) {
            gsm->Disconnect(static_cast<int>(key));
            if (ex_over->m_comp_type == OP_SEND) delete ex_over;
            continue;
        }

        switch (ex_over->m_comp_type) {
        case OP_ACCEPT: {
            int client_id = gsm->GetNewClientId();
            if (client_id != -1) {
                auto& client = gsm->GetClient(client_id);
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
                cout << "[Instance] Max user exceeded.\n";
            }
            ZeroMemory(&m_accept_over.m_over, sizeof(m_accept_over.m_over));
            int addr_size = sizeof(SOCKADDR_IN);
            AcceptEx(m_listen_socket, m_client_socket, m_accept_over.m_send_buf,
                0, addr_size + 16, addr_size + 16, 0, &m_accept_over.m_over);
            break;
        }
        case OP_RECV: {
            auto& client = gsm->GetClient(static_cast<int>(key));
            int remain_data = num_bytes + client.m_prev_remain;
            char* p = ex_over->m_send_buf;
            while (remain_data > 0) {
                int packet_size = p[0];
                if (packet_size <= remain_data) {
                    gsm->ProcessPacket(static_cast<int>(key), p);
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

void InstanceServer::PrintServerIP()
{
    cout << "[Instance #" << m_instance_id << "] IP: " << m_my_ip << endl;
}