# HelloDinner — 필요 서버 로직 & 기능 정리

> 최초 작성: 2026-06-20  
> 기준 코드: `Suhyeon` 브랜치 (커밋 73e5332)  
> 작성 목적: 클라이언트 전수 조사 결과를 토대로 멀티플레이어 완성을 위해 추가해야 할  
> **서버 로직 / 패킷 / 권위(authoritative) 처리**를 체계적으로 정리한다.

---

## 1. 현재 동기화 범위 (현황)

### 지금 네트워크로 동기화되는 것

| 항목 | 방식 |
|------|------|
| 플레이어 접속 / 퇴장 | `SC_ADD_PLAYER` / `SC_REMOVE_PLAYER` |
| 플레이어 이동 (XYZ 위치 + 회전) | `CS_MOVE` (20Hz) → 서버 재시뮬 → `SC_MOVE_PLAYER` 브로드캐스트 |
| 매치메이킹 (대기열·성공·리다이렉트) | `SC_MATCH_WAIT` / `SC_MATCH_SUCCESS` / `SC_REDIRECT` |
| 로그인 / 인스턴스 입장 | `CS_LOGIN`, `CS_JOIN_ROOM`, `SC_LOGIN_INFO` |

### 지금 동기화되지 않는 것 (문제)

현재 `Level_Gameplay::Initialize()`에서 **`EnableOfflineMode()`를 호출하고
네트워크 이벤트 처리 코드가 전부 주석 처리**되어 있다(`Level_Gameplay.cpp:34`, `:85-110`).  
즉 실제 게임플레이의 거의 모든 의사결정이 **클라이언트별로 독립적으로** 이루어진다.

---

## 2. 미동기화 기능 전수 목록

| # | 기능 | 현재 클라이언트 파일:라인 | 현재 문제 | 서버화 필요 이유 |
|---|------|--------------------------|----------|----------------|
| 1 | **게임 페이즈/상태머신** | `Game_Manager.cpp:85-145` | 클라마다 독립 `CGame_Manager` 실행, 동기화 없음 | 클라가 각자 다른 페이즈에 있으면 게임 진행 불일치 |
| 2 | **라운드 타이머** | `Game_Manager.cpp:350-355` (`m_fRoundTimer`, 100s) | 클라 로컬 카운트다운, 부동소수점 누적으로 drift 발생 | 모든 클라가 정확히 같은 시점에 라운드 종료해야 함 |
| 3 | **라운드 승패 판정** | `Game_Manager.cpp:359-373` | 타임아웃 시 `End_Round(0)` (팀A 고정 더미), F9/F10 디버그키 | 실제 생존자 수 기반 판정을 서버가 내려야 치트 방지 |
| 4 | **팀 점수** | `Game_Manager.cpp:397-418` (`m_iTeamScore[2]`) | 로컬에서만 증가, 클라 간 동기화 없음 | 스코어보드가 클라마다 달라짐 |
| 5 | **팀 배정 / 6인 로스터** | `Game_Manager.cpp:448-489` (`Setup_DummyPlayers`) | 나머지 5명이 더미 객체, 실제 플레이어 데이터 없음 | 실제 이름·캐릭터·팀을 서버에서 배분해야 함 |
| 6 | **캐릭터 선택** | `Game_Manager.cpp:598-633` (`Handle_CharSelectClick`) | 클라에서만 결정, 서버/타 클라에 전달 안 됨 | 다른 플레이어가 내 캐릭터 선택을 알아야 올바른 모델 표시 가능 |
| 7 | **스폰/맵 선택** | `Game_Manager.cpp:1088-1107`, `MapSelect.cpp:177-241` | 로컬에서 선택 후 `Launch_To` 호출, 서버 검증 없음 | 스폰 위치 조작(맵 밖, 유리한 위치) 방지 필요 |
| 8 | **발사(Shoot)** | `Player_1rd.cpp:576-584` | 애니메이션·탄약 감소만, 투사체·레이캐스트·데미지 없음 | 히트 판정은 반드시 서버 권위여야 에임봇 방지 |
| 9 | **HP / 데미지** | `Player_1rd.h:80` (`m_iHealth`, 미사용 死코드) | `TakeDamage` 주석 처리, HP 갱신 로직 없음 | HP는 서버가 관리해야 클라 조작 불가 |
| 10 | **사망 / 리스폰** | `Player_1rd.cpp:586-591` (`Die()`, 호출 없음) | `Die()` 트리거 없음, 리스폰 로직 없음 | 사망·부활 이벤트를 서버가 브로드캐스트해야 함 |
| 11 | **탄약 / 리로드** | `Player_1rd.cpp:564-583`, `Controller.cpp:69-74` | 클라 로컬 카운트(`m_iAmmo=30`), 리로드 R키도 패킷 미포함 | 무한 탄약 치트 방지를 위해 서버 탄약 관리 필요 |
| 12 | **무기 교체** | `Player_1rd.cpp:593-609` (`Set_Weapon`) | 비용 차감 없이 로컬 적용, 타 클라에 전파 안 됨 | 무기 종류가 다르면 원격 플레이어 모델 표시 오류 |
| 13 | **상점 / 경제** | `Game_Manager.cpp:907-1006`, `Game_Manager.h:43` (`iMoney=800`) | `USE_SHOP=false`, 구매 시 금액 차감 없음 | 서버가 자금 검증 없이 구매 허용하면 무제한 무기 획득 가능 |
| 14 | **K/D/A 통계** | `Game_Manager.cpp:478-481` | 하드코딩 더미(`iKill=0, iDeath=0`) | 실제 킬/데스를 서버에서 집계해 브로드캐스트해야 함 |
| 15 | **생존자 HUD** | `Game_Manager.cpp:883-886` | `bAlive=true` 하드코딩, 실제 사망 반영 안 됨 | 서버에서 생존자 상태 수신 후 업데이트 필요 |
| 16 | **맵 로드 동기화** | `Game_Manager.cpp:379-440` (`Notify_MapLoaded`) | 스캐폴딩만 존재, 3초 타이머로 무조건 넘어감 | 모든 플레이어가 로드 완료해야 라운드 시작 가능 |
| 17 | **접속 해제 처리** | `protocol.h:62-65` (`CS_LOGOUT`) | 패킷 정의만 있고 클라가 전송하지 않음 | 정상 퇴장 시 `CS_LOGOUT` 발송, 서버 룸 정리 필요 |

