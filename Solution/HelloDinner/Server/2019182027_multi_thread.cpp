#include <iostream>
#include <array>
#include <thread>
#include <random>
#include <mutex>
#include <vector>
#include <unordered_set>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include "protocol.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
using namespace std;

std::uniform_int_distribution<int> x_uid(0, W_WIDTH);
std::uniform_int_distribution<int> y_uid(0, W_HEIGHT);

void disconnect(int c_id);

enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };

constexpr int VIEW_RANGE = 5;

class SESSION {

public:
	S_STATE _state;
	mutex _s_lock;
	mutex	_vl_l;
	unordered_set<int> view_list;
	WSABUF _wsabuf;
	char _send_buf[BUF_SIZE];

	int _id;
	SOCKET _socket;
	short	x, y;
	char	_name[NAME_SIZE];
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
		_wsabuf.buf = _send_buf;
	}

	~SESSION() {}

	void do_recv()
	{
		DWORD recv_size;
		DWORD recv_flag = 0;

		_wsabuf.len = BUF_SIZE - _prev_remain;
		_wsabuf.buf = _send_buf + _prev_remain;

		int rev = WSARecv(_socket, &_wsabuf, 1, &recv_size, &recv_flag, nullptr, nullptr);

		if (rev != SOCKET_ERROR)
		{
			// 남은 데이터 처리

			int remain_data = recv_size + _prev_remain;
			char* p = _send_buf;
			while (remain_data > 0) {
				int packet_size = p[0];
				if (packet_size <= remain_data) {
					process_packet(_id, p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}

			_prev_remain = remain_data;

			if (remain_data > 0) {
				memcpy(_send_buf, p, remain_data);
			}
		}
		else
		{
			disconnect(_id);
			
		}
	}

	void do_send(void* packet)
	{
		DWORD send_size;
		DWORD send_flag = 0;

		char* p = reinterpret_cast<char*>(packet);
		WSABUF wsabuf_send;
		wsabuf_send.len = p[0];
		wsabuf_send.buf = p;

		int rev = WSASend(_socket, &wsabuf_send, 1, &send_size, send_flag, nullptr, nullptr);

		if (rev != 0)
		{
			disconnect(_id);
		}
	}

	void process_packet(int c_id, char* packet);
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
};

array<SESSION, MAX_USER> clients;

SOCKET g_s_socket, g_c_socket;

bool can_see(int a, int b)
{
	int dist = (clients[a].x - clients[b].x) * (clients[a].x - clients[b].x) +
		(clients[a].y - clients[b].y) * (clients[a].y - clients[b].y);
	return dist <= VIEW_RANGE * VIEW_RANGE;
	//if (abs(clients[a].x - clients[b].x) > VIEW_RANGE) return false;
	//return (abs(clients[a].y - clients[b].y) <= VIEW_RANGE);
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

int get_new_client_id()
{
	for (int i = 0; i < MAX_USER; ++i) {
		lock_guard <mutex> ll{ clients[i]._s_lock };
		if (clients[i]._state == ST_FREE)
			return i;
	}
	return -1;
}

void SESSION::process_packet(int c_id, char* packet)
{
	switch (packet[1]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		strcpy_s(clients[c_id]._name, p->name);
		clients[c_id].x = x;
		clients[c_id].y = y;
		clients[c_id].send_login_info_packet();
		{
			lock_guard<mutex> ll{ clients[c_id]._s_lock };
			clients[c_id]._state = ST_INGAME;
		}
		for (auto& pl : clients) {
			{
				lock_guard<mutex> ll(pl._s_lock);
				if (ST_INGAME != pl._state) continue;
			}
			if (false == can_see(c_id, pl._id)) continue;
			if (pl._id == c_id) continue;
			pl.send_add_player_packet(c_id);
			clients[c_id].send_add_player_packet(pl._id);
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

		clients[c_id]._vl_l.lock();
		unordered_set<int> old_viewlist = clients[c_id].view_list;
		clients[c_id]._vl_l.unlock();
		unordered_set<int> new_viewlist;

		for (auto& pl : clients) {
			if (pl._state != ST_INGAME) continue;
			if (false == can_see(c_id, pl._id)) continue;
			if (pl._id == c_id) continue;
			new_viewlist.insert(pl._id);
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

std::vector <thread> worker_threads;

void disconnect(int c_id)
{
	for (auto& pl : clients) {
		{
			lock_guard<mutex> ll(pl._s_lock);
			if (ST_INGAME != pl._state) continue;
		}
		if (pl._id == c_id) continue;
		pl.send_remove_player_packet(c_id);
	}
	closesocket(clients[c_id]._socket);
	lock_guard<mutex> ll(clients[c_id]._s_lock);
	clients[c_id]._state = ST_FREE;
}

void worker(SOCKET client)
{
	// Client Initialize
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
		clients[client_id]._socket = client;
	}
	else {
		cout << "Max user exceeded.\n";
	}

	// recv
	while (true)
	{
		clients[client_id].do_recv();
	}

}

int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	SOCKET server = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;

	bind(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(server, SOMAXCONN);

	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);

	int index{};

	while (true) {
		SOCKET client = WSAAccept(server,
			reinterpret_cast<sockaddr*>(&cl_addr), &addr_size, NULL, NULL);
		
		// 접속시 스레드 생성
		if (client != INVALID_SOCKET)
		{
			worker_threads.emplace_back(worker, client);
			worker_threads[index++].detach();
		}
	}
	closesocket(server);
	WSACleanup();
}