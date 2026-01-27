#include <iostream>
#include <array>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <random>
#include <unordered_set>
#include <unordered_map>
#include "protocol.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
using namespace std;

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND };

std::uniform_int_distribution<int> x_uid(0, W_WIDTH);
std::uniform_int_distribution<int> y_uid(0, W_HEIGHT);

constexpr int VIEW_RANGE = 10;	

constexpr int SECTOR_ROW = 20;
constexpr int SECTOR_COL = 20;

pair<int, int> near_sector[8];

void initialize_near_sector()
{
	near_sector[0] = { -1, -1 };
	near_sector[1] = { -1, 0 };
	near_sector[2] = { -1, 1 };
	near_sector[3] = { 0, -1 };
	near_sector[4] = { 0, 1 };
	near_sector[5] = { 1, -1 };
	near_sector[6] = { 1, 0 };
	near_sector[7] = { 1, 1 };
}

// 해시 함수
struct pair_hash {
	template <class T1, class T2>
	std::size_t operator () (const std::pair<T1, T2>& pair) const {
		auto hash1 = std::hash<T1>{}(pair.first);
		auto hash2 = std::hash<T2>{}(pair.second);
		return hash1 ^ hash2;  // XOR 연산을 사용하여 두 해시 값을 결합
	}
};

class SECTOR {
public:
	unordered_set<int> _player_list;
	mutex	_pl_l;


	SECTOR() {}

	SECTOR(const SECTOR& other) 
	{
		// _player_list 멤버를 복사합니다.
		_player_list = other._player_list;

	}

	SECTOR& operator=(const SECTOR& other) 
	{
		if (this != &other) 
		{ 
			_player_list = other._player_list;
		}
		return *this;
	}

	void add_player_list(int c_id)
	{
		_pl_l.lock();
		_player_list.insert(c_id);
		_pl_l.unlock();
	}

	void remove_player_list(int c_id)
	{
		// 플레이어가 없을 경우 return
		if (0 == _player_list.count(c_id)) return;

		_pl_l.lock();
		_player_list.erase(c_id);
		_pl_l.unlock();
	}

};

// sector 행, 열 번호와 SECTOR를 mapping하는 자료형
unordered_map<pair<int, int>, SECTOR, pair_hash> g_sectors;


void initialize_sector()
{
	int row = static_cast<int>(W_WIDTH / SECTOR_ROW);
	int col = static_cast<int>(W_HEIGHT / SECTOR_COL);

	pair<int, int> sector_id;

	for (int i = 0; i < row; ++i)
	{
		for (int j = 0; j < col; ++j)
		{
			sector_id = { i, j };
			SECTOR sector;
			g_sectors[sector_id] = sector;
		}
	}
}

class OVER_EXP {
public:
	WSAOVERLAPPED _over;
	WSABUF _wsabuf;
	char _send_buf[BUF_SIZE];
	COMP_TYPE _comp_type;
	OVER_EXP()
	{
		_wsabuf.len = BUF_SIZE;
		_wsabuf.buf = _send_buf;
		_comp_type = OP_RECV;
		ZeroMemory(&_over, sizeof(_over));
	}
	OVER_EXP(char* packet)
	{
		_wsabuf.len = packet[0];
		_wsabuf.buf = _send_buf;
		ZeroMemory(&_over, sizeof(_over));
		_comp_type = OP_SEND;
		memcpy(_send_buf, packet, packet[0]);
	}
};

enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };
class SESSION {
	OVER_EXP _recv_over;

public:
	mutex _s_lock;
	S_STATE _state;
	int _id;
	SOCKET _socket;
	short	x, y;
	char	_name[NAME_SIZE];
	
	unordered_set<int> view_list;
	mutex	_vl_l;

	// SECTOR 좌표(행, 열)
	pair<int, int> _sector;
	
	int		_prev_remain;
	int		_last_move_time;

public:
	SESSION()
	{
		_id = -1;
		_socket = 0;
		x = y = 0;
		_name[0] = 0;
		_state = ST_FREE;
		_prev_remain = 0;
	}

	~SESSION() {}