---

## 3. 기능별 필요 서버 로직 & 패킷 설계

각 항목은 **(a) 서버 권위 상태, (b) 신규 패킷 제안, (c) 클라 연동 지점, (d) 치트 방지**로 구성한다.

---

### 3-A. 게임 페이즈 / 상태머신 동기화

#### 현재 상태
- `CGame_Manager`가 클라마다 독립적으로 `PHASE_CHARSELECT → SCOREBOARD → SHOP → PLAYING → GAMEOVER` 전환.
- `GAME_PHASE` enum: `Game_Manager.h:24-32`

#### 서버 권위 상태
인스턴스 서버가 **룸별 `GamePhase`** 와 **`iRound`(1~10)** 를 소유한다.  
클라이언트는 로컬 타이머/입력으로 페이즈 전환을 제안하고, 서버가 확정·브로드캐스트한다.

#### 신규 패킷

```
// Server → Client: 페이즈 변경 통지
SC_PHASE_CHANGE = 20
struct SC_PHASE_CHANGE_PACKET {
    unsigned char size;
    char          type;           // SC_PHASE_CHANGE
    unsigned char phase;          // 0=CHARSELECT 1=SCOREBOARD 2=SHOP 3=PLAYING 4=GAMEOVER
    unsigned char round;          // 현재 라운드 번호 (1~10)
};  // 4바이트

// Client → Server: 페이즈 전환 요청 (선택적 — 서버가 자체 타이머로 전환해도 됨)
CS_PHASE_REQUEST = 20
struct CS_PHASE_REQUEST_PACKET {
    unsigned char size;
    char          type;           // CS_PHASE_REQUEST
    unsigned char requested_phase;
};  // 3바이트
```

#### 클라 연동 지점
- `CGame_Manager::Enter_Phase()` (`Game_Manager.cpp:129`) 진입 시 서버 패킷으로 동기화.
- `NetworkClient::ProcessInstancePacket`에 `SC_PHASE_CHANGE` 핸들러 추가.
- `CController::Apply_ServerEvents`에서 `pGameManager->Enter_Phase(phase)` 호출.

#### 치트 방지
서버 타이머가 권위를 가지므로 클라가 F9/F10 디버그키(`Game_Manager.cpp:362-371`)를 눌러도
서버 확정 없이 실제 라운드 종료가 발생하지 않는다. 최종적으로 디버그 키는 제거한다.

---

### 3-B. 라운드 타이머 동기화

#### 현재 상태
- `m_fRoundTimer`(100s) 로컬 카운트다운 → 0이 되면 `End_Round(0)` (`Game_Manager.cpp:347-374`).

#### 서버 권위 상태
인스턴스 서버가 라운드 타이머를 소유하고 **`SC_ROUND_START`** 에 종료 절대시각(서버 timestamp)을 포함.  
클라는 서버 시각 기준으로 표시만 하고, 타임아웃 이벤트를 서버가 `SC_ROUND_END`로 발송한다.

#### 신규 패킷

```
// Server → Client: 라운드 시작 (타이머 동기화 포함)
SC_ROUND_START = 21
struct SC_ROUND_START_PACKET {
    unsigned char  size;
    char           type;           // SC_ROUND_START
    unsigned char  round;          // 1~10
    unsigned int   duration_ms;    // 라운드 길이 (ms), 기본 100000
    unsigned int   server_time_ms; // 서버 현재 시각 (ms, 세션 기준)
};  // 11바이트

// Server → Client: 라운드 종료
SC_ROUND_END = 22
struct SC_ROUND_END_PACKET {
    unsigned char size;
    char          type;        // SC_ROUND_END
    unsigned char winner_team; // 0=팀A, 1=팀B, 2=무승부
    unsigned char score_a;     // 팀A 현재 누적 점수
    unsigned char score_b;     // 팀B 현재 누적 점수
};  // 5바이트
```

#### 클라 연동 지점
- `SC_ROUND_START` 수신 시 `CGame_Manager::Start_Round(round, duration_ms, server_time_ms)` 호출.
  남은 시간 = `(server_time_ms + duration_ms) - 현재클라시각`.
