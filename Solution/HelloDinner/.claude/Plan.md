# 서버 네트워크 동기화 구현 계획

> 기준 문서: `GameManager_Network_Sync.md`
> 범위: InstanceServer — 인게임 라운드 진행 전체
> 의존 관계: 각 STEP은 앞 STEP이 완료되어야 다음 STEP을 진행할 수 있다.

---

## 진행 현황 요약

| STEP | 항목 | 상태 |
|------|------|------|
| 1 | 단계·타이머 동기화 | ✅ 완료 |
| 2 | 매치 로스터 | ✅ 완료 |
| 3 | 맵 로드 완료 | ✅ 완료 |
| 4 | 캐릭터 선택 릴레이 | ✅ 완료 |
| 5 | 스폰 위치 선택 릴레이 + 싱크대 스테이징 | ✅ 완료 |
| 6 | 전투 결과 (피격·사망·K/D/A) | ✅ 완료 (K/D는 미연동) |
| 7 | 라운드 승패 판정 (팀 전멸) | 🔲 미구현 |

---

## STEP 1 — 단계·타이머 동기화 ✅ 구현 완료

**목적:** 모든 클라이언트가 동일한 시점에 동일한 페이즈·타이머를 공유한다.

---

### 동작 흐름 (이벤트 순서)

```
[전원 CS_JOIN_ROOM 완료]
  → OnPlayerJoined() → joined == expected
  → TransitionTo(CHARSELECT)
  → SC_PHASE_CHANGE(0, round=1) 브로드캐스트
  → SC_TIMER_SYNC(30000) 즉시 전송

[클라 수신]
  → Apply_PhaseChange(phase=0, round=1) → Enter_Phase(CHARSELECT)
  → Apply_TimerSync(30000) → m_fSelectTimer = 30.0f
  → Tick_SelectTimer(dt) 로 로컬 카운트다운 + UI 표시

[서버 1초마다]
  → TickRoom(): sync_elapsed >= 1.0 → Broadcast_TimerSync
  → 클라 Apply_TimerSync(남은ms) → m_fSelectTimer 스냅

[선택 페이즈 타이머 만료 — 경로 A: 서버 타임아웃]
  → TickRoom(): timer_sec <= 0 → do_sync=true → Broadcast_TimerSync(0)
  → 클라 Apply_TimerSync(0) → m_bSelectExpired=true → Send_PhaseReady(0/1/2)
  → OnPlayerPhaseReady(): 전원 수신 → TransitionTo(다음 페이즈)
  → SC_PHASE_CHANGE 브로드캐스트 → 클라 Enter_Phase

[선택 페이즈 타이머 만료 — 경로 B: Ready 버튼 클릭 (CHARSELECT만)]
  → Handle_CharSelectClick() → m_bCSReady = true
  → Update_CharSelect(): m_bSelectExpired=false 시 Send_PhaseReady(0) 즉시 전송
  → OnPlayerPhaseReady(): 전원 수신 → TransitionTo(SCOREBOARD)

[PLAYING 페이즈]
  → TransitionTo(PLAYING) → Broadcast_RoundStart(duration_ms=100000)
  → 클라 Apply_RoundStart() → m_fRoundTimer = 100.0f
  → SC_TIMER_SYNC 1초 주기로 m_fRoundTimer 보정

[라운드 종료]
  → TickRoom(): PLAYING timer_sec<=0 → pending_winner=0 → OnRoundEnd(0)
  → Broadcast_RoundEnd + Broadcast_ScoreUpdate
  → TransitionTo(SCOREBOARD 또는 GAMEOVER)
  → 클라 Apply_RoundEnd() (점수만 업데이트)
  → 클라 Apply_PhaseChange() → Enter_Phase(다음)
```

---

### 구현 완료 — 서버