	void do_recv()
	{
		DWORD recv_flag = 0;
		memset(&_recv_over._over, 0, sizeof(_recv_over._over));
		_recv_over._wsabuf.len = BUF_SIZE - _prev_remain;
		_recv_over._wsabuf.buf = _recv_over._send_buf + _prev_remain;
		WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag,
			&_recv_over._over, 0);
	}

	void do_send(void* packet)
	{
		OVER_EXP* sdata = new OVER_EXP{ reinterpret_cast<char*>(packet) };
		WSASend(_socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, 0);
	}
	void send_login_info_packet()
	{
		SC_LOGIN_INFO_PACKET p;
		p.id = _id;
		p.size = sizeof(SC_LOGIN_INFO_PACKET);
		p.type = SC_LOGIN_INFO;
		p.x = x;
		p.y = y;
		do_send(&p);
	}
	void send_move_packet(int c_id);
	void send_add_player_packet(int c_id);
	void send_remove_player_packet(int c_id)
	{
		_vl_l.lock();
		view_list.erase(c_id);
		_vl_l.unlock();

		SC_REMOVE_PLAYER_PACKET p;
		p.id = c_id;
		p.size = sizeof(p);
		p.type = SC_REMOVE_PLAYER;
		do_send(&p);
	}

	void initialize_sector()
	{
		_sector = { static_cast<int>(x / SECTOR_ROW),
			static_cast<int>(y / SECTOR_COL) };

		g_sectors[_sector].add_player_list(_id);

	}

	void update_sector();
	
};

array<SESSION, MAX_USER> clients;

SOCKET g_s_socket, g_c_socket;
OVER_EXP g_a_over;

bool can_see(int a, int b)
{
	int dist = (clients[a].x - clients[b].x) * (clients[a].x - clients[b].x) +
		(clients[a].y - clients[b].y) * (clients[a].y - clients[b].y);
	return dist <= VIEW_RANGE * VIEW_RANGE;
}

void SESSION::send_move_packet(int c_id)
{
	SC_MOVE_PLAYER_PACKET p;
	p.id = c_id;
	p.size = sizeof(SC_MOVE_PLAYER_PACKET);
	p.type = SC_MOVE_PLAYER;
	p.x = clients[c_id].x;
	p.y = clients[c_id].y;
	p.move_time = clients[c_id]._last_move_time;
	do_send(&p);
}

void SESSION::send_add_player_packet(int c_id)
{
	_vl_l.lock();
	view_list.insert(c_id);
	_vl_l.unlock();

	SC_ADD_PLAYER_PACKET add_packet;
	add_packet.id = c_id;
	strcpy_s(add_packet.name, clients[c_id]._name);
	add_packet.size = sizeof(add_packet);
	add_packet.type = SC_ADD_PLAYER;
	add_packet.x = clients[c_id].x;
	add_packet.y = clients[c_id].y;
	do_send(&add_packet);
}

void SESSION::update_sector()
{
	pair<int, int> new_sector{ static_cast<int>(x / SECTOR_ROW),
		static_cast<int>(y / SECTOR_COL) };

	// SECTOR 변화 없으면 return
	if (_sector == new_sector) return;

	// 기존 SECTOR에서 삭제
	g_sectors[_sector].remove_player_list(_id);

	// 새로운 SECTOR에 추가
	g_sectors[new_sector].add_player_list(_id);

	_sector = new_sector;
}

int get_new_client_id()
{
	for (int i = 0; i < MAX_USER; ++i) {
		lock_guard <mutex> ll{ clients[i]._s_lock };
		if (clients[i]._state == ST_FREE)
			return i;
	}
	return -1;
}

void process_packet(int c_id, char* packet)
{
	switch (packet[1]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		strcpy_s(clients[c_id]._name, p->name);
		clients[c_id].send_login_info_packet();

		// 로그인 시 SECTOR 할당
		clients[c_id].initialize_sector();

		{
			lock_guard<mutex> ll{ clients[c_id]._s_lock };
			clients[c_id]._state = ST_INGAME;
		}

		// 현재 클라이언트가 속해있는 SECTOR 내부 검색으로 주변 다른 client에게 전송
		for (int pl : g_sectors[clients[c_id]._sector]._player_list)
		{
			if (false == can_see(c_id, pl)) continue;
			if (pl == c_id) continue;
			clients[pl].send_add_player_packet(c_id);
			clients[c_id].send_add_player_packet(pl);
		}
		break;
	}
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
		clients[c_id]._last_move_time = p->move_time;
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

		// sector update
		clients[c_id].update_sector();

		// client 주변 sector 검색 후 new_player_list에 추가
		unordered_set<int> near_sector_player_list;

		pair<int, int> current_sector = clients[c_id]._sector;

		near_sector_player_list = g_sectors[current_sector]._player_list;

		for (int i = 0; i < 8; ++i)
		{
			int x = current_sector.first + near_sector[i].first;
			int y = current_sector.second + near_sector[i].second;
			if (x < 0 || y < 0 || x > W_WIDTH / SECTOR_ROW || y > W_HEIGHT / SECTOR_COL) continue;

			for (int pl : g_sectors[{x, y}]._player_list)
			{
				near_sector_player_list.emplace(pl);
			}
		}

		clients[c_id]._vl_l.lock();
		unordered_set<int> old_viewlist = clients[c_id].view_list;
		clients[c_id]._vl_l.unlock();
		unordered_set<int> new_viewlist;

		clients[c_id].send_move_packet(c_id);

		for (int pl : near_sector_player_list) {
			if (false == can_see(c_id, pl)) continue;
			if (pl == c_id) continue;
			new_viewlist.emplace(pl);
		}

		clients[c_id].send_move_packet(c_id);

		for (int p_id : new_viewlist) {
			if (0 == old_viewlist.count(p_id)) {
				clients[c_id].send_add_player_packet(p_id);
				clients[p_id].send_add_player_packet(c_id);
			}
			else {
				clients[p_id].send_move_packet(c_id);
			}
		}

		for (int p_id : old_viewlist) {
			if (0 == new_viewlist.count(p_id)) {
				clients[c_id].send_remove_player_packet(p_id);
				clients[p_id].send_remove_player_packet(c_id);
			}
		}

	}
	}
}

