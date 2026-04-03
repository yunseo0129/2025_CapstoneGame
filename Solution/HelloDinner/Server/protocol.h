#pragma once

constexpr int PORT_NUM = 4000;
constexpr int BUF_SIZE = 200;
constexpr int NAME_SIZE = 20;

constexpr int MAX_USER = 4000;
constexpr int ROOM_MAX_PLAYER = 6;		// 6인 매칭	
constexpr int MAX_ROOM = MAX_USER / ROOM_MAX_PLAYER;			

// 키인풋 (비트 플래그)
constexpr unsigned char KEY_W     = 0x01;
constexpr unsigned char KEY_S     = 0x02;
constexpr unsigned char KEY_A     = 0x04;
constexpr unsigned char KEY_D     = 0x08;
constexpr unsigned char KEY_SPACE = 0x10;
constexpr unsigned char KEY_CTRL  = 0x20;

// 패킷 ID
constexpr char CS_LOGIN = 0;
constexpr char CS_MOVE = 1;
constexpr char CS_LOGOUT = 2;

constexpr char SC_LOGIN_INFO = 0;
constexpr char SC_ADD_PLAYER = 1;
constexpr char SC_REMOVE_PLAYER = 2;
constexpr char SC_MOVE_PLAYER = 3;
constexpr char SC_MATCH_WAIT = 4;		
constexpr char SC_MATCH_SUCCESS = 5;	

// 패킷 처리 완료 상태를 나타내는 enum
enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND };

// 세션 상태를 나타내는 enum
enum S_STATE { ST_FREE, ST_ALLOC, ST_LOBBY, ST_INGAME };

// ================================ 클라이언트 => 서버 패킷 ================================
#pragma pack (push, 1)
struct CS_LOGIN_PACKET {
	unsigned char size;
	char	type;
	char	name[NAME_SIZE];
};

struct CS_MOVE_PACKET {
	unsigned char size;
	char	type;
	unsigned char	keyInput;			// 비트 플래그 (W|S|A|D|SPACE|CTRL)
	unsigned int	timestamp;			// 클라이언트 타임스탬프 (ms)
	float	mouseYaw;					// 마우스 좌우 회전량 (이번 프레임, raw값)
	float	worldMatrix[16];			// 클라이언트 예측 월드 매트릭스
};

struct CS_LOGOUT_PACKET {
	unsigned char size;
	char	type;
};

// ================================ 서버 => 클라이언트 패킷 ================================
struct SC_LOGIN_INFO_PACKET {
	unsigned char size;
	char	type;
	int		id;
	float	worldMatrix[16];
};

struct SC_ADD_PLAYER_PACKET {
	unsigned char size;
	char	type;
	int		id;
	float	worldMatrix[16];
	char	name[NAME_SIZE];
};

struct SC_REMOVE_PLAYER_PACKET {
	unsigned char size;
	char	type;
	int		id;
};

struct SC_MOVE_PLAYER_PACKET {
	unsigned char size;
	char	type;
	int		id;
	unsigned char	keyInput;
	unsigned int	timestamp;			// 서버 타임스탬프 (ms)
	float	worldMatrix[16];
};

struct SC_MATCH_WAIT_PACKET {
	unsigned char size;
	char	type;
	int		queue_size;
};

struct SC_MATCH_SUCCESS_PACKET {
	unsigned char size;
	char	type;
	int		room_id;
	int		player_count;
	int		player_ids[ROOM_MAX_PLAYER];
};

#pragma pack (pop)