- `SC_ROUND_END` 수신 시 `CGame_Manager::End_Round(winner_team)` 호출.
  `Game_Manager.cpp:397-418`의 기존 `End_Round` 시그니처와 연결.

---

### 3-C. 팀 배정 / 6인 로스터 동기화

#### 현재 상태
- `Setup_DummyPlayers` (`Game_Manager.cpp:448-489`)가 나머지 5명을 더미로 채움.
- `IS_ROOM_NOTIFY` (`protocol.h:151`) 에 `player_ids[6]`와 `player_names[6][20]`이 이미 포함되어 있음.

#### 서버 권위 상태
인스턴스 서버가 `CS_JOIN_ROOM` 응답 또는 `SC_PHASE_CHANGE(CHARSELECT)` 시점에  
**팀 배정(slot 0~5, team A/B, 번호 1~3)과 실제 이름**을 클라에 전송한다.

#### 신규 패킷

```
// Server → Client: 룸 로스터 전달
SC_ROOM_ROSTER = 23
struct PlayerSlotInfo {
    int           player_id;
    unsigned char slot;      // 0~5 (0~2=팀A, 3~5=팀B)
    unsigned char team;      // 0=팀A, 1=팀B
    unsigned char number;    // 팀 내 번호 1~3
    char          name[20];
};  // 27바이트
struct SC_ROOM_ROSTER_PACKET {
    unsigned char  size;
    char           type;              // SC_ROOM_ROSTER
    unsigned char  player_count;      // 실제 인원 (최대 6)
    PlayerSlotInfo slots[6];          // 6명 × 27 = 162바이트
};  // 165바이트 (≤255, 안전)
```

#### 클라 연동 지점
- `CGame_Manager::Setup_DummyPlayers` 대신 `Setup_RealPlayers(roster)`로 교체.
- `m_iMyTeam`, `m_iMyNumber`, `m_vMySpot` 등을 서버 응답값으로 설정.

---

### 3-D. 캐릭터 선택 동기화

#### 현재 상태
- `Handle_CharSelectClick` (`Game_Manager.cpp:598-633`)에서 로컬만 선택, 타 클라 전파 없음.
- 타임아웃 시 `Force_StartPlaying`이 강제로 Pig 선택 (`Game_Manager.cpp:1174`).

#### 신규 패킷

```
// Client → Server: 캐릭터 선택 확정
CS_SELECT_CHARACTER = 21
struct CS_SELECT_CHARACTER_PACKET {
    unsigned char size;
    char          type;      // CS_SELECT_CHARACTER
    unsigned char character; // 0=Pig, 1=Chick
};  // 3바이트

// Server → Client: 특정 플레이어의 캐릭터 선택 브로드캐스트
SC_CHARACTER_SELECTED = 24
struct SC_CHARACTER_SELECTED_PACKET {
    unsigned char size;
    char          type;      // SC_CHARACTER_SELECTED
    int           player_id;
    unsigned char character; // 0=Pig, 1=Chick
};  // 7바이트
```

#### 클라 연동 지점
- `Handle_CharSelectClick`에서 Ready 클릭 시 `CS_SELECT_CHARACTER` 발송.
- `SC_CHARACTER_SELECTED` 수신 시 해당 슬롯의 `PLAYER_STAT.character` 갱신 + 원격 플레이어 모델 교체.
- 타임아웃 시 서버가 미선택 플레이어에게 기본값(Pig)으로 `SC_CHARACTER_SELECTED` 발송.

---

### 3-E. 스폰 / 맵 선택 동기화

#### 현재 상태
- `CMapSelect::Get_SelectedWorld()`로 로컬 픽셀 → 월드 변환 후 `Apply_SpawnLaunch` 호출 (`Game_Manager.cpp:1088-1107`).
- 스폰 위치 검증 없음.

#### 신규 패킷

```
// Client → Server: 스폰 위치 선택
CS_SELECT_SPAWN = 22
struct CS_SELECT_SPAWN_PACKET {
    unsigned char size;
    char          type;   // CS_SELECT_SPAWN
    float         world_x;
    float         world_z;
};  // 10바이트

// Server → Client: 확정된 스폰 위치 통보 (검증 후)
SC_SPAWN_ASSIGN = 25
struct SC_SPAWN_ASSIGN_PACKET {
    unsigned char size;
    char          type;      // SC_SPAWN_ASSIGN
    int           player_id;
    float         world_x;
    float         world_y;   // 서버가 Y 높이 보정
    float         world_z;
};  // 18바이트
```

#### 클라 연동 지점
- `Apply_SpawnLaunch` (`Game_Manager.cpp:1088`) 에서 직접 `Launch_To` 호출하는 대신
  `CS_SELECT_SPAWN` 발송 → `SC_SPAWN_ASSIGN` 수신 후 `Launch_To(world_x, world_y, world_z)` 호출.
- 서버는 맵 바운더리(BVH) 기반으로 클라가 요청한 위치가 유효한지 검증.

---

### 3-F. 발사 (Shoot) & 히트 판정

