constexpr int PORT_NUM = 4000;
constexpr int BUF_SIZE = 200;
constexpr int NAME_SIZE = 20;

constexpr int MAX_USER = 20000;

// 패킷 ID
constexpr char CS_LOGIN = 0;
constexpr char CS_MOVE = 1;
constexpr char CS_LOGOUT = 2;

constexpr char SC_LOGIN_INFO = 0;
constexpr char SC_ADD_PLAYER = 1;
constexpr char SC_REMOVE_PLAYER = 2;
constexpr char SC_MOVE_PLAYER = 3;

// 패킷 처리 완료 유형을 나타내는 enum
enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND };

// 세션 상태를 나타내는 enum
enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };

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
	char	direction;  // 0 : UP, 1 : DOWN, 2 : LEFT, 3 : RIGHT
	unsigned	move_time;
};

struct CS_LOGOUT_PACKET {
	unsigned char size;
	char	type;
	int		id;
};

// ================================ 서버 => 클라이언트 패킷 ================================
struct SC_LOGIN_INFO_PACKET {
	unsigned char size;
	char	type;
	int		id;
	float   positionX;
	float	positionY;
	float	positionZ;
	float	rotationX;
	float	rotationY;
	float	rotationZ;
};

struct SC_ADD_PLAYER_PACKET {
	unsigned char size;
	char	type;
	int		id;
	float   positionX;
	float	positionY;
	float	positionZ;
	float	rotationX;
	float	rotationY;
	float	rotationZ;
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
	short	x, y, z;
	unsigned int move_time;
};
#pragma pack (pop)