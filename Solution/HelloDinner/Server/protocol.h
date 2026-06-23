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

// 키입력 (비트 플래그) — unsigned short (9키)
constexpr unsigned short KEY_W        = 0x0001;
constexpr unsigned short KEY_S        = 0x0002;
constexpr unsigned short KEY_A        = 0x0004;
constexpr unsigned short KEY_D        = 0x0008;
constexpr unsigned short KEY_SPACE    = 0x0010;
constexpr unsigned short KEY_CTRL     = 0x0020;
constexpr unsigned short KEY_SHIFT    = 0x0040;  // 달리기
constexpr unsigned short KEY_R        = 0x0080;  // 상호작용
constexpr unsigned short KEY_MOUSE_LB = 0x0100;  // 공격/사용

// 패킷 ID (클라 → 서버)
constexpr char CS_LOGIN = 0;
constexpr char CS_MOVE = 1;
constexpr char CS_LOGOUT = 2;
constexpr char CS_JOIN_ROOM = 3;       // 인스턴스 서버에 방 진입 요청
constexpr char CS_CHAR_SELECT = 4;     // 캐릭터 선택 완료 통보 (클라 권위)
// 수동 방 관련 패킷 (로비 소켓 전용)
constexpr char CS_CREATE_ROOM    = 5;  // 방 생성 요청
constexpr char CS_JOIN_ROOM_CODE = 6;  // 코드로 대기방 입장
constexpr char CS_START_GAME     = 7;  // 방장: 게임 시작
constexpr char CS_LEAVE_ROOM     = 8;  // 대기방 나가기
constexpr char CS_QUICK_MATCH    = 9;  // 빠른 매칭 옵트인
constexpr char CS_SELECT_SEAT    = 10; // 팀/번호 선택 (0xFF/0 = 해제)
constexpr char CS_PLAYER_READY   = 11; // 준비/준비해제 토글
constexpr char CS_PHASE_READY    = 13; // 인게임 페이즈 타이머 만료 알림 (인스턴스 서버 전용)
constexpr char CS_MAP_LOADED     = 14; // 맵 로드 완료 통보 (인스턴스 서버 전용)

// 패킷 ID (서버 → 클라)
constexpr char SC_LOGIN_INFO = 0;
constexpr char SC_ADD_PLAYER = 1;
constexpr char SC_REMOVE_PLAYER = 2;
constexpr char SC_MOVE_PLAYER = 3;
constexpr char SC_MATCH_WAIT = 4;
constexpr char SC_MATCH_SUCCESS = 5;
constexpr char SC_REDIRECT = 6;        // 인스턴스 서버 주소 전달
// 수동 방 관련 패킷 (로비 소켓 전용)
constexpr char SC_ROOM_CREATED     = 7;  // 방 코드 통보 (방장)
constexpr char SC_ROOM_JOIN_RESULT = 8;  // 입장 결과 (성공/실패)
constexpr char SC_ROOM_UPDATE      = 9;  // 멤버 목록 갱신 브로드캐스트

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
    unsigned char  size;
    char           type;
    unsigned short keyInput;    // KEY_W~KEY_MOUSE_LB 비트 플래그
    unsigned int   timestamp;
    float          mouseYaw;
    float          worldMatrix[16];
};

struct CS_LOGOUT_PACKET {
    unsigned char size;
    char    type;
};

