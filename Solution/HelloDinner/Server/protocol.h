#pragma once

constexpr int LOBBY_PORT = 4000;
constexpr int INSTANCE_PORT_BASE = 5000;  // 인스턴스별 5001, 5002, ...
constexpr int INTERNAL_PORT = 3999;       // 로비↔인스턴스 내부 통신 포트
constexpr int BUF_SIZE = 200;
constexpr int NAME_SIZE = 20;

constexpr int MAX_USER = 4000;
constexpr int ROOM_MAX_PLAYER = 6;
constexpr int MAX_ROOM = MAX_USER / ROOM_MAX_PLAYER;
constexpr int MAX_INSTANCE_SERVERS = 5;

// 키입력 (비트 플래그)
constexpr unsigned char KEY_W     = 0x01;
constexpr unsigned char KEY_S     = 0x02;
constexpr unsigned char KEY_A     = 0x04;
constexpr unsigned char KEY_D     = 0x08;
constexpr unsigned char KEY_SPACE = 0x10;
constexpr unsigned char KEY_CTRL  = 0x20;

// 패킷 ID (클라 → 서버)
constexpr char CS_LOGIN = 0;
constexpr char CS_MOVE = 1;
constexpr char CS_LOGOUT = 2;
constexpr char CS_JOIN_ROOM = 3;       // 인스턴스 서버에 방 진입 요청

// 패킷 ID (서버 → 클라)
constexpr char SC_LOGIN_INFO = 0;
constexpr char SC_ADD_PLAYER = 1;
constexpr char SC_REMOVE_PLAYER = 2;
constexpr char SC_MOVE_PLAYER = 3;
constexpr char SC_MATCH_WAIT = 4;
constexpr char SC_MATCH_SUCCESS = 5;
constexpr char SC_REDIRECT = 6;        // 인스턴스 서버 주소 전달

// 인스턴스 ↔ 로비 (내부 통신)
constexpr char IS_HEARTBEAT = 10;      // 부하 정보 리포트
constexpr char IS_REGISTER = 11;       // 인스턴스 등록 요청
constexpr char IS_ROOM_NOTIFY = 12;    // 방 정보 전달 (로비→인스턴스)

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND };
enum S_STATE { ST_FREE, ST_ALLOC, ST_LOBBY, ST_INGAME };

#pragma pack (push, 1)

struct CS_LOGIN_PACKET {
    unsigned char size;
    char    type;
    char    name[NAME_SIZE];
};

struct CS_MOVE_PACKET {
    unsigned char size;
    char    type;
    unsigned char   keyInput;
    unsigned int    timestamp;
    float   mouseYaw;
    float   worldMatrix[16];
};

struct CS_LOGOUT_PACKET {
    unsigned char size;
    char    type;
};

// 인스턴스 서버에 방 진입 요청
struct CS_JOIN_ROOM_PACKET {
    unsigned char size;
    char    type;
    int     room_id;
    int     player_id;
    char    name[NAME_SIZE];
    char    auth_token[32];
};

struct SC_LOGIN_INFO_PACKET {
    unsigned char size;
    char    type;
    int     id;
    float   worldMatrix[16];
};

struct SC_ADD_PLAYER_PACKET {
    unsigned char size;
    char    type;
    int     id;
    float   worldMatrix[16];
    char    name[NAME_SIZE];
};

struct SC_REMOVE_PLAYER_PACKET {
    unsigned char size;
    char    type;
    int     id;
};

struct SC_MOVE_PLAYER_PACKET {
    unsigned char size;
    char    type;
    int     id;
    unsigned char   keyInput;
    unsigned int    timestamp;
    float   worldMatrix[16];
};

struct SC_MATCH_WAIT_PACKET {
    unsigned char size;
    char    type;
    int     queue_size;
};

struct SC_MATCH_SUCCESS_PACKET {
    unsigned char size;
    char    type;
    int     room_id;
    int     player_count;
    int     player_ids[ROOM_MAX_PLAYER];
};

// 클라이언트에게 인스턴스 서버 주소 전달
struct SC_REDIRECT_PACKET {
    unsigned char size;
    char    type;
    char    ip[16];
    unsigned short port;
    int     room_id;
    char    auth_token[32];
};

// 인스턴스 서버 → 로비 서버 부하 리포트
struct IS_HEARTBEAT_PACKET {
    unsigned char size;
    char    type;
    int     instance_id;
    int     current_rooms;
    int     current_players;
    float   cpu_usage;
};

// 인스턴스 서버 → 로비 서버 등록
struct IS_REGISTER_PACKET {
    unsigned char size;
    char    type;
    int     instance_id;
    char    ip[16];
    unsigned short port;
};

// 로비 서버 → 인스턴스 서버 방 정보 전달
struct IS_ROOM_NOTIFY_PACKET {
    unsigned char size;
    char    type;
    int     room_id;
    int     player_count;
    int     player_ids[ROOM_MAX_PLAYER];
    char    player_names[ROOM_MAX_PLAYER][NAME_SIZE];
    char    auth_token[32];
};

#pragma pack (pop)