#### 현재 상태
- `Shoot()` (`Player_1rd.cpp:576-584`): 애니메이션 재생 + `--m_iAmmo` 만 실행.
- 투사체·레이캐스트·히트박스 질의 전혀 없음.
- `Controller.cpp:74`에서 좌클릭 시 로컬 `Shoot()` 호출하지만 `Build_KeyBitFlags`에 미포함.

#### 서버 권위 상태
- 클라가 **발사 이벤트** (발사 시점의 카메라 위치·방향)를 서버에 전송.
- 서버가 **권위적 플레이어 위치 + per-bone 히트박스**를 기준으로 히트 여부 판정.
- 결과를 발사자에게 `SC_HIT`, 피격자에게 `SC_DAMAGE`/`SC_PLAYER_HP`로 전달.

#### 신규 패킷

```
// Client → Server: 발사
CS_FIRE = 23
struct CS_FIRE_PACKET {
    unsigned char size;
    char          type;        // CS_FIRE
    unsigned int  timestamp;   // 발사 시점 클라 시각
    float         origin[3];   // 카메라 위치 (x, y, z)
    float         direction[3];// 정규화된 발사 방향
    unsigned char weapon;      // 0=케첩건, 1=마요네즈건
};  // 30바이트

// Server → Client(발사자): 히트 확인
SC_HIT = 26
struct SC_HIT_PACKET {
    unsigned char size;
    char          type;        // SC_HIT
    int           target_id;   // 피격된 플레이어 id
    unsigned char damage;      // 실제 입힌 데미지
    unsigned char hitbox;      // 피격 부위 (0=Head, 1=Body, 2=Arm, 3=Leg)
};  // 8바이트

// Server → Client(피격자 포함 룸 전체): HP 갱신
SC_PLAYER_HP = 27
struct SC_PLAYER_HP_PACKET {
    unsigned char size;
    char          type;      // SC_PLAYER_HP
    int           player_id;
    unsigned char hp;        // 갱신된 HP (0~100)
};  // 7바이트
```

#### 클라 연동 지점
- `CController::Input_Player` (`Controller.cpp:64-75`)에서 LMB 감지 시 `CS_FIRE` 발송.
  로컬에서는 애니메이션만 재생(`Shoot(0)` 그대로), 데미지는 서버 응답 후 처리.
- `SC_PLAYER_HP` 수신 시 해당 플레이어의 `m_iHealth` 갱신.
  로컬 플레이어면 HUD 체력 표시 갱신; 원격 플레이어면 `CPlayer_Pig` HP 필드 갱신.

#### 치트 방지
- 서버가 최근 `SC_MOVE_PLAYER` 기반 권위 위치로 히트박스를 재구성하여 검증.
- 발사 timestamp로 최대 허용 랙(예: 200ms) 초과 시 무효 처리.
- per-bone 히트박스(`Player_1rd.cpp:663-850`)는 이미 생성되어 있으므로 서버에도 동일 구조 복사 후 활용.

---

### 3-G. HP / 데미지 관리

#### 현재 상태
- `m_iHealth` (`Player_1rd.h:80`, `Player_Pig.h:45`): 선언만 있고 초기화·사용 전혀 없음.
- `TakeDamage`: 주석 처리(`Player_1rd.h:35`, `Player_Pig.h:33`).

#### 서버 권위 상태
- 인스턴스 서버의 `GameSession`에 `int hp` 필드 추가 (기본 100).
- `CS_FIRE` 처리 시 히트 확인 → `hp -= damage` → `SC_PLAYER_HP` 브로드캐스트.
- `hp <= 0`이면 즉시 `SC_PLAYER_DIE` 발송.

#### 클라 연동 지점
- `TakeDamage` 주석 해제 후 `SC_PLAYER_HP` 수신 핸들러에서 호출.
- 로컬 플레이어: HUD 체력 바 갱신. 원격 플레이어: `CPlayer_Pig::m_iHealth` 갱신.

---

### 3-H. 사망 & 리스폰

#### 현재 상태
- `Die()` (`Player_1rd.cpp:586`): 호출 코드 없음. 리스폰 로직 없음.
- `bAlive` HUD: 하드코딩 `true` (`Game_Manager.cpp:885`).

#### 신규 패킷

```
// Server → Client: 플레이어 사망
SC_PLAYER_DIE = 28
struct SC_PLAYER_DIE_PACKET {
    unsigned char size;
    char          type;       // SC_PLAYER_DIE
    int           player_id;  // 사망한 플레이어
    int           killer_id;  // 킬한 플레이어 (-1=낙사 등)
    unsigned char weapon;     // 사용 무기
};  // 12바이트

// Server → Client: 리스폰 (라운드 중 부활 있는 경우)
SC_RESPAWN = 29
struct SC_RESPAWN_PACKET {
    unsigned char size;
    char          type;       // SC_RESPAWN
    int           player_id;
    float         spawn_x;
    float         spawn_y;
    float         spawn_z;
    unsigned char hp;         // 리스폰 시 HP (기본 100)
};  // 22바이트
```

#### 클라 연동 지점
- `SC_PLAYER_DIE` 수신:
  - 해당 플레이어가 로컬 플레이어면 `CPlayer_1rd::Die()` 호출.
  - 원격 플레이어면 `CPlayer_Pig::Die()` 호출.
  - `CGame_Manager`의 생존자 HUD 업데이트 (`bAlive = false`).
  - K/D/A: `killer_id` 플레이어 kill+1, 사망 플레이어 death+1.
