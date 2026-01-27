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
#include <queue>
#include <locale.h>
#include <chrono>

#define UNICODE  
#include <sqlext.h>  
#include "protocol.h"

#include "include/lua.hpp"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "lua54.lib")
using namespace std;

void disp_error(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
	SQLSMALLINT iRec = 0;
	SQLINTEGER iError;
	WCHAR wszMessage[1000];
	WCHAR wszState[SQL_SQLSTATE_SIZE + 1];
	if (RetCode == SQL_INVALID_HANDLE) {
		fwprintf(stderr, L"Invalid handle!\n");
		return;
	}
	while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
		(SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT*)NULL) == SQL_SUCCESS) {
		// Hide data truncated..
		if (wcsncmp(wszState, L"01004", 5)) {
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
	}
}

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND, OP_RANDOM_MOVE};

std::uniform_int_distribution<int> x_uid(0, W_WIDTH);
std::uniform_int_distribution<int> y_uid(0, W_HEIGHT);

constexpr int VIEW_RANGE = 7;
constexpr int NPC_START = 0;
constexpr int USER_START = MAX_NPC;

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

	pair<int, int> near_sector[8];

	SECTOR() 
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
		_player_list.emplace(c_id);
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

bool is_npc(int a)
{
	return a < MAX_NPC;
}

enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };

class SESSION {
	OVER_EXP _recv_over;

public:
	mutex _s_lock;
	S_STATE _state;
	atomic_bool _active;
	int _id;
	SOCKET _socket;
	short	x, y;
	char	_name[NAME_SIZE];
	chrono::system_clock::time_point _rm_time;

	unordered_set<int> view_list;
	mutex	_vl_l;

	int		_prev_remain;
	int		_last_move_time;

	pair<int, int> _sector;

	lua_State* _L;
	mutex	_ll;
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
		if (true == is_npc(_id))
		{
			return;
		}
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
	void send_add_object_packet(int c_id);
	void send_remove_object_packet(int c_id)
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

	void do_random_move();
	void heart_beat()
	{
		do_random_move();
	}

	void initialize_sector()
	{
		_sector = { static_cast<int>(x / SECTOR_ROW),
			static_cast<int>(y / SECTOR_COL) };

		g_sectors[_sector].add_player_list(_id);

	}

	void update_sector()
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
};

enum EVENT_TYPE{EV_RANDOM_MOVE, EV_HEAL, EV_ATTACK};

class EVENT {
public:
	int obj_id;
	chrono::system_clock::time_point wakeup_time;
	EVENT_TYPE e_type;
	int target_id;

	constexpr bool operator<(const EVENT& other) const {
		return wakeup_time > other.wakeup_time;
	}
};

class DBEVENT {
public:
	int id;


};

priority_queue<EVENT> g_event_queue;
queue<DBEVENT> g_db_event_queue;
mutex eql;

// NPC, Player랑 섞어쓰면 최적화에 문제가 있음
array<SESSION, MAX_NPC + MAX_USER> objects;

SOCKET g_s_socket, g_c_socket;
OVER_EXP g_a_over;

bool can_see(int a, int b)
{
	int dist = (objects[a].x - objects[b].x) * (objects[a].x - objects[b].x) +
		(objects[a].y - objects[b].y) * (objects[a].y - objects[b].y);
	return dist <= VIEW_RANGE * VIEW_RANGE;
}

void add_timer(int id, EVENT_TYPE et, int ms)
{
	EVENT ev;
	ev.obj_id = id;
	ev.e_type = et;
	ev.wakeup_time = chrono::system_clock::now() + chrono::milliseconds(ms);

	eql.lock();
	g_event_queue.push(ev);
	eql.unlock();

}