| 항목 | 파일 | 함수/라인 |
|------|------|-----------|
| WAITING→CHARSELECT 전환 | `RoomPhaseManager.cpp:115` | `OnPlayerJoined()` |
| 페이즈 전환 + 브로드캐스트 | `RoomPhaseManager.cpp:65` | `TransitionTo()` |
| SC_PHASE_CHANGE | `RoomPhaseManager.cpp:181` | `Broadcast_PhaseChange()` |
| SC_TIMER_SYNC 1초 주기 | `RoomPhaseManager.cpp:25` | `TickRoom()` — `sync_elapsed >= SYNC_INTERVAL` |
| SC_TIMER_SYNC(0) 만료 신호 | `RoomPhaseManager.cpp:45` | `TickRoom()` — `timer_sec <= 0` 시 `do_sync=true` |
| 타임아웃 중복 전송 방지 | `RoomPhaseManager.cpp:33` | `TickRoom()` early-return: `timer_sec <= 0.f` |
| CS_PHASE_READY 수신 처리 | `RoomPhaseManager.cpp:273` | `OnPlayerPhaseReady()` |
| 전원 준비 시 페이즈 전환 | `RoomPhaseManager.cpp:283` | `phase_ready_count >= joined` |
| SC_ROUND_START | `RoomPhaseManager.cpp:195` | `Broadcast_RoundStart()` |
| SC_ROUND_END + SC_SCORE_UPDATE | `RoomPhaseManager.cpp:136` | `OnRoundEnd()` |
| SC_SCORE_UPDATE K/D/A | `RoomPhaseManager.cpp:218` | `Broadcast_ScoreUpdate()` — 현재 kills/deaths/assists=0 더미 |
| CS_PHASE_READY 라우팅 | `GameSessionManager.cpp:116` | `case CS_PHASE_READY` → `OnPlayerPhaseReady()` |
| 패킷 구조체 | `Server/protocol.h:220~279` | SC_PHASE_CHANGE(20), SC_ROUND_START(21), SC_ROUND_END(22), SC_SCORE_UPDATE(33), SC_TIMER_SYNC(36), CS_PHASE_READY(13) |

### 구현 완료 — 클라이언트

| 항목 | 파일 | 함수/라인 |
|------|------|-----------|
| 인스턴스 패킷 수신 | `NetworkClient.cpp:448` | `ProcessInstancePacket()` — SC_PHASE_CHANGE~SC_TIMER_SYNC 케이스 |
| 이벤트 큐 → GM 처리 | `Game_Manager.cpp:88` | `Update()` — `IsInGame()` 시 `PopAllMatchEvents()` → `Apply_*` 직접 처리 |
| 서버 신호 전 PHASE_END 대기 | `Game_Manager.cpp:49` | `Start_Match()` — `Enter_Phase` 제거, `Apply_PhaseVisibility(PHASE_END)`로 모든 UI 숨김 대기 |
| 페이즈 전환 적용 | `Game_Manager.cpp:378` | `Apply_PhaseChange()` — phase 바이트 → Enter_Phase |
| 라운드 타이머 세팅 | `Game_Manager.cpp:394` | `Apply_RoundStart()` — m_fRoundTimer = duration_ms/1000 |
| 점수 업데이트 (전환 없음) | `Game_Manager.cpp:403` | `Apply_RoundEnd()` — 점수만; 페이즈는 SC_PHASE_CHANGE로 |
| 팀 점수 + HUD 갱신 | `Game_Manager.cpp:437` | `Apply_ScoreUpdate()` |
| 타이머 스냅 + CS_PHASE_READY | `Game_Manager.cpp:412` | `Apply_TimerSync()` — 선택 페이즈: m_fSelectTimer 스냅 + time_ms=0 시 Send_PhaseReady |
| PLAYING 타이머 스냅 | `Game_Manager.cpp:416` | `Apply_TimerSync()` — PLAYING 시 m_fRoundTimer 스냅 |
| Ready 클릭 → CS_PHASE_READY(0) | `Game_Manager.cpp:244` | `Update_CharSelect()` — m_bCSReady true 시 Send_PhaseReady(0) 즉시 전송 |
| m_bSelectExpired 리셋 (SCOREBOARD) | `Game_Manager.cpp:174` | `OnEnter_Scoreboard()` — m_bSelectExpired = false |
| m_bSelectExpired 리셋 (SHOP) | `Game_Manager.cpp:192` | `OnEnter_Shop()` — m_bSelectExpired = false |
| 로컬 카운트다운 | `Game_Manager.cpp:1270` | `Tick_SelectTimer()` — 선택 페이즈에서 매 프레임 감소 |
| 타이머 UI 갱신 | `Game_Manager.cpp:595` | `Refresh_Timer()` — PLAYING: Make_TimerString, 기타: Make_SelectTimerString |
| CS_PHASE_READY 전송 | `NetworkClient.cpp:238` | `Send_PhaseReady()` — 인스턴스 소켓으로 전송 |