- `SC_RESPAWN` 수신:
  - `CPlayer_1rd::Revive()` (신규 함수) 또는 `Launch_To(spawn_xyz)`.
  - HUD `bAlive = true`로 복구.

---

### 3-I. 탄약 & 리로드

#### 현재 상태
- `m_iAmmo = 30`, 로컬 감소. `Reload()` (`Player_1rd.cpp:564`): 애니 완료 시 `m_iAmmo = 30` 복구.
- `CS_MOVE`의 `keyInput` 비트플래그에 R(리로드) 미포함 (`Controller.cpp:118-128`).

#### 서버 권위 상태
서버 `GameSession`에 `int ammo` 필드 추가.  
발사(`CS_FIRE`) 처리 시 서버도 탄약 감소 → 0이면 발사 거부 후 `SC_AMMO_UPDATE` 전송.  
리로드 완료 시 서버가 탄약 복구 및 `SC_AMMO_UPDATE` 전송.

#### 신규 패킷

```
// Client → Server: 리로드 요청
CS_RELOAD = 24
struct CS_RELOAD_PACKET {
    unsigned char size;
    char          type;   // CS_RELOAD
    unsigned char weapon; // 0 or 1
};  // 3바이트

// Server → Client: 탄약 상태 갱신
SC_AMMO_UPDATE = 30
struct SC_AMMO_UPDATE_PACKET {
    unsigned char size;
    char          type;      // SC_AMMO_UPDATE
    int           player_id;
    unsigned char ammo;      // 현재 탄약
    unsigned char max_ammo;  // 최대 탄약
};  // 8바이트
```

#### 클라 연동 지점
- `CController::Input_Player` R키 감지 시 `CS_RELOAD` 발송 + 로컬 애니 재생.
- `SC_AMMO_UPDATE` 수신 시 `m_iAmmo` 갱신, HUD 탄약 표시 업데이트.

---

### 3-J. 무기 교체 (상점 구매 포함)

#### 현재 상태
- `Set_Weapon(i)` (`Player_1rd.cpp:593`): 비용 차감 없이 로컬만 변경.
- `USE_SHOP=false`로 상점 자체가 꺼져 있음 (`Game_Manager.h:229`).

#### 서버 권위 상태
- 서버 `GameSession`에 `int equipped_weapon`, `int money` 보관.
- 클라가 `CS_BUY_WEAPON` 요청 → 서버가 자금 검증 → 성공 시 `SC_WEAPON_CHANGED` + `SC_MONEY_UPDATE` 브로드캐스트.

#### 신규 패킷

```
// Client → Server: 무기 구매/교체 요청
CS_BUY_WEAPON = 25
struct CS_BUY_WEAPON_PACKET {
    unsigned char size;
    char          type;         // CS_BUY_WEAPON
    unsigned char weapon_slot;  // 0=케첩건, 1=마요네즈건
};  // 3바이트

// Server → Client: 무기 교체 확정 브로드캐스트
SC_WEAPON_CHANGED = 31
struct SC_WEAPON_CHANGED_PACKET {
    unsigned char size;
    char          type;        // SC_WEAPON_CHANGED
    int           player_id;
    unsigned char weapon_slot;
};  // 7바이트

// Server → Client: 자금 갱신
SC_MONEY_UPDATE = 32
struct SC_MONEY_UPDATE_PACKET {
    unsigned char size;
    char          type;      // SC_MONEY_UPDATE
    int           player_id;
    unsigned short money;    // 현재 보유 자금
};  // 8바이트
```

#### 클라 연동 지점
- `Handle_ShopClick` (`Game_Manager.cpp:951`)에서 직접 `Set_Weapon` 대신 `CS_BUY_WEAPON` 발송.
- `SC_WEAPON_CHANGED` 수신 시 해당 플레이어 `Set_Weapon` 호출(원격 플레이어도 모델 교체).
- `SC_MONEY_UPDATE` 수신 시 `PLAYER_STAT::iMoney` 갱신, 스코어보드/HUD 반영.

---

### 3-K. 점수 / K·D·A 통계 동기화

#### 현재 상태
- `m_iTeamScore[2]`, `iKill`, `iDeath`, `iAssist` 모두 하드코딩 더미 (`Game_Manager.cpp:478-481`).

#### 신규 패킷

```
// Server → Client: 팀/개인 점수 브로드캐스트
SC_SCORE_UPDATE = 33
struct PlayerStatBrief {
    int           player_id;
    unsigned char kills;
    unsigned char deaths;
    unsigned char assists;
    unsigned short money;
};  // 9바이트
struct SC_SCORE_UPDATE_PACKET {
    unsigned char    size;
    char             type;       // SC_SCORE_UPDATE
    unsigned char    score_a;    // 팀A 라운드 승수
    unsigned char    score_b;    // 팀B 라운드 승수
    unsigned char    player_count;
    PlayerStatBrief  stats[6];   // 6명 × 9 = 54바이트
};  // 60바이트 (≤255, 안전)
```