// 캐릭터 선택 완료 (클라이언트 권위 — Ready 클릭 시 1회 전송)
struct CS_CHAR_SELECT_PACKET {
    unsigned char size;
    char          type;       // CS_CHAR_SELECT
    unsigned char char_type;  // 0=Pig, 1=Chick, 2=Blank(기본값)
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

// ── 수동 방 관련 패킷 구조체 ──────────────────────────────────────────────
enum ROOM_JOIN_RESULT : unsigned char {
    RJR_OK = 0, RJR_NOT_FOUND = 1, RJR_FULL = 2, RJR_STARTED = 3
};

struct CS_CREATE_ROOM_PACKET    { unsigned char size; char type; };
struct CS_JOIN_ROOM_CODE_PACKET { unsigned char size; char type; int code; };
struct CS_START_GAME_PACKET     { unsigned char size; char type; };
struct CS_LEAVE_ROOM_PACKET     { unsigned char size; char type; };
struct CS_QUICK_MATCH_PACKET    { unsigned char size; char type; };
struct CS_SELECT_SEAT_PACKET    {
    unsigned char size; char type;
    unsigned char team; // 0=RED, 1=BLUE, 0xFF=선택 해제
    unsigned char slot; // 1~3, 0=선택 해제
};
struct CS_PLAYER_READY_PACKET   { unsigned char size; char type; unsigned char ready; }; // 1=준비, 0=해제

struct SC_ROOM_CREATED_PACKET   { unsigned char size; char type; int code; };
struct SC_ROOM_JOIN_RESULT_PACKET {
    unsigned char size; char type;
    unsigned char result;  // ROOM_JOIN_RESULT
    int           code;    // 성공 시 입장한 방 코드
};
struct SC_ROOM_UPDATE_PACKET {
    unsigned char size; char type;
    int           code;
    int           host_id;
    unsigned char member_count;
    int           member_ids[ROOM_MAX_PLAYER];
    char          member_names[ROOM_MAX_PLAYER][NAME_SIZE];
    unsigned char member_teams[ROOM_MAX_PLAYER]; // 0=RED, 1=BLUE, 0xFF=미선택
    unsigned char member_slots[ROOM_MAX_PLAYER]; // 1~3, 0=미선택
    unsigned char member_ready[ROOM_MAX_PLAYER]; // 1=준비완료, 0=미준비
};  // 1+1+4+4+1+24+120+6+6+6 = 173B

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
    unsigned char  size;
    char           type;
    int            id;
    unsigned short keyInput;    // KEY_W~KEY_MOUSE_LB 비트 플래그
    unsigned int   timestamp;
    float          worldMatrix[16];
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
    unsigned char player_teams[ROOM_MAX_PLAYER]; // 0=RED, 1=BLUE, 0xFF=미선택
    unsigned char player_slots[ROOM_MAX_PLAYER]; // 1~3, 0=미선택
    char    auth_token[32];
};

// Phase 1·2: 게임 상태머신 동기화 패킷 (서버 → 클라)
constexpr char SC_PHASE_CHANGE = 20;   // 페이즈 전환
constexpr char SC_ROSTER_INFO  = 37;   // CHARSELECT 진입 시 플레이어 로스터
constexpr char SC_ROUND_START  = 21;   // 라운드 시작 (타이머 동기화)
constexpr char SC_ROUND_END    = 22;   // 라운드 종료 (승패 + 점수)
constexpr char SC_SCORE_UPDATE = 33;   // K/D/A + 팀 점수 브로드캐스트
constexpr char SC_TIMER_SYNC   = 36;   // 페이즈 남은 시간 주기 전송 (0 = 만료 신호)
constexpr char SC_MAP_LOADED   = 38;   // 특정 플레이어 맵 로드 완료 브로드캐스트

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

// 인게임 페이즈 타이머 만료 알림 (클라 → 인스턴스 서버)
struct CS_PHASE_READY_PACKET {
    unsigned char size;
    char          type;   // CS_PHASE_READY
    unsigned char phase;  // 0=CHARSELECT 1=SCOREBOARD 2=SHOP
};

// CHARSELECT 진입 시 방 전원에게 플레이어 구성 전송 (인스턴스 서버 → 클라)
struct RosterEntry {
    int           player_id;
    char          name[NAME_SIZE];
    unsigned char team;   // 0=RED, 1=BLUE, 0xFF=미선택
    unsigned char slot;   // 1~3, 0=미선택
};

struct SC_ROSTER_INFO_PACKET {
    unsigned char size;
    char          type;           // SC_ROSTER_INFO
    unsigned char player_count;
    RosterEntry   players[ROOM_MAX_PLAYER];
};

// 페이즈 남은 시간 주기 전송 (인스턴스 서버 → 클라)
struct SC_TIMER_SYNC_PACKET {
    unsigned char size;
    char          type;     // SC_TIMER_SYNC
    unsigned int  time_ms;  // 남은 시간(ms). 0 = 타이머 만료 신호
};

// 맵 로드 완료 통보 (클라 → 인스턴스 서버)
struct CS_MAP_LOADED_PACKET {
    unsigned char size;
    char          type;   // CS_MAP_LOADED
    unsigned char slot;   // 발신자 평면 슬롯 (team*3 + (number-1), 0~5)
};

// 특정 플레이어 맵 로드 완료 (인스턴스 서버 → 클라)
struct SC_MAP_LOADED_PACKET {
    unsigned char size;
    char          type;   // SC_MAP_LOADED
    unsigned char slot;   // 로드 완료한 플레이어 슬롯 (0~5)
};

#pragma pack (pop)