---

### 미완성 / 잔존 문제

| # | 위치 | 내용 | 위험도 |
|---|------|------|--------|
| 1 | `Game_Manager.cpp:289` | `Update_Shop()` USE_SHOP=true 경로 — `Enter_Phase(PLAYING)` 로컬 전환. USE_SHOP=false라 현재 실행 안 됨. | 낮음 |
| 2 | `RoomPhaseManager.cpp:48` | PLAYING 타임아웃 승자 = `pending_winner = 0` (팀A 고정). 생존 기반 판정은 STEP 7에서 교체. | STEP 7 의존 |
| 3 | `RoomPhaseManager.cpp:244` | `Broadcast_ScoreUpdate()` — kills/deaths/assists 전부 0 더미. 실제 값은 STEP 6에서 교체. | STEP 6 의존 |

**이번 실행에서 제거된 항목:**
- `Force_StartPlaying()` — 죽은 코드 삭제 (`.h`, `.cpp` 모두)
- `End_Round()` — 죽은 코드 삭제 (로컬 Enter_Phase 포함)
- `Ready_HUD()` 내 사용 안 되는 타이머 텍스트 블록 삭제
- `SCOREBOARD_AUTO = 3.f` 상수 삭제

**이번 실행에서 추가된 방어 코드 및 버그픽스:**
- `OnEnter_Scoreboard()`: `m_fSelectTimer = SCOREBOARD_TIMEOUT` 초기값 추가 (SC_TIMER_SYNC 지연 대비)
- `OnEnter_Shop()`: `m_fSelectTimer = SHOP_DURATION` 초기값 추가
- `Apply_TimerSync()`: `GAMEOVER/CHARSELECT_END` 페이즈에서 CS_PHASE_READY 오발송 차단 (`else if` 조건으로 세 선택 페이즈만 처리)
- `Start_Match()`: `Enter_Phase(CHARSELECT)` 제거 → `m_ePhase = PHASE_END` + `Apply_PhaseVisibility(PHASE_END)` 로 서버 SC_PHASE_CHANGE 수신 전까지 대기 (2026-06-22)
- `CGame_Manager::Create()`: `m_pInstance = pInstance` 추가 — Controller의 `GetInstance()`가 미초기화 인스턴스를 새로 생성하던 버그 수정 (2026-06-22)
- `Level_Gameplay::Free()`: `Safe_Release(m_pGameManager)` → `CGame_Manager::DestroyInstance()` 로 교체 — 싱글톤 포인터 안전 해제 (2026-06-22)
- `Level_Gameplay::Update()`: `IsConnected()` (로비 소켓) → `IsInGame()` (인스턴스 소켓) — 로비 연결 종료 후 CGame_Manager::Update()가 호출 안 되던 버그 수정 (2026-06-22)
- 매치 이벤트 처리: `Controller::Apply_ServerEvents()` → `CGame_Manager::Update()` 로 이동 (2026-06-22)

---

### 검증 체크리스트 ✅ 완료

