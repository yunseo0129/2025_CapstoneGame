# HelloDinner 서버 개발 컨텍스트

졸업작품 멀티플레이어 게임 **HelloDinner**의 서버 파트 담당자를 위한 AI 협업 가이드.

---

## 프로젝트 역할

- **담당**: 서버 전체 (LobbyServer, InstanceServer, 프로토콜)
- **클라이언트 팀**과 패킷 구조 / 포트 / 행동 규약을 맞춰야 함
- 언어: C++ (IOCP, Winsock2, Windows 전용)

---

## 전체 구조

```
3-Tier 구조
┌─────────┐     CS_LOGIN / CS_MOVE        ┌─────────────┐
│ Client  │ ─────────────────────────────▶│ LobbyServer │  Port 4000  (클라↔로비)
│(DX12)   │ ◀─────────────────────────── │  SERVER.exe │  Port 3999  (로비↔인스턴스 내부)
└─────────┘  SC_REDIRECT (IP+Token)       └──────┬──────┘
                                                  │ IS_ROOM_NOTIFY
                 CS_JOIN_ROOM                     ▼
┌─────────┐ ─────────────────────────────▶┌──────────────────┐
│ Client  │                               │ InstanceServer   │  Port 5001~5005
│(DX12)   │ ◀───────────────────────────  │InstanceServer.exe│
└─────────┘  SC_ADD_PLAYER / SC_MOVE_PLAYER└──────────────────┘
```

---

## 파일 구조

### LobbyServer (`Server/`)
| 파일 | 역할 |
|------|------|
| `LobbyServer.h/.cpp` | IOCP 리스닝, 워커스레드, Accept |
| `SessionManager.h/.cpp` | 클라이언트 세션 배열 관리, 패킷 파싱 디스패치 |
| `Session.h/.cpp` | 세션 1개 (소켓, 상태, PlayerInfo, Recv/Send) |
| `MatchManager.h/.cpp` | 대기열 관리, 6인 매칭 시도 (`TryMatch`) |
| `InstanceManager.h/.cpp` | 인스턴스 서버 등록/Heartbeat/로드밸런싱 |
| `InstanceInfo.h` | 인스턴스 상태 구조체 (부하점수 포함) |
| `PlayerInfo.h` | 플레이어 정보 구조체 |
| `protocol.h` | **모든 패킷 구조체 + 상수 정의** (클라/서버 공유) |
| `OverllapedEXP.h/.cpp` | IOCP 확장 OVERLAPPED |

### InstanceServer (`InstanceServer/`)
| 파일 | 역할 |
|------|------|
| `InstanceServer.h/.cpp` | IOCP 리스닝, 로비 등록, Heartbeat 전송 |
| `GameSessionManager.h/.cpp` | 인게임 세션 관리, 패킷 파싱 |
| `GameSession.h/.cpp` | 인게임 세션 (CS_JOIN_ROOM, CS_MOVE 처리) |
| `Room.h/.cpp` | 방 단위 플레이어 묶음, 브로드캐스트 대상 |

---

## 포트 / 상수

```cpp
LOBBY_PORT        = 4000   // 클라 → 로비
INTERNAL_PORT     = 3999   // 인스턴스 → 로비 내부 등록/Heartbeat
INSTANCE_PORT_BASE = 5000  // 인스턴스는 5001, 5002, ... 사용
MAX_USER          = 4000
ROOM_MAX_PLAYER   = 6
MAX_INSTANCE_SERVERS = 5
```

---

## 패킷 프로토콜 (`protocol.h`)

### 클라 → 서버 (CS_*)
| ID | 구조체 | 설명 |
|----|--------|------|
| 0 | `CS_LOGIN_PACKET` | 로비 로그인 |
| 1 | `CS_MOVE_PACKET` | 이동 (keyInput 비트플래그 + mouseYaw + worldMatrix) |
| 2 | `CS_LOGOUT_PACKET` | 로그아웃 |
| 3 | `CS_JOIN_ROOM_PACKET` | 인스턴스 서버 입장 (room_id + auth_token) |

### 서버 → 클라 (SC_*)
| ID | 구조체 | 설명 |
|----|--------|------|
| 0 | `SC_LOGIN_INFO_PACKET` | 로그인 완료 (id + 초기 worldMatrix) |
| 1 | `SC_ADD_PLAYER_PACKET` | 플레이어 입장 알림 |
| 2 | `SC_REMOVE_PLAYER_PACKET` | 플레이어 퇴장 알림 |
| 3 | `SC_MOVE_PLAYER_PACKET` | 이동 브로드캐스트 (worldMatrix 포함) |
| 4 | `SC_MATCH_WAIT_PACKET` | 대기열 현황 |
| 5 | `SC_MATCH_SUCCESS_PACKET` | 매칭 완료 |
| 6 | `SC_REDIRECT_PACKET` | 인스턴스 서버 IP/Port + auth_token 전달 |

### 인스턴스 ↔ 로비 내부 (IS_*)
| ID | 구조체 | 설명 |
|----|--------|------|
| 10 | `IS_HEARTBEAT_PACKET` | 부하 정보 리포트 (3초마다) |
| 11 | `IS_REGISTER_PACKET` | 인스턴스 서버 등록 |
| 12 | `IS_ROOM_NOTIFY_PACKET` | 매칭된 방 정보 전달 (로비→인스턴스) |