void SESSION::do_random_move() 
{
	objects[_id].update_sector();

	pair<int, int> current_sector = _sector;

	unordered_set<int> old_vl;
	for (int i = MAX_NPC; i < MAX_NPC + MAX_USER; ++i)
	{
		if (objects[i]._state != ST_INGAME) continue;
		if (true == can_see(i, _id))
		{
			old_vl.insert(i);
		}
	}

	switch (rand() % 4)
	{
	case 0: if (x > 0) x--; break;
	case 1: if (x < W_WIDTH - 1) x++; break;
	case 2: if (y > 0) y--; break;
	case 3: if (y < W_HEIGHT - 1) y++; break;
	}

	unordered_set<int> new_vl;

	unordered_set<int> near_sector_player_list;

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

	for (int pl : near_sector_player_list) {
		if (is_npc(pl) == true) continue;
		if (false == can_see(_id, pl)) continue;
		if (pl == _id) continue;
		new_vl.emplace(pl);
	}

	for (auto pl : new_vl)
	{
		if (0 == old_vl.count(pl))
		{
			objects[pl].send_add_object_packet(_id);
		}
		else
		{
			objects[pl].send_move_packet(_id);
		}
	}
	for (auto pl : old_vl)
	{
		if (0 == new_vl.count(pl))
		{
			objects[pl].send_remove_object_packet(_id);
		}
	}

}

void SESSION::send_move_packet(int c_id)
{
	if (true == is_npc(_id)) return;

	SC_MOVE_PLAYER_PACKET p;
	p.id = c_id;
	p.size = sizeof(SC_MOVE_PLAYER_PACKET);
	p.type = SC_MOVE_PLAYER;
	p.x = objects[c_id].x;
	p.y = objects[c_id].y;
	p.move_time = objects[c_id]._last_move_time;
	do_send(&p);
}

void SESSION::send_add_object_packet(int c_id)
{
	_vl_l.lock();
	view_list.emplace(c_id);
	_vl_l.unlock();

	SC_ADD_PLAYER_PACKET add_packet;
	add_packet.id = c_id;
	strcpy_s(add_packet.name, objects[c_id]._name);
	add_packet.size = sizeof(add_packet);
	add_packet.type = SC_ADD_PLAYER;
	add_packet.x = objects[c_id].x;
	add_packet.y = objects[c_id].y;
	do_send(&add_packet);
}

int get_new_client_id()
{
	for (int i = MAX_NPC; i < MAX_NPC + MAX_USER; ++i) {
		lock_guard <mutex> ll{ objects[i]._s_lock };
		if (objects[i]._state == ST_FREE)
			return i;
	}
	return -1;
}

void process_packet(int c_id, char* packet)
{
	switch (packet[1]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		// DB 스레드에서 해당 ID가 유효한지 검사

		char id[20];
		strcpy_s(id, p->name);
		
		// DB 호출

		// 성공하면 LOGIN
		strcpy_s(objects[c_id]._name, p->name);

		// DB 에 있는 x,y 꺼내옴
		// x,y가  null이면 업데이트 X
		objects[c_id].send_login_info_packet();

		objects[c_id].initialize_sector();

		{
			lock_guard<mutex> ll{ objects[c_id]._s_lock };
			objects[c_id]._state = ST_INGAME;
		}

		for (int pl : g_sectors[objects[c_id]._sector]._player_list)
		{
			if (false == can_see(c_id, pl)) continue;
			if (pl == c_id) continue;
			objects[pl].send_add_object_packet(c_id);
			objects[c_id].send_add_object_packet(pl);
		}
		break;
	}
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
		objects[c_id]._last_move_time = p->move_time;
		short x = objects[c_id].x;
		short y = objects[c_id].y;
		switch (p->direction) {
		case 0: if (y > 0) y--; break;
		case 1: if (y < W_HEIGHT - 1) y++; break;
		case 2: if (x > 0) x--; break;
		case 3: if (x < W_WIDTH - 1) x++; break;
		}
		objects[c_id].x = x;
		objects[c_id].y = y;

		objects[c_id].update_sector();

		pair<int, int> current_sector = objects[c_id]._sector;

		objects[c_id]._vl_l.lock();
		unordered_set<int> old_viewlist = objects[c_id].view_list;
		objects[c_id]._vl_l.unlock();
		unordered_set<int> new_viewlist;

		unordered_set<int> near_sector_player_list;

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

		for (int pl : near_sector_player_list) {
			if ((true == is_npc(pl)))
			{
				bool expected = false;
				if (true == atomic_compare_exchange_strong(&objects[pl]._active, &expected, true))
					add_timer(pl, EV_RANDOM_MOVE, 1000);
			}
			if (false == can_see(c_id, pl)) continue;
			if (pl == c_id) continue;
			new_viewlist.emplace(pl);

		}

		objects[c_id].send_move_packet(c_id);

		for (int p_id : new_viewlist) {
			if (0 == old_viewlist.count(p_id)) {
				objects[c_id].send_add_object_packet(p_id);
				objects[p_id].send_add_object_packet(c_id);
			}
			else {
				objects[p_id].send_move_packet(c_id);
			}
		}

		for (int p_id : old_viewlist) {
			if (0 == new_viewlist.count(p_id)) {
				objects[c_id].send_remove_object_packet(p_id);
				objects[p_id].send_remove_object_packet(c_id);
			}
		}
	}
	}
}

