# 전체 서버 구조
```mermaid
sequenceDiagram
    participant IS as InstanceServer.exe
    participant LS as LobbyServer(SERVER.exe)
    participant C as Client

    Note over IS,LS: 1단계: 서버 기동
    IS->>LS: 내부 포트(3999)로 접속 + IS_REGISTER 전송
    LS->>LS: 인스턴스 등록 완료
    IS->>LS: 3초마다 IS_HEARTBEAT (부하 리포트)

    Note over C,LS: 2단계: 클라이언트 로그인 + 매칭
    C->>LS: 로비 포트(4000)로 접속 + CS_LOGIN
    LS->>C: SC_LOGIN_INFO + SC_MATCH_WAIT
    LS->>LS: 6명 모이면 TryMatch()
    LS->>LS: 가장 여유로운 인스턴스 선택
    LS->>IS: IS_ROOM_NOTIFY (방 정보 + 인증 토큰)
    LS->>C: SC_REDIRECT (인스턴스 IP/Port + 토큰)

    Note over C,IS: 3단계: 게임 진행
    C->>IS: 인스턴스 포트(5001~)로 재접속 + CS_JOIN_ROOM
    IS->>C: SC_ADD_PLAYER (방 내 동기화)
    C->>IS: CS_MOVE
    IS->>C: SC_MOVE_PLAYER (브로드캐스트)
```

# 로드 밸런싱 + 매칭

```mermaid
graph TD
    C1["Client 1"] --> LB["Lobby / Match Server\n(매칭 + 로드밸런싱)"]
    C2["Client 2"] --> LB
    C3["Client 3"] --> LB
    LB -->|"매칭 완료 → 가장 여유로운 서버 할당"| IS1["Instance Server #1\n(Game IOCP)"]
    LB -->|"매칭 완료 → 할당"| IS2["Instance Server #2\n(Game IOCP)"]
    LB -->|"매칭 완료 → 할당"| IS3["Instance Server #3\n(Game IOCP)"]
    IS1 --> DB["DB Server\n(로그인/전적/상점)"]
    IS2 --> DB
    IS3 --> DB
    LB -->|"Heartbeat / 부하 리포트"| IS1
    LB -->|"Heartbeat / 부하 리포트"| IS2
    LB -->|"Heartbeat / 부하 리포트"| IS3
```

## 흐름도

```mermaid
sequenceDiagram
    participant C as Client
    participant L as Lobby Server
    participant I as Instance Server
    
    C->>L: CS_LOGIN (로그인)
    L->>C: SC_LOGIN_INFO
    L->>L: 매칭 큐에 등록
    L->>L: 6명 모이면 TryMatch()
    L->>L: 인스턴스 서버 부하 비교 → 최적 서버 선택
    L->>C: SC_MATCH_SUCCESS + 인스턴스 서버 IP/Port
    C->>I: 인스턴스 서버에 재접속 (CS_JOIN_ROOM)
    I->>C: SC_ADD_PLAYER (방 내 동기화)
    C->>I: CS_MOVE (게임 중 패킷)
    I->>C: SC_MOVE_PLAYER (브로드캐스트)
```

## 사용법

### 1번째: 로비 서버 먼저 실행
SERVER.exe

### 2번째~: 인스턴스 서버를 각각 다른 창에서 실행
InstanceServer.exe 0 5001 127.0.0.1\
InstanceServer.exe 1 5002 127.0.0.1\
InstanceServer.exe 2 5003 127.0.0.1

## 사용법2

배치 파일로 실행