void disconnect(int c_id)
{
	unordered_set<int> near_sector_player_list;
	pair<int, int> current_sector = clients[c_id]._sector;

	near_sector_player_list = g_sectors[current_sector]._player_list;

	for (int i = 0; i < 8; ++i)
	{
		int x = current_sector.first + near_sector[i].first;
		int y = current_sector.second + near_sector[i].second;
		if (x < 0 || y < 0 || x > W_WIDTH / SECTOR_ROW || y > W_HEIGHT / SECTOR_COL) continue;

		for (int pl : g_sectors[{x, y}]._player_list)
		{
			near_sector_player_list.emplace(pl);
		}
	}

	for (int pl : near_sector_player_list)
	{
		if (pl == c_id) continue;
		if (false == can_see(c_id, pl))   continue;
		clients[pl].send_remove_player_packet(c_id);
	}

	closesocket(clients[c_id]._socket);

	lock_guard<mutex> ll(clients[c_id]._s_lock);
	clients[c_id]._state = ST_FREE;
}

void worker_thread(HANDLE h_iocp)
{
	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP* ex_over = reinterpret_cast<OVER_EXP*>(over);
		if (FALSE == ret) {
			if (ex_over->_comp_type == OP_ACCEPT) cout << "Accept Error";
			else {
				cout << "GQCS Error on client[" << key << "]\n";
				disconnect(static_cast<int>(key));
				if (ex_over->_comp_type == OP_SEND) delete ex_over;
				continue;
			}
		}

		if ((0 == num_bytes) && ((ex_over->_comp_type == OP_RECV) || (ex_over->_comp_type == OP_SEND))) {
			disconnect(static_cast<int>(key));
			if (ex_over->_comp_type == OP_SEND) delete ex_over;
			continue;
		}

		switch (ex_over->_comp_type) {
		case OP_ACCEPT: {
			int client_id = get_new_client_id();
			std::default_random_engine dre{ static_cast<unsigned int>(client_id) };

			if (client_id != -1) {
				{
					lock_guard<mutex> ll(clients[client_id]._s_lock);
					clients[client_id]._state = ST_ALLOC;
				}
				clients[client_id].x = x_uid(dre);
				clients[client_id].y = y_uid(dre);
				clients[client_id]._id = client_id;
				clients[client_id]._name[0] = 0;
				clients[client_id]._prev_remain = 0;
				clients[client_id]._socket = g_c_socket;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket),
					h_iocp, client_id, 0);
				clients[client_id].do_recv();
				g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else {
				cout << "Max user exceeded.\n";
			}
			ZeroMemory(&g_a_over._over, sizeof(g_a_over._over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(g_s_socket, g_c_socket, g_a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);
			break;
		}
		case OP_RECV: {
			int remain_data = num_bytes + clients[key]._prev_remain;
			char* p = ex_over->_send_buf;
			while (remain_data > 0) {
				int packet_size = p[0];
				if (packet_size <= remain_data) {
					process_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}
			clients[key]._prev_remain = remain_data;
			if (remain_data > 0) {
				memcpy(ex_over->_send_buf, p, remain_data);
			}
			clients[key].do_recv();
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

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(g_s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(g_s_socket, SOMAXCONN);

	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);

	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_s_socket), h_iocp, 9999, 0);
	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over._comp_type = OP_ACCEPT;
	AcceptEx(g_s_socket, g_c_socket, g_a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);

	initialize_sector();
	initialize_near_sector();

	vector <thread> worker_threads;

	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);
	for (auto& th : worker_threads)
		th.join();

	closesocket(g_s_socket);
	WSACleanup();
}