void disconnect(int c_id)
{
	for (auto& pl : objects) {
		{
			lock_guard<mutex> ll(pl._s_lock);
			if (ST_INGAME != pl._state) continue;
		}
		if (pl._id == c_id) continue;
		if (true == can_see(pl._id, c_id)) continue;
		pl.send_remove_object_packet(c_id);
	}
	closesocket(objects[c_id]._socket);

	lock_guard<mutex> ll(objects[c_id]._s_lock);
	objects[c_id]._state = ST_FREE;
}

bool player_exist(int npc_id)
{
	for (int i = USER_START; i < USER_START + MAX_USER; ++i)
	{
		if (ST_INGAME != objects[i]._state) continue;
		if (true == can_see(npc_id, i))
			return true;
	}
	return false;
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
					lock_guard<mutex> ll(objects[client_id]._s_lock);
					objects[client_id]._state = ST_ALLOC;
				}
				objects[client_id].x = x_uid(dre);
				objects[client_id].y = y_uid(dre);
				objects[client_id]._id = client_id;
				objects[client_id]._name[0] = 0;
				objects[client_id]._prev_remain = 0;
				objects[client_id]._socket = g_c_socket;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket),
					h_iocp, client_id, 0);
				objects[client_id].do_recv();
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
			int remain_data = num_bytes + objects[key]._prev_remain;
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
			objects[key]._prev_remain = remain_data;
			if (remain_data > 0) {
				memcpy(ex_over->_send_buf, p, remain_data);
			}
			objects[key].do_recv();
			break;
		}
		case OP_SEND:
			delete ex_over;
			break;
		
		case OP_RANDOM_MOVE:
			if (true == player_exist(static_cast<int>(key)))
			{
				objects[key].do_random_move();

				add_timer(static_cast<int>(key), EV_RANDOM_MOVE, 1000);
			}
			else 
			{
				objects[key]._active = false;
			}
			delete ex_over;

			break;
		}
	}
}

void Initialize_NPC()
{
	cout << "NPC intialize begin.\n";

	for (int i = 0; i < MAX_NPC; ++i)
	{
		std::default_random_engine dre{ static_cast<unsigned int>(i) };

		objects[i].x = x_uid(dre);
		objects[i].y = y_uid(dre);
		objects[i]._id = i;
		sprintf_s(objects[i]._name, "N%d", i);
		objects[i]._state = ST_INGAME;
		objects[i]._rm_time = chrono::system_clock::now();
		objects[i]._active = false;
		objects[i]._sector = { static_cast<int>(objects[i].x / SECTOR_ROW), 
			static_cast<int>(objects[i].y / SECTOR_COL) };

		g_sectors[objects[i]._sector].add_player_list(objects[i]._id);
	}

	cout << "NPC initialize end.\n";
}

// 1s부터 시작 동접 4000까지
void do_ai_old()
{
	using namespace chrono;
	while (true)
	{
		for (int i = 0; i < MAX_NPC; ++i)
		{
			if (objects[i]._rm_time < system_clock::now() - 1s)
			{
				if (i == 1)
				{
					auto move_delay = system_clock::now() - objects[i]._rm_time;
					cout << "NPC[1] move delay = " << duration_cast<milliseconds>(move_delay).count() << "ms\n";
				}
				objects[i].do_random_move();
				objects[i]._rm_time = system_clock::now();
			}
			
		}
	}
}