#### 클라 연동 지점
- `SC_ROUND_END` 또는 `SC_PHASE_CHANGE(SCOREBOARD)` 직후 `SC_SCORE_UPDATE` 수신.
- `CGame_Manager::m_vStats` 갱신 → 스코어보드 UI 재빌드.
- `m_iTeamScore[2]` 갱신 → 상단 점수 패널.

---

### 3-L. 맵 로드 완료 동기화

#### 현재 상태
- `Notify_MapLoaded` / `Force_AllLoaded` / `Are_AllLoaded` 스캐폴딩 있음 (`Game_Manager.cpp:379-440`).
- 스코어보드 전환은 로컬 3초 타이머로 처리 (`SCOREBOARD_AUTO=3s`).

#### 신규 패킷

```
// Client → Server: 맵 로드 완료 신호
CS_MAP_LOADED = 26
struct CS_MAP_LOADED_PACKET {
    unsigned char size;
    char          type; // CS_MAP_LOADED
};  // 2바이트

// Server → Client: 전원 로드 완료 → 다음 페이즈 진행 허가
// (SC_PHASE_CHANGE로 대체 가능 — 별도 패킷 불필요)
```

#### 클라 연동 지점
- `CGame_Manager::Notify_MapLoaded` 호출 시 `CS_MAP_LOADED` 발송.
- 서버가 룸 전원의 `CS_MAP_LOADED` 수신 확인 후 `SC_PHASE_CHANGE(PLAYING)` 발송.
- 타임아웃(예: 30s) 이후엔 서버가 강제로 페이즈 전환.

---

### 3-M. 접속 해제(CS_LOGOUT) 실사용

#### 현재 상태
- `CS_LOGOUT_PACKET` 정의(`protocol.h:62-65`)만 있고 클라가 전송하지 않음.
- 서버는 `recv <= 0`(소켓 끊김) 시에만 정리 수행.

#### 개선
- 앱 종료 / 레벨 전환 시 `CS_LOGOUT` 발송 후 소켓 close.
- 서버 `CS_LOGOUT` 핸들러에서 `SC_REMOVE_PLAYER` 브로드캐스트 + 룸 정리 (기존 disconnect 로직과 동일).

---

## 4. 프로토콜 확장 시 주의사항

### 4-1. 1바이트 size framing 한계

현재 recv 루프는 `p[0]`을 패킷 크기로 읽는다 (`NetworkClient.cpp`, `InstanceServer::WorkerThread`).  
이 구조는 **최대 255바이트** 패킷만 지원한다.

- 본 문서의 신규 패킷은 모두 255바이트 이하로 설계했다.
- 향후 대형 패킷(예: 전체 맵 상태 스냅샷)이 필요하면 framing을 **2바이트 `uint16_t` size**로 교체해야 한다.
  recv 루프 내 `p[0]` 참조를 `*(uint16_t*)p` 로 변경하고, 기존 패킷들의 `size` 필드도 2바이트로 확장.

### 4-2. keyInput 비트플래그 확장 필요

현재 `CS_MOVE_PACKET.keyInput`은 6비트(W/S/A/D/SPACE/CTRL)만 사용한다 (`Controller.cpp:118-128`).  
원격 플레이어의 발사/리로드/달리기 애니메이션을 `SC_MOVE_PLAYER` 하나로 전달하려면 비트 확장 필요.

```
기존  : 0b00CSADWX (상위 2비트 미사용)
확장안: 0b RFSADWX  (R=Reload, F=Fire/Shoot, S=Sprint 등 상위 비트 활용)
```

단, 발사는 별도 `CS_FIRE` 패킷이 있으므로 `keyInput`의 F비트는 애니메이션 동기화 전용으로만 사용.

### 4-3. 패킷 type 번호 충돌 방지

현재 `SC_*`와 `CS_*`가 같은 `char type` 바이트를 공유한다 (CS_LOGIN=0, SC_LOGIN_INFO=0 등).  
송수신 방향이 다르므로 실제 충돌은 없지만, 코드 가독성을 위해 신규 패킷은 **양방향 통틀어 겹치지 않는 값**으로 할당한다.  
본 문서의 신규 패킷은 20번부터 시작하여 기존 0~12 범위와 겹치지 않는다.

### 4-4. 오프라인 모드 폴백

기존 `IsInGame()` = `m_bInGame || m_bOfflineMode` 패턴을 유지해  
신규 패킷 발송 함수에 `if (!m_bInGame) return;` 가드를 넣으면 오프라인 테스트가 그대로 동작한다.  
오프라인 시엔 `EnableOfflineMode()` + 로컬 로직으로 단독 테스트 가능하도록 유지한다.

### 4-5. BUF_SIZE 조정

현재 `BUF_SIZE = 200` (`protocol.h:6`).  
`SC_ROOM_ROSTER`(165바이트), `SC_SCORE_UPDATE`(60바이트) 등 대형 패킷 추가 시  
recv 임시 버퍼(`recvBuf`)가 충분한지 확인하고 필요 시 **512 또는 1024로 확대**한다.

---

## 5. 우선순위 로드맵

> 하위 Phase는 상위 Phase 완료 후 착수한다.

### Phase 1 — 게임 상태머신 서버 일원화 *(최우선)*