- [x] 2클라 동시 접속 → CHARSELECT 화면 표시
- [x] 타이머 UI "TIME 30" → "TIME 29" → ... 실시간 감소 확인
- [x] SC_TIMER_SYNC 수신 시 서버 값으로 스냅 (드리프트 없음)
- [x] CHARSELECT: Ready 클릭 → CS_PHASE_READY(0) 전송 → 전원 클릭 시 SCOREBOARD 전환
- [x] CHARSELECT: 30초 만료 → SC_TIMER_SYNC(0) → CS_PHASE_READY(0) → SC_PHASE_CHANGE(SCOREBOARD)
- [x] SCOREBOARD: 8초 후 SC_TIMER_SYNC(0) → CS_PHASE_READY(1) → SC_PHASE_CHANGE(SHOP)
- [x] SHOP: 15초 후 SC_TIMER_SYNC(0) → CS_PHASE_READY(2) → SC_PHASE_CHANGE(PLAYING)
- [x] PLAYING: SC_ROUND_START 수신 → m_fRoundTimer "1:40" → HUD 카운트다운
- [x] PLAYING: 100초 후 SC_ROUND_END + SC_PHASE_CHANGE(SCOREBOARD) → 2라운드 진입
- [x] 10라운드 후 SC_PHASE_CHANGE(GAMEOVER)

---

## STEP 2 — 매치 로스터 브로드캐스트 ✅ 구현 완료

**목적:** 게임 시작 시 서버가 실제 플레이어 구성(팀·슬롯·이름)을 전체에 전송한다. 클라이언트의 `Setup_DummyPlayers()` 더미를 실제 로스터로 교체.

**서버에서 해야 할 작업**
1. `protocol.h` — `SC_ROSTER_INFO_PACKET` 정의  
   - 필드: `size`, `type`, `player_count`, `players[]`  
   - 각 player 항목: `player_id`, `team(0/1)`, `slot(1~3)`, `name[20]`
2. `GameSession` / `GameSessionManager` — 플레이어별 `team`, `slot` 정보 저장 필드 추가  
   (현재 `m_iCharType`만 있음)
3. `RoomPhaseManager::TransitionTo(CHARSELECT)` 또는 `OnPlayerJoined` 내 전원 입장 완료 시점에서  
   `Broadcast_RosterInfo(room_id)` 호출
4. `Broadcast_RosterInfo()` 구현 — 방 안 플레이어 전원의 정보를 수집하여 패킷 구성 후 브로드캐스트

**완료 조건**
- CHARSELECT 진입 시 모든 클라가 SC_ROSTER_INFO 수신
- 클라 `Setup_DummyPlayers()` 대신 수신 데이터로 `m_vStats` 구성 가능

---

## STEP 3 — 맵 로드 완료 동기화 ✅ 구현 완료

**목적:** 각 클라이언트의 맵 로드 완료를 서버가 수집하고 전원 완료 시 다음 단계 진행을 허가한다.

**서버에서 해야 할 작업**
1. `protocol.h` — 패킷 정의
   - `CS_MAP_LOADED_PACKET`: `size`, `type`, `slot`
   - `SC_MAP_LOADED_PACKET`: `size`, `type`, `slot` (발신자 slot 포함 브로드캐스트)
2. `GameSessionManager::ProcessPacket` — `CS_MAP_LOADED` 케이스 추가  
   - 수신 시 `RoomPhaseManager::OnMapLoaded(room_id, slot)` 호출  
   - 발신자 정보 포함 `SC_MAP_LOADED` 브로드캐스트
3. `RoomPhaseManager` — `OnMapLoaded(room_id, slot)` 구현  
   - 방별 로드 완료 카운트 추적 (`map_loaded_count`)  
   - 전원 완료 시 SCOREBOARD → SHOP 전환 트리거

**완료 조건**
- 전원 로드 완료 → 서버가 다음 페이즈 전환
- 한 명이 느려도 전원 완료 전까지 페이즈 진행 대기