// 0.2s부터 시작 
// Heart Beat 
void do_ai_hb()
{
	using namespace chrono;
	while (true)
	{
		auto start_time = system_clock::now();
		for (int i = 0; i < MAX_NPC; ++i)
		{
			if (i == 1)
			{
				auto move_delay = system_clock::now() - objects[i]._rm_time;
				cout << "NPC[1] move delay = " << duration_cast<milliseconds>(move_delay).count() << "ms\n";
			}
			objects[i].heart_beat();
			objects[i]._rm_time = system_clock::now();
		}
		auto end_time = system_clock::now();
		auto hb_time = end_time - start_time;
		if (hb_time < 1s)
		{
			this_thread::sleep_for(1s - hb_time);
		}
	}
}

// Worker Thread에 넘기기 
void do_ai_wk(HANDLE h_iocp)
{
	using namespace chrono;
	while (true)
	{
		auto start_time = system_clock::now();
		for (int i = 0; i < MAX_NPC; ++i)
		{
			if (i == 1)
			{
				auto move_delay = system_clock::now() - objects[i]._rm_time;
				cout << "NPC[1] move delay = " << duration_cast<milliseconds>(move_delay).count() << "ms\n";
			}
			OVER_EXP* over = new OVER_EXP;
			over->_comp_type = OP_RANDOM_MOVE;
			PostQueuedCompletionStatus(h_iocp, 1, i, &over->_over);
			objects[i]._rm_time = system_clock::now();
		}
		auto end_time = system_clock::now();
		auto hb_time = end_time - start_time;
		if (hb_time < 1s)
		{
			this_thread::sleep_for(1s - hb_time);
		}
	}
}

void do_timer(HANDLE h_iocp)
{
	using namespace chrono;
	while (true)
	{
		eql.lock();
		if (g_event_queue.empty())
		{
			eql.unlock();
			continue;
		}

		EVENT ev = g_event_queue.top();
		if (ev.wakeup_time < system_clock::now())
		{
			g_event_queue.pop();

			OVER_EXP* ov = new OVER_EXP;
			ov->_comp_type = OP_RANDOM_MOVE;
			PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
		}
		eql.unlock();
	}
}

void do_db(HANDLE h_iocp)
{
	SQLHENV henv;
	SQLHDBC hdbc;
	SQLHSTMT hstmt = 0;
	SQLRETURN retcode;
	SQLINTEGER dId;
	SQLSMALLINT dX;
	SQLSMALLINT dY;
	SQLLEN cbId = 0, cbX, cbY;

	setlocale(LC_ALL, "korean");

	// Allocate environment handle  
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

	// Set the ODBC version environment attribute  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);

		// Allocate connection handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

			// Set login timeout to 5 seconds  
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

				// Connect to data source  
				retcode = SQLConnect(hdbc, (SQLWCHAR*)L"2024_GameServer_ODBC", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);

				// Allocate statement handle  
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
					retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);

					retcode = SQLExecDirect(hstmt, (SQLWCHAR*)L"SELECT user_id, user_pos_x, user_pos_y FROM GameServer_Table", SQL_NTS);
					if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

						// Bind columns 1, 2, and 3  
						retcode = SQLBindCol(hstmt, 1, SQL_C_LONG, &dId, 10, &cbId);
						retcode = SQLBindCol(hstmt, 2, SQL_C_SHORT, &dX, 10, &cbX);
						retcode = SQLBindCol(hstmt, 3, SQL_C_SHORT, &dY, 10, &cbY);

						// Fetch and print each row of data. On an error, display a message and exit.  
						for (int i = 0; ; i++) {
							retcode = SQLFetch(hstmt);
							if (retcode == SQL_ERROR)
								disp_error(hstmt, SQL_HANDLE_STMT, retcode);
							if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
							{
								wprintf(L"%d: %6d %3d %3d\n", i + 1, dId, dX, dY);
							}
							else
								break;
						}
					}
					else {
						disp_error(hstmt, SQL_HANDLE_STMT, retcode);
					}
					// Process data  
					if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
						SQLCancel(hstmt);
						SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
					}

					SQLDisconnect(hdbc);
				}

				SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
			}
		}
		SQLFreeHandle(SQL_HANDLE_ENV, henv);
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
	Initialize_NPC();
	initialize_near_sector();

	thread ai_thread{ do_timer, h_iocp };

	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);
	for (auto& th : worker_threads)
		th.join();
	ai_thread.join();

	closesocket(g_s_socket);
	WSACleanup();
}