**목표:** 여러 클라이언트가 같은 페이즈·라운드·타이머에서 동작하도록 한다.

| 작업 | 관련 패킷 | 관련 파일 |
|------|-----------|-----------|
| 인스턴스 서버에 `RoomPhaseManager` 추가 | — | `InstanceServer/GameSessionManager.cpp` |
| `SC_PHASE_CHANGE` 발송 및 클라 수신 처리 | `SC_PHASE_CHANGE` (20) | `NetworkClient.cpp`, `Game_Manager.cpp` |
| `SC_ROUND_START` / `SC_ROUND_END` 구현 | (21)(22) | `GameSessionManager.cpp`, `Game_Manager.cpp` |
| `SC_SCORE_UPDATE` 브로드캐스트 | (33) | `GameSessionManager.cpp`, `Game_Manager.cpp` |
| 로컬 타이머 / F9·F10 디버그 키 제거 | — | `Game_Manager.cpp:362-371` |

**검증:** 서버 + 클라 2개 실행 → 양쪽이 같은 시점에 페이즈 전환, 라운드 타이머 동일 표시 확인.

---

### Phase 2 — 로스터 & 선택 동기화

**목표:** 실제 6인이 같은 팀 구성·캐릭터·스폰 위치로 인게임에 진입한다.

| 작업 | 관련 패킷 | 관련 파일 |
|------|-----------|-----------|
| `SC_ROOM_ROSTER` 발송 및 `Setup_RealPlayers` 구현 | (23) | `GameSessionManager.cpp`, `Game_Manager.cpp` |
| `CS_SELECT_CHARACTER` + `SC_CHARACTER_SELECTED` 구현 | (21)(24) | `Controller.cpp`, `NetworkClient.cpp`, `Game_Manager.cpp` |
| `CS_MAP_LOADED` + 전원 완료 후 `SC_PHASE_CHANGE(PLAYING)` | (26)(20) | 위 동일 |
| `CS_SELECT_SPAWN` + `SC_SPAWN_ASSIGN` 구현 | (22)(25) | `MapSelect.cpp`, `Game_Manager.cpp` |

**검증:** 2인 매칭 후 각자 다른 캐릭터·스폰 선택 → 양측 화면에 올바른 모델/위치 표시 확인.

---

### Phase 3 — 전투 핵심 (발사·HP·사망·리스폰)

**목표:** 상대를 실제로 죽이고 리스폰할 수 있다.

| 작업 | 관련 패킷 | 관련 파일 |
|------|-----------|-----------|
| `CS_FIRE` 클라 발송 (`CController`, LMB) | (23) | `Controller.cpp` |
| 서버 히트스캔 (권위 위치 기반 레이-히트박스 검사) | — | `GameSessionManager.cpp`, 신규 `HitDetection.h` |
| `SC_HIT`, `SC_PLAYER_HP` 브로드캐스트 | (26)(27) | 위 동일 |
| 클라 HP HUD, `TakeDamage` 활성화 | — | `Player_1rd.cpp`, `Player_Pig.cpp`, HUD |
| `SC_PLAYER_DIE`, `SC_RESPAWN` 구현 | (28)(29) | `GameSessionManager.cpp`, `Player_1rd.cpp` |
| 생존자 HUD (`bAlive`) 실 연동 | — | `Game_Manager.cpp:885` |

**검증:** 2인 교전 → 한 명이 30발 모두 맞으면 사망 처리, 상대 화면에도 동일하게 표시 확인.

---

### Phase 4 — 탄약/리로드, 무기 교체 동기화

**목표:** 탄약이 서버에서 관리되어 무제한 발사 불가. 무기 교체 시 상대 화면에도 반영.

| 작업 | 관련 패킷 | 관련 파일 |
|------|-----------|-----------|
| 서버 `GameSession.ammo` 필드 추가 | — | `GameSession.h` |
| `CS_RELOAD` 발송 및 서버 탄약 복구 + `SC_AMMO_UPDATE` | (24)(30) | `Controller.cpp`, `GameSessionManager.cpp` |
| `SC_WEAPON_CHANGED` 원격 플레이어 모델 교체 | (31) | `Controller.cpp`, `Player_Pig.cpp` |

**검증:** 탄약 소모 후 서버 거부 확인(발사 무시), 리로드 후 탄약 복구 확인.

---

### Phase 5 — 상점/경제, K·D·A 통계 서버 권위화

**목표:** 돈이 실제로 소비되고, 스코어보드가 서버 집계 데이터를 표시한다.

| 작업 | 관련 패킷 | 관련 파일 |
|------|-----------|-----------|
| 서버 `GameSession.money` 초기화·검증 | — | `GameSession.h`, `GameSessionManager.cpp` |
| `CS_BUY_WEAPON` + `SC_WEAPON_CHANGED` + `SC_MONEY_UPDATE` | (25)(31)(32) | 위 동일, `Game_Manager.cpp` |
| `USE_SHOP = true` 로 상점 재활성화 | — | `Game_Manager.h:229` |
| 서버 K/D/A 집계 → `SC_SCORE_UPDATE` 브로드캐스트 | (33) | `GameSessionManager.cpp` |

**검증:** 잔고 부족 시 구매 거부 확인, 스코어보드 K/D/A가 양쪽 클라에서 동일 확인.