---

## STEP 4 — 캐릭터 선택 릴레이 ✅ 구현 완료

**목적:** 한 플레이어의 캐릭터 선택(Pig/Chick)을 같은 방의 다른 플레이어에게 전달한다. 상대방이 올바른 모델로 렌더링되려면 필수.

**서버에서 해야 할 작업**
1. `protocol.h` — `SC_CHAR_SELECT_PACKET` 정의  
   - 필드: `size`, `type`, `player_id`, `char_type(0=Pig/1=Chick)`
2. `GameSessionManager::ProcessPacket` — `CS_CHAR_SELECT` 케이스 수정  
   - 현재: `m_iCharType` 저장만 함  
   - 추가: `SC_CHAR_SELECT`를 같은 방 전원에게 브로드캐스트  
   - 발신자 본인 포함 or 제외 여부는 클라 구현에 맞게 결정

**완료 조건**
- 클라 A가 Pig 선택 → 클라 B가 SC_CHAR_SELECT 수신 → 클라 B 화면에 A가 Pig 모델로 표시
- 클라 A가 Chick 선택 → 클라 B가 SC_CHAR_SELECT 수신 → 클라 B 화면에 A가 Chick 모델로 표시
- 1인칭 모델은 Pig/Chick 구분 없이 동일하므로 별도 처리 불필요

---

## STEP 5 — 스폰 위치 선택 릴레이 ✅ 구현 완료

**목적:** SHOP(MAPSELECT) 단계에서 선택한 스폰 좌표를 전체에 공유하고, 캐릭터 선택/스코어보드 페이즈에서 캐릭터를 팀 시작 지점에 배치한다.

**구현 완료 항목 (2026-06-25)**

| 항목 | 파일 | 내용 |
|------|------|------|
| CS_SPAWN_SELECT 수신 릴레이 | `GameSessionManager.cpp` | `m_spawnPos` 저장 + `SC_SPAWN_SELECT` 방 전원 브로드캐스트 |
| PLAYING 진입 스폰 위치 적용 | `RoomPhaseManager.cpp::ApplySpawnPositions` | 선택 좌표로 서버 권위 위치 세팅, 브로드캐스트 지연(포물선 연출용) |
| 싱크대→식탁 포물선 호 | `Player_1rd.cpp::Apply_ServerCorrection` | `m_bLaunching` 중 서버 보정 차단, 착지 후 서버 위치로 수렴 |
| 팀 싱크대 스테이징 배치 | `RoomPhaseManager.cpp::ApplyTeamSpawnPositions` | CHARSELECT/SCOREBOARD 진입 시 싱크대 위 팀/슬롯 정렬 좌표 세팅 + 즉시 브로드캐스트 (2026-06-26) |
| CS_MOVE 물리 게이트 | `GameSessionManager.cpp::CS_MOVE` | `GetRoomPhase()==PLAYING` 일 때만 `ApplyPlayerPhysics` 실행 (선택 페이즈엔 싱크대 위 고정) |

---

## STEP 6 — 전투 결과 (피격·사망·K/D/A) ✅ 구현 완료

**목적:** 히트 판정을 서버가 처리하고 K/D/A·생존 여부를 전체에 브로드캐스트한다.

**구현 완료 (2026-06-26)**

