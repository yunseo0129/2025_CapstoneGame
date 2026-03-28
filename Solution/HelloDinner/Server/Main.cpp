#include "SessionManager.h"

SOCKET g_s_socket, g_c_socket;
OverllapedEXP g_a_over;

void WorkerThread(HANDLE h_iocp)
{
	auto* sm = SessionManager::GetInstance();

	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
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

		// 작업 완료된 IOCP 패킷의 종류에 따라 처리
		switch (ex_over->m_comp_type) {
		case OP_ACCEPT: {
			int client_id = sm->GetNewClientId();
			if (client_id != -1) {
				auto& client = sm->GetClient(client_id);
				{
					lock_guard<mutex> ll(client.m_s_lock);
					client.m_state = ST_ALLOC;
				}
				client.m_id = client_id;
				client.m_name[0] = 0;
				client.m_prev_remain = 0;
				client.m_socket = g_c_socket;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket),
					h_iocp, client_id, 0);
				client.Recv();
				g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else {
				cout << "Max user exceeded.\n";
			}
			ZeroMemory(&g_a_over.m_over, sizeof(g_a_over.m_over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(g_s_socket, g_c_socket, g_a_over.m_send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over.m_over);
			break;
		}
		case OP_RECV: {
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

void PrintServerIP()
{
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
		cout << "gethostname failed.\n";
		return;
	}

	struct addrinfo hints{}, *info = nullptr;
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
		cout << "Server IP: " << ip << endl;
	}

	freeaddrinfo(info);
}

int main()
{
	HANDLE h_iocp;

	// 윈속 초기화
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	
	if(!g_s_socket) {
		cout << "Socket creation failed.\n";
		return 1;
	}

	// 서버 소켓 주소 정보 설정
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	
	// 서버 소켓에 주소 정보 바인딩 및 리슨 상태로 전환
	if (bind(g_s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
		cout << "Bind failed.\n";
		return 1;
	}

	if (listen(g_s_socket, SOMAXCONN) == SOCKET_ERROR) {
		cout << "Listen failed.\n";
		return 1;
	}

	// 클라이언트 소켓 주소 정보 설정
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);

	// IOCP 생성 및 서버 소켓과 IOCP 연결
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_s_socket), h_iocp, 9999, 0);

	if(!h_iocp) {
		cout << "IOCP creation failed.\n";
		return 1;
	}

	cout << "Server is running on port " << PORT_NUM << endl;
	PrintServerIP();

	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over.m_comp_type = OP_ACCEPT;
	AcceptEx(g_s_socket, g_c_socket, g_a_over.m_send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over.m_over);

	vector<thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(WorkerThread, h_iocp);
	for (auto& th : worker_threads)
		th.join();
	closesocket(g_s_socket);
	WSACleanup();
}