---

## 7. 방 코드 기반 매칭 (수동 방)

> 구현 완료: 2026-06-20

### 목표

친구끼리 4자리 코드를 공유해 **원하는 사람들끼리** 같은 Room에 모인 뒤,
방장이 시작을 누르면 그 인원 그대로 인스턴스 서버로 핸드오프된다.
기존 자동 매칭(빠른 매칭)은 **유지하며 두 경로가 공존**한다.

### 핵심 설계 결정

- **방 코드:** 4자리 숫자 (1000~9999), 로비 전용 식별자
- **`code ≠ room_id`:** 코드는 로비 대기방용, `room_id`는 인스턴스 서버용 (별개)
- **자동 큐 옵트인:** 로그인 시 자동 큐 진입 제거 → `CS_QUICK_MATCH` 수신 시 큐 진입

### 추가된 패킷 (`Server/protocol.h`)

| ID | 패킷 | 방향 | 설명 |
|----|------|------|------|
| CS 5 | `CS_CREATE_ROOM` | 클라→로비 | 방 생성 요청 |
| CS 6 | `CS_JOIN_ROOM_CODE` | 클라→로비 | 코드로 대기방 입장 |
| CS 7 | `CS_START_GAME` | 클라→로비 | 방장 게임 시작 |
| CS 8 | `CS_LEAVE_ROOM` | 클라→로비 | 대기방 나가기 |
| CS 9 | `CS_QUICK_MATCH` | 클라→로비 | 빠른 매칭 옵트인 |
| SC 7 | `SC_ROOM_CREATED` | 로비→클라 | 발급된 코드 통보 |
| SC 8 | `SC_ROOM_JOIN_RESULT` | 로비→클라 | 입장 결과 (`ROOM_JOIN_RESULT`) |
| SC 9 | `SC_ROOM_UPDATE` | 로비→클라 | 멤버 목록 갱신 브로드캐스트 |

### 추가/수정된 파일

| 파일 | 변경 내용 |
|------|-----------|
| `Server/protocol.h` | 신규 패킷 ID/구조체 + `ROOM_JOIN_RESULT` enum |
| `Server/RoomManager.h/.cpp` | **신규** 대기방 레지스트리 싱글톤 |
| `Server/SessionManager.cpp` | 로그인 자동큐 제거, CS 핸들러 5종, disconnect 시 LeaveRoom |
| `Server/MatchManager.h/.cpp` | `LaunchRoom()` 추출 (자동/수동 공용 핸드오프) |
| `Server/SERVER.vcxproj` | RoomManager 파일 컴파일 등록 |
| `Server/pch.h` | `unordered_map`, `algorithm` 추가 |
| `HelloDinner/NetworkClient.h/.cpp` | 송신 5종, 수신 3종, 방 상태 필드 (`RoomSnapshot`) |
| `HelloDinner/HelloDinner.cpp` | RunFrontend: 방 만들기 시 서버 코드 대기 로직 |
| `HelloDinner/LobbyWindow.cpp` | 버튼→패킷 연결, 코드 입력 팝업 (`PromptRoomCode`) |
| `HelloDinner/RoomWindow.h/.cpp` | 서버 코드 표시, WM_TIMER 폴링, 멤버 목록 표시 |

### 검증 방법

1. `Server` → `InstanceServer` → `HelloDinner.exe` ×2 빌드/실행
2. **방 생성:** 클라A "방 만들기" → 서버 콘솔 `Created room XXXX` + 클라A 창에 코드 표시
3. **코드 입장:** 클라B "방 들어가기" → 코드 입력 → 양쪽 대기방에 2명 목록 동시 갱신
4. **오입력:** 없는 코드 → `RJR_NOT_FOUND` 메시지박스 / 가득 찬 방 → `RJR_FULL` 확인
5. **게임 시작:** 클라A(방장) "게임 시작" → 양쪽 모두 동일 인스턴스 Room에 접속
6. **빠른 매칭 회귀:** 클라 2개가 "게임 시작" → 기존 자동매칭 동작 확인
7. **나가기/끊김:** 대기방 나가기 / 강제 종료 시 남은 멤버 목록 갱신 확인

---

## 6. 검증 방법 (전체 공통)

1. **빌드 순서:** `Server` (LobbyServer) → `InstanceServer` → `HelloDinner` (클라이언트) 빌드.
2. **실행 순서:** LobbyServer.exe → InstanceServer.exe → HelloDinner.exe ×2 (각각 로컬 연결).
3. **매칭:** `MIN_MATCH_PLAYERS = 2` 조건 충족 → `SC_REDIRECT` 수신 후 인스턴스 서버 접속 확인.
4. **Phase별 검증 포인트**는 각 Phase 섹션의 "검증" 항목 참조.
5. **오프라인 테스트**: `Level_Gameplay::Initialize()`에서 `EnableOfflineMode()` 유지한 채로 단독 실행 시
   신규 기능이 크래시 없이 오프라인 폴백으로 동작하는지 확인.
6. **패킷 크기 검증**: 각 신규 패킷의 `sizeof()` 가 `size` 필드 값 및 255바이트 이하인지 `static_assert`로 컴파일 타임 검증 권장.
