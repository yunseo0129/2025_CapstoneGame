#include "InstanceManager.h"

bool InstanceManager::StartInternalListener()
{
    m_internal_listen = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (m_internal_listen == INVALID_SOCKET) {
        cout << "[InstanceManager] Internal socket creation failed.\n";
        return false;
    }

    SOCKADDR_IN addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(INTERNAL_PORT);
    addr.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(m_internal_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        cout << "[InstanceManager] Internal bind failed.\n";
        return false;
    }

    if (listen(m_internal_listen, MAX_INSTANCE_SERVERS) == SOCKET_ERROR) {
        cout << "[InstanceManager] Internal listen failed.\n";
        return false;
    }

    cout << "[InstanceManager] Internal listener started on port " << INTERNAL_PORT << endl;
    return true;
}

void InstanceManager::InternalAcceptThread()
{
    while (true) {
        SOCKADDR_IN client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client_sock = accept(m_internal_listen,
            reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

        if (client_sock == INVALID_SOCKET) {
            cout << "[InstanceManager] Internal accept failed.\n";
            continue;
        }

        // 첫 패킷으로 IS_REGISTER_PACKET 수신
        char buf[BUF_SIZE] = {};
        int received = recv(client_sock, buf, BUF_SIZE, 0);
        if (received <= 0) {
            closesocket(client_sock);
            continue;
        }

        if (buf[1] != IS_REGISTER) {
            cout << "[InstanceManager] Expected IS_REGISTER packet.\n";
            closesocket(client_sock);
            continue;
        }

        IS_REGISTER_PACKET* rp = reinterpret_cast<IS_REGISTER_PACKET*>(buf);
        RegisterInstance(rp->instance_id, rp->ip, rp->port, client_sock);

        cout << "[InstanceManager] Instance #" << rp->instance_id
            << " registered (" << rp->ip << ":" << rp->port << ")" << endl;

        // 해당 인스턴스 전용 수신 스레드 시작
        thread recv_th(&InstanceManager::InternalRecvThread, this, rp->instance_id);
        recv_th.detach();
    }
}

void InstanceManager::InternalRecvThread(int inst_id)
{
    InstanceInfo* inst = GetInstanceInfo(inst_id);
    if (!inst || inst->socket == INVALID_SOCKET) return;

    char buf[BUF_SIZE] = {};
    int prevRemain = 0;
    while (true) {
        int received = recv(inst->socket, buf + prevRemain, BUF_SIZE - prevRemain, 0);
        if (received <= 0) {
            cout << "[InstanceManager] Instance #" << inst_id << " disconnected.\n";
            lock_guard<mutex> ll(m_lock);
            inst->alive = false;
            inst->socket = INVALID_SOCKET;
            return;
        }

        // 패킷 처리
        char* p = buf;
        int remain = received + prevRemain;
        while (remain > 0) {
            int pkt_size = static_cast<unsigned char>(p[0]);
            if (pkt_size < 2 || pkt_size > remain) break;

            if (p[1] == IS_HEARTBEAT) {
                IS_HEARTBEAT_PACKET* hb = reinterpret_cast<IS_HEARTBEAT_PACKET*>(p);
                UpdateHeartbeat(*hb);
            }

            p += pkt_size;
            remain -= pkt_size;
        }

        prevRemain = remain;
        if (prevRemain > 0)
            memmove(buf, p, prevRemain);
    }
}

InstanceInfo* InstanceManager::SelectBestInstance()
{
    lock_guard<mutex> ll(m_lock);

    InstanceInfo* best = nullptr;
    float bestScore = FLT_MAX;

    for (int i = 0; i < MAX_INSTANCE_SERVERS; ++i) {
        auto& inst = m_instances[i];
        if (!inst.alive) continue;

        // Heartbeat 타임아웃 체크 (10초)
        auto elapsed = duration_cast<seconds>(
            steady_clock::now() - inst.last_heartbeat).count();
        if (elapsed > 10) {
            inst.alive = false;
            continue;
        }

        float score = inst.GetLoadScore();
        if (score < bestScore) {
            bestScore = score;
            best = &inst;
        }
    }
    return best;
}

void InstanceManager::RegisterInstance(int id, const char* ip, unsigned short port, SOCKET sock)
{
    lock_guard<mutex> ll(m_lock);
    if (id < 0 || id >= MAX_INSTANCE_SERVERS) return;

    auto& inst = m_instances[id];
    inst.id = id;
    strcpy_s(inst.ip, sizeof(inst.ip), ip);
    inst.port = port;
    inst.socket = sock;
    inst.alive = true;
    inst.last_heartbeat = steady_clock::now();
}

void InstanceManager::UpdateHeartbeat(const IS_HEARTBEAT_PACKET& pkt)
{
    lock_guard<mutex> ll(m_lock);
    int id = pkt.instance_id;
    if (id < 0 || id >= MAX_INSTANCE_SERVERS) return;

    auto& inst = m_instances[id];
    inst.current_rooms = pkt.current_rooms;
    inst.current_players = pkt.current_players;
    inst.cpu_usage = pkt.cpu_usage;
    inst.last_heartbeat = steady_clock::now();
    inst.alive = true;
}

void InstanceManager::NotifyRoomToInstance(InstanceInfo* inst, const IS_ROOM_NOTIFY_PACKET& pkt)
{
    if (!inst || inst->socket == INVALID_SOCKET) return;
    send(inst->socket, reinterpret_cast<const char*>(&pkt), pkt.size, 0);
}

InstanceInfo* InstanceManager::GetInstanceInfo(int id)
{
    lock_guard<mutex> ll(m_lock);
    if (id < 0 || id >= MAX_INSTANCE_SERVERS) return nullptr;
    return &m_instances[id];
}