| 항목 | 파일 | 내용 |
|------|------|------|
| 패킷 정의 | `Server/protocol.h` | CS_HIT(16) 19B, SC_HIT(23) 25B, SC_DEATH(24) 10B |
| HP/생사 필드 | `InstanceServer/GameSession.h/cpp` | `m_hp(MAX_HP=500)`, `m_bAlive`; 생성자 초기화 |
| HP 라운드 리셋 | `RoomPhaseManager.cpp::ApplySpawnPositions` | PLAYING 진입 시 `m_hp=MAX_HP, m_bAlive=true` |
| CS_HIT 처리 | `GameSessionManager.cpp::case CS_HIT` | PLAYING 게이트 + 거리(≤150) + LOS 검증 → 부위별 데미지 → SC_HIT/SC_DEATH 브로드캐스트 |
| LOS 검증 | `GameSessionManager.cpp::CheckLOS` | `m_mapPtrs` flat 순회 + `SServerCollider::IntersectsRay` |
| 클라 송신 | `NetworkClient.cpp::Send_Hit` | 인스턴스 소켓으로 CS_HIT 전송 |
| 클라 히트 감지 | `Collision_Manager.cpp::NewBullet` | 플레이어 hit 시 `Send_Hit` 전송; 로컬 TakeDamage 제거 |
| 클라 SC_HIT 수신 | `NetworkClient.cpp::ProcessInstancePacket` + `Game_Manager.cpp::Apply_Hit` | KETCHUP_SPRAY 파티클 + 본인 피격 시 HP 갱신 |
| 클라 SC_DEATH 수신 | `NetworkClient.cpp::ProcessInstancePacket` + `Game_Manager.cpp::Apply_Death` | 본인: `Die(0.f)`, 타인: `Kill_OtherPlayer(id)` |
| victim NetworkId | `CPlayer_Pig::m_iNetworkId` + `Get_NetworkId()` | `Spawn_OtherPlayer`에서 `Set_NetworkId(id)` 호출 |

**잔존 미구현:**
- 팀 전멸 판정 → `OnRoundEnd` 호출 (STEP 7)
- K/D/A 실제 연동 (현재 Broadcast_ScoreUpdate에서 0 하드코딩)

---

## STEP 7 — 라운드 승패 판정

**목적:** 서버가 전멸 또는 시간초과를 기준으로 승패를 판정하고 점수를 관리한다. 현재 F9/F10 디버그키 + `pending_winner = 0` 타임아웃 고정값 교체.

**서버에서 해야 할 작업**
1. `RoomPhaseManager::OnRoundEnd()` 수정
   - 현재: `winner_team` 파라미터를 받아 점수 누적 후 SCOREBOARD 전환  
   - 추가: `winner_team == -1`(무승부) 케이스 처리 (양 팀 동시 전멸 시)
2. 타임아웃 판정 수정 — `TickRoom`에서 PLAYING 타이머 만료 시 `pending_winner = 0` 고정값을  
   실제 생존 인원 기반 판정으로 교체  
   - 팀A 생존자 > 0 && 팀B 생존자 == 0 → 팀A 승  
   - 팀B 생존자 > 0 && 팀A 생존자 == 0 → 팀B 승  
   - 동수 또는 양팀 0 → 무승부
3. `SC_ROUND_END` 패킷에 `winner_team`, `score_a`, `score_b` 포함 확인 (이미 구조체 있음)
4. 10라운드 후 `GAMEOVER` 전환 — 최종 점수 기반 최종 승자 정보 포함 브로드캐스트 추가  
   (현재 점수만 보내고 최종 승자 명시 없음)

**완료 조건**
- 전멸 판정 → 올바른 팀이 승리 판정 받음
- 10라운드 후 GAMEOVER 전환 + 최종 승자 UI 표시

---

## 참고 — 현재 protocol.h 패킷 목록

```
// 이미 정의됨
SC_ADD_PLAYER      SC_REMOVE_PLAYER   SC_MOVE
SC_PHASE_CHANGE    SC_ROUND_START     SC_ROUND_END
SC_SCORE_UPDATE    SC_TIMER_SYNC      SC_GAME_START
CS_JOIN_ROOM       CS_MOVE            CS_CHAR_SELECT
CS_PHASE_READY     CS_LOGOUT

// STEP 2~7에서 추가 필요
SC_ROSTER_INFO     CS_MAP_LOADED      SC_MAP_LOADED
SC_CHAR_SELECT     CS_SPAWN_SELECT    SC_SPAWN_SELECT
CS_HIT             SC_COMBAT_RESULT
```
