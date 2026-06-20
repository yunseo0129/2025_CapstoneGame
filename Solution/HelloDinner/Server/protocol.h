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

// Phase 1: 게임 상태머신 동기화 패킷 (서버 → 클라)
constexpr char SC_PHASE_CHANGE = 20;   // 페이즈 전환
constexpr char SC_ROUND_START  = 21;   // 라운드 시작 (타이머 동기화)
constexpr char SC_ROUND_END    = 22;   // 라운드 종료 (승패 + 점수)
constexpr char SC_SCORE_UPDATE = 33;   // K/D/A + 팀 점수 브로드캐스트

struct SC_PHASE_CHANGE_PACKET {
    unsigned char size;
    char          type;    // SC_PHASE_CHANGE
    unsigned char phase;   // 0=CHARSELECT 1=SCOREBOARD 2=SHOP 3=PLAYING 4=GAMEOVER
    unsigned char round;   // 현재 라운드 (1~10)
};

struct SC_ROUND_START_PACKET {
    unsigned char size;
    char          type;           // SC_ROUND_START
    unsigned char round;          // 1~10
    unsigned int  duration_ms;    // 라운드 길이 (ms)
    unsigned int  server_time_ms; // 서버 현재 시각 (ms)
};

struct SC_ROUND_END_PACKET {
    unsigned char size;
    char          type;         // SC_ROUND_END
    unsigned char winner_team;  // 0=팀A, 1=팀B, 2=무승부
    unsigned char score_a;
    unsigned char score_b;
};

struct PlayerStatBrief {
    int            player_id;
    unsigned char  kills;
    unsigned char  deaths;
    unsigned char  assists;
    unsigned short money;
};

struct SC_SCORE_UPDATE_PACKET {
    unsigned char   size;
    char            type;          // SC_SCORE_UPDATE
    unsigned char   score_a;
    unsigned char   score_b;
    unsigned char   player_count;
    PlayerStatBrief stats[ROOM_MAX_PLAYER];
};

#pragma pack (pop)