### 키입력 비트플래그
```cpp
KEY_W = 0x01, KEY_S = 0x02, KEY_A = 0x04, KEY_D = 0x08
KEY_SPACE = 0x10, KEY_CTRL = 0x20
```

---

## 세션 상태 머신

```
ST_FREE → ST_ALLOC → ST_LOBBY → ST_INGAME
                        ↑
               CS_LOGIN 처리 후
```

---

## 연결 흐름

1. **인스턴스 서버 기동**: `InstanceServer.exe <id> <port> <lobby_ip>`  
   → 로비 3999 포트에 접속 후 `IS_REGISTER` 전송
2. **로비 서버 기동**: `SERVER.exe`  
   → 4000 포트 클라 수신, 3999 포트 인스턴스 수신
3. **클라이언트 로그인**: 4000 포트 접속 → `CS_LOGIN` → `SC_LOGIN_INFO` + `SC_MATCH_WAIT`
4. **매칭**: 6명 쌓이면 `TryMatch()` → 여유 인스턴스 선택 → `IS_ROOM_NOTIFY` 전송 → 클라에 `SC_REDIRECT`
5. **인게임**: 클라가 인스턴스 포트에 접속 → `CS_JOIN_ROOM` (auth_token 검증) → `SC_ADD_PLAYER` 브로드캐스트

---

## 로드밸런싱

`InstanceInfo::GetLoadScore()` — 플레이어 수 60% + CPU 사용률 40% 가중 합산.  
`InstanceManager::SelectBestInstance()`에서 alive 인스턴스 중 점수 최저 선택.

---

## 네트워킹 아키텍처 원칙

### 키 입력 릴레이 방식 (현재 채택)

```
클라이언트 A                 InstanceServer              클라이언트 A, B, C ...
─────────────────────────────────────────────────────────────────────────────
CS_MOVE (keyInput + worldMatrix)
        ──────────────────▶
                            서버: 물리 계산 후
                            SC_MOVE_PLAYER (keyInput 포함)
                                    ──────────────────▶  각 클라이언트:
                                                         keyInput 기반 로직·애니메이션 처리
```

**규칙:**
- **서버 역할**: 키 입력 수신 → 물리(중력/이동) 계산 → 위치 확정 → Room 전체에 `SC_MOVE_PLAYER` 브로드캐스트
- **클라이언트 역할**: 받은 `SC_MOVE_PLAYER.keyInput`을 기준으로 애니메이션·이펙트 등 로컬 처리
- 서버는 이벤트 발생 여부만 전달 — 이벤트에 대한 시각/음향 처리는 클라이언트가 담당

### 상승 에지(Rising Edge) 감지

단발성 액션(사격, 장전 등)은 **이전 keyInput과 현재 keyInput의 XOR**로 새로 눌린 키만 추출:

```cpp
// Controller::Apply_ServerEvents — 서버 에코 패킷 수신 시
unsigned short risingEdge = (~m_prevServerKeyInput) & evt.keyInput;
m_prevServerKeyInput = evt.keyInput;

if (risingEdge & KEY_MOUSE_LB)  pPlayer->Shoot();    // 1인칭 발사 애니메이션
if (risingEdge & KEY_R)         pPlayer->Reload();   // 1인칭 장전 애니메이션
```

### 이벤트 종류별 처리 위치

| 이벤트 | 서버 처리 | 클라이언트 처리 |
|--------|-----------|-----------------|
| 이동 (WASD) | 물리 계산, 위치 확정 | 예측 이동(Predict_Local) + 보정(Apply_ServerCorrection) |
| 발사 (LMB) | keyInput 기록·전달 | 상승 에지 감지 → 1인칭/3인칭 발사 애니메이션 |
| 장전 (R) | keyInput 기록·전달 | 상승 에지 감지 → 1인칭/3인칭 장전 애니메이션 |
| 페이즈 전환 | 타이머 관리, SC_PHASE_CHANGE 전송 | Apply_PhaseChange → UI 전환 |
| 스폰 발사 | SC_SPAWN_LAUNCH 전송 | Apply_SpawnLaunch_Server → Launch_To |

---

## 주의사항 / 알려진 제약

- 패킷 구조체는 `#pragma pack(push, 1)` 적용 — 멤버 순서/타입 변경 시 클라이언트 팀과 반드시 동기화
- `CS_MOVE_PACKET`에 `worldMatrix[16]` 포함 — 서버 권위적(Server Authoritative) 이동: 서버가 받은 worldMatrix를 그대로 브로드캐스트
- Input 장치 설정(`DISCL_BACKGROUND` 등)은 클라이언트 팀 담당 — 서버에서 임의 변경 금지
- 인스턴스 서버는 복수 실행 가능 (최대 5개), 각각 다른 포트 사용
- **클라이언트 로직을 서버로 이식할 때는 로직을 수정하지 않고 그대로 복사한다** — 동작을 검증하며 기존 클라이언트 로직과 완전히 동일한 결과가 보장된 후에만 리팩터링/최적화를 허용. 이식 과정에서 임의로 로직을 변경하면 클라↔서버 간 결과 불일치가 발생할 수 있음

---

## 실행 순서 (로컬 테스트)

```
# 1. 로비 서버
SERVER.exe

# 2. 인스턴스 서버 (별도 창)
InstanceServer.exe 0 5001 127.0.0.1
InstanceServer.exe 1 5002 127.0.0.1

# 3. 클라이언트 실행
HelloDinner.exe
```
