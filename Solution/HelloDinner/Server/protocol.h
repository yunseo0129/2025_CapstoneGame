#pragma once

constexpr int PORT_NUM = 4000;
constexpr int BUF_SIZE = 200;
constexpr int NAME_SIZE = 20;

constexpr int MAX_USER = 4000;
constexpr int ROOM_MAX_PLAYER = 6;		// 6인 매칭	
constexpr int MAX_ROOM = MAX_USER / ROOM_MAX_PLAYER;			

// 키인풋 
constexpr char KEY_W = 1;
constexpr char KEY_A = 2;
constexpr char KEY_S = 3;
constexpr char KEY_D = 4;

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

// 패킷 처리 완료 유형을 나타내는 enum
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
	unsigned char	keyInput;			// 키인풋
	float	cameraPosX;					// 카메라 위치 (= 플레이어 위치)
	float	cameraPosY;
	float	cameraPosZ;
	float	cameraYaw;					// 좌우 회전 (= 플레이어 Y축 회전)
	float	cameraPitch;				// 상하 회전
	float	cameraLookX;				// 카메라 look 벡터
	float	cameraLookY;
	float	cameraLookZ;
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
	float	cameraPosX;					// 초기 위치
	float	cameraPosY;
	float	cameraPosZ;
	float	cameraYaw;					// 초기 Y축 회전
	float	cameraPitch;				// 초기 X축 회전
};

struct SC_ADD_PLAYER_PACKET {
	unsigned char size;
	char	type;
	int		id;
	float	cameraPosX;					// 해당 플레이어 위치
	float	cameraPosY;
	float	cameraPosZ;
	float	cameraYaw;					// 해당 플레이어 Y축 회전
	float	cameraPitch;				// 해당 플레이어 X축 회전
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
	unsigned char	keyInput;			// 키인풋 (클라에서 애니메이션 판단용)
	float	cameraPosX;					// 카메라 위치 (= 플레이어 위치)
	float	cameraPosY;
	float	cameraPosZ;
	float	cameraYaw;					// 좌우 회전 (= 플레이어 Y축 회전)
	float	cameraPitch;				// 상하 회전
	float	cameraLookX;				// 카메라 look 벡터
	float	cameraLookY;
	float	cameraLookZ;
};

struct SC_MATCH_WAIT_PACKET {
	unsigned char size;
	char	type;
	int		queue_size;					// 현재 대기큐 인원
};

struct SC_MATCH_SUCCESS_PACKET {
	unsigned char size;
	char	type;
	// Todo: 수정 필요
	int		room_id;
	int		player_count;
	int		player_ids[ROOM_MAX_PLAYER];
};

#pragma pack (pop)