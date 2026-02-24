#include "Session.h"

array<Session, MAX_USER> clients;
SOCKET g_s_socket, g_c_socket;
OverllapedEXP g_a_over;

int get_new_client_id()
{
	for (int i = 0; i < MAX_USER; ++i) {
		lock_guard <mutex> ll{ clients[i].m_s_lock };
		if (clients[i].m_state == ST_FREE)
			cout << i << "New Client Connected." << endl;

			return i;
	}
	return -1;
}

// 클라이언트로부터 받은 패킷을 처리하는 함수
void process_packet(int c_id, char* packet)
{
	switch (packet[1]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		strcpy_s(clients[c_id].m_name, p->name);
		clients[c_id].x = rand() % W_WIDTH;
		clients[c_id].y = rand() % W_HEIGHT;
		clients[c_id].Send_Login_Info_Packet();
		{
			lock_guard<mutex> ll{ clients[c_id].m_s_lock };
			clients[c_id].m_state = ST_INGAME;
		}
		for (auto& pl : clients) {
			{
				lock_guard<mutex> ll(pl.m_s_lock);
				if (ST_INGAME != pl.m_state) continue;
			}
			if (pl.m_id == c_id) continue;
			pl.Send_Add_Player_Packet(c_id);
			clients[c_id].Send_Add_Player_Packet(pl.m_id);
		}
		break;
	}
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
		clients[c_id].m_last_move_time = p->move_time;
		short x = clients[c_id].x;
		short y = clients[c_id].y;
		switch (p->direction) {
		case 0: if (y > 0) y--; break;
		case 1: if (y < W_HEIGHT - 1) y++; break;
		case 2: if (x > 0) x--; break;
		case 3: if (x < W_WIDTH - 1) x++; break;
		}
		clients[c_id].x = x;
		clients[c_id].y = y;

		for (auto& cl : clients) {
			if (cl.m_state != ST_INGAME) continue;
			cl.Send_Move_Packet(c_id);

		}
	}
	}
}

void disconnect(int c_id)
{
	for (auto& pl : clients) {
		{
			lock_guard<mutex> ll(pl.m_s_lock);
			if (ST_INGAME != pl.m_state) continue;
		}
		if (pl.m_id == c_id) continue;
		pl.Send_Remove_Player_Packet(c_id);
	}
	closesocket(clients[c_id].m_socket);

	lock_guard<mutex> ll(clients[c_id].m_s_lock);
	clients[c_id].m_state = ST_FREE;
}

// IOCP worker thread 함수
void worker_thread(HANDLE h_iocp)
{
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
				disconnect(static_cast<int>(key));
				if (ex_over->m_comp_type == OP_SEND) delete ex_over;
				continue;
			}
		}

		if ((0 == num_bytes) && ((ex_over->m_comp_type == OP_RECV) || (ex_over->m_comp_type == OP_SEND))) {
			disconnect(static_cast<int>(key));
			if (ex_over->m_comp_type == OP_SEND) delete ex_over;
			continue;
		}

		switch (ex_over->m_comp_type) {
		case OP_ACCEPT: {
			int client_id = get_new_client_id();
			if (client_id != -1) {
				{
					lock_guard<mutex> ll(clients[client_id].m_s_lock);
					clients[client_id].m_state = ST_ALLOC;
				}
				clients[client_id].x = 0;
				clients[client_id].y = 0;
				clients[client_id].m_id = client_id;
				clients[client_id].m_name[0] = 0;
				clients[client_id].m_prev_remain = 0;
				clients[client_id].m_socket = g_c_socket;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket),
					h_iocp, client_id, 0);
				clients[client_id].Recv();
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
			int remain_data = num_bytes + clients[key].m_prev_remain;
			char* p = ex_over->m_send_buf;
			while (remain_data > 0) {
				int packet_size = p[0];
				if (packet_size <= remain_data) {
					process_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}
			clients[key].m_prev_remain = remain_data;
			if (remain_data > 0) {
				memcpy(ex_over->m_send_buf, p, remain_data);
			}
			clients[key].Recv();
			break;
		}
		case OP_SEND:
			delete ex_over;
			break;
		}
	}
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

	// 클라이언트 연결 수락 및 IOCP 작업 등록
	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over.m_comp_type = OP_ACCEPT;
	AcceptEx(g_s_socket, g_c_socket, g_a_over.m_send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over.m_over);

	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);
	for (auto& th : worker_threads)
		th.join();
	closesocket(g_s_socket);
	WSACleanup();
}
