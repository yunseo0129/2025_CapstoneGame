#pragma once
#include "Base.h"

/*
    CGame_Manager
    --------------------------------------------------------------------
    게임의 "라운드 진행"을 담당하는 싱글톤.

    - 총 10라운드.
    - 한 라운드는 세 개의 화면(Phase)으로 구성된다.
        1) PHASE_SCOREBOARD : 현재 게임 상황(스코어보드) 화면.
                              모든 플레이어 맵 로드가 끝나면(또는 타임아웃) 다음으로.
        2) PHASE_SHOP       : 상품 구매 화면. 게임은 계속 돌아가며,
                              E키로 구매창을 껐다 켤 수 있다(뒤로 게임이 비침).
        3) PHASE_PLAYING    : 실제 게임 플레이.
                              라운드 종료 조건이 되면 다음 라운드로.

    - 실제 UI 렌더링은 아직 더미. (콘솔/타이틀 출력으로만 확인)
      나중에 UI 오브젝트 시스템이 생기면 OnEnter_* / On*_Update 안에서
      해당 UI를 켜고 끄도록만 채우면 된다.
*/

// 라운드 화면(단계)
enum class GAME_PHASE
{
    PHASE_CHARSELECT,   // 캐릭터 선택 화면
    PHASE_SCOREBOARD,   // 현재 게임 상황 화면
    PHASE_SHOP,         // 상품 구매 화면
    PHASE_PLAYING,      // 게임 플레이 화면
    PHASE_GAMEOVER,     // 10라운드 종료
    PHASE_END
};

// 한 플레이어의 라운드 스탯(로컬 더미 데이터)
struct PLAYER_STAT
{
    _int    iSlot = -1;             // 슬롯/플레이어 인덱스
    _int    iPlayerId = -1;         // 로비 플레이어 ID (-1 = 더미/미설정)
    _int    iTeam = 0;              // 0: 팀A, 1: 팀B
    _wstring strName = L"Player";
    _int    iCharType = 0;          // 선택 캐릭터 (0=Pig, 1=Chick, 2=Fish)
    _int    iKill = 0;
    _int    iDeath = 0;
    _int    iAssist = 0;
    _int    iMoney = 0;             // 보유 금액(상점에서 사용)
    _bool   bMapLoaded = false;     // 이 라운드 맵 로드 완료 여부
    _float3 vSpawnPos = _float3(0.f, 0.f, 0.f); // 선택한 스폰 좌표 (SC_SPAWN_SELECT 수신 시 갱신)
    _bool   bSpawnSet = false;      // 스폰 좌표 수신 여부
};

class CGame_Manager final: public CBase
{
    DECLARE_SINGLETON(CGame_Manager)
private:
    CGame_Manager();
    virtual ~CGame_Manager() = default;

public:
    // ---- 수명주기 ----
    HRESULT Initialize();                       // 게임 시작 시 1회
    void    Update(_float fTimeDelta);          // 매 프레임 (GameInstance에서 호출)

    // 새 매치 시작(1라운드부터). 더미 플레이어 셋업 포함.
    void    Start_Match();

public:
    // ---- 상태 조회 ----
    GAME_PHASE  Get_Phase()        const { return m_ePhase; }
    _int        Get_Round()        const { return m_iRound; }            // 1 ~ MAX_ROUND
    _int        Get_MaxRound()     const { return MAX_ROUND; }
    _bool       Is_ShopOpen()      const { return m_bShopOpen; }         // 상점창이 떠 있는지
    _int        Get_TeamScore(_int iTeam) const;                        // 팀 라운드 승수

    // 캐릭터 선택 카메라가 바라볼 내 시작 지점(레벨에서 사용).
    _float3     Get_MySpot()       const { return m_vMySpot; }

    const vector<PLAYER_STAT>& Get_Stats() const { return m_vStats; }
    vector<PLAYER_STAT>& Get_Stats_Mutable() { return m_vStats; } // 더미 조작용

public:
    // ---- 외부 신호 ----
    // 특정 플레이어의 맵 로드가 끝났음을 알림(스코어보드 단계 종료 판정용)
    void    Notify_MapLoaded(_int iSlot);
    // (테스트/네트워크용) 모든 플레이어 로드 완료를 강제
    void    Force_AllLoaded();

    // ---- 서버 주도 이벤트 (Phase 1: Controller::Apply_ServerEvents 에서 호출) ----
    void    Apply_PhaseChange(unsigned char phase, unsigned char round);
    void    Apply_RoundStart(unsigned char round, unsigned int duration_ms, unsigned int server_time_ms);
    void    Apply_RoundEnd(unsigned char winner_team, unsigned char score_a, unsigned char score_b);
    void    Apply_ScoreUpdate(unsigned char score_a, unsigned char score_b,
                              unsigned char player_count, const PlayerStatBrief* stats);
    void    Apply_TimerSync(unsigned int time_ms);  // SC_TIMER_SYNC 수신 → 타이머 갱신
    void    Apply_RosterInfo(unsigned char count, const RosterEntry* entries); // SC_ROSTER_INFO 수신 → m_vStats 교체
    void    Apply_MapLoaded(unsigned char slot);                               // SC_MAP_LOADED 수신 → bMapLoaded 갱신
    void    Apply_CharSelect(int player_id, unsigned char char_type);          // SC_CHAR_SELECT 수신 → iCharType 갱신
    void    Apply_SpawnSelect(int player_id, _float3 vSpawnPos);              // SC_SPAWN_SELECT 수신 → vSpawnPos 갱신
    void    Apply_Hit(int shooter_id, int victim_id, short victim_hp,
                      unsigned char part_num, const float hit_pos[3]);       // SC_HIT 수신 → 파티클 + HP 반영
    void    Apply_Death(int victim_id, int killer_id);                       // SC_DEATH 수신 → 사망 처리
    void    Apply_WallBreak(int wall_id);                                    // SC_WALL_BREAK 수신 → 벽 파괴 처리

private:
    // ---- 단계 진입(한 번) ----
    void    Enter_Phase(GAME_PHASE eNext);
    void    OnEnter_CharSelect();
    void    OnEnter_Scoreboard();
    void    OnEnter_Shop();
    void    OnEnter_Playing();
    void    OnEnter_GameOver();

    // ---- 단계별 매 프레임 ----
    void    Update_CharSelect(_float fTimeDelta);
    void    Update_Scoreboard(_float fTimeDelta);
    void    Update_Shop(_float fTimeDelta);
    void    Update_Playing(_float fTimeDelta);

    // ---- 내부 헬퍼 ----
    _bool   Are_AllLoaded() const;          // 모든 플레이어 로드 완료?
    void    Reset_RoundLoadFlags();         // 라운드 시작 시 로드 플래그 초기화
    void    Setup_DummyPlayers();           // 로컬 더미 플레이어 생성

    // 레이어의 모든 UI 오브젝트를 켜고/끈다 (SetOnOff)
    void    Set_LayerVisible(const _wstring& strLayerTag, _bool bVisible);

    // ---- UI: 통합 타이머 (모든 Phase 공통, Layer_UI_Timer, 항상 표시) ----
    HRESULT  Ready_Timer();      // 단일 타이머 텍스트 1개 생성(항상 ON)
    void     Refresh_Timer();    // 현재 Phase 에 맞는 남은 시간 문자열로 갱신

    // ---- Phase 별 레이어 가시성 일괄 적용 ----
    void     Apply_PhaseVisibility(GAME_PHASE ePhase);


    // ---- UI 캐릭터선택 ----
    HRESULT Ready_CharSelect();
    void    Handle_CharSelectClick();
    void    Refresh_CharSelectFaces();

    // ---- 선택 구간 글로벌 타이머 ----
    void     Tick_SelectTimer(_float fTimeDelta);   // 감소 + 세 화면 텍스트 갱신
    _wstring Make_SelectTimerString() const;        // "TIME  nn" 포맷
    void     Set_MySpot_From_Setup();               // g_MatchSetup → 팀/번호/시작 지점
    void     Place_PlayerAt_Spot();                 // 내 플레이어 transform 을 시작 지점에 세움

    // ---- UI 텍스트 (스코어보드) ----
    HRESULT Ready_ScoreboardText();         // 점수판/플레이어 행 텍스트 생성 + 포인터 보관
    void    Refresh_Scoreboard();           // 현재 상태를 텍스트에 push (Set_Text)
    _wstring Make_RowString(const PLAYER_STAT& stat) const; // "이름  K/D/A  $돈" 포맷
    _wstring Make_ScoreString() const;      // "TEAM A  n : n  TEAM B   ROUND r/10"

    // ---- UI: 인게임 HUD (중앙 상단 바: 생존박스 - 승수 - 타이머 - 승수 - 생존박스) ----
    HRESULT  Ready_HUD();                   // HUD 패널/텍스트 생성 + 포인터 보관
    void     Refresh_HUD();                 // 점수/타이머 텍스트 + 생존 박스 색 갱신
    _wstring Make_TimerString() const;      // 남은 라운드 시간 "m:ss"

    // ---- UI: 라운드 결과 배너 (화면 정중앙 WIN/LOSE/DRAW, 5초 표시) ----
    HRESULT  Ready_ResultBanner();          // 배너 패널+텍스트 생성 (최초 1회, 숨긴 상태)

    // ---- UI: 하단 HUD (내 HP / 남은 탄약) ----
    //  화면 좌하단에 HP, 우하단에 탄약을 텍스트로 표시(이후 텍스처로 교체).
    HRESULT  Ready_PlayerHUD();             // HP/탄약 텍스트 생성 + 포인터 보관
    void     Refresh_PlayerHUD();           // 내 플레이어 상태 → 텍스트 갱신

    // ---- UI: 상점 무기 슬롯 (클릭 → 무기 교체) ----
    HRESULT  Ready_ShopSlots();             // 무기 슬롯 패널 생성 + 위치/포인터 보관
    void     Handle_ShopClick();            // 좌클릭 시 슬롯 히트테스트 → 무기 교체
    void     Highlight_ShopSlot(_int iWeapon); // 선택된 슬롯 테두리/색 강조
    // 상점 창 표시/숨김에 맞춰 커서 표시 + 플레이어 입력 차단을 함께 처리
    void     Set_ShopUIMode(_bool bOpen);

    // ---- UI: 맵 선택 창 (상점 대신 사용. 클릭 → 스폰 위치 선택) ----
    //  실제 창(CMapSelect)은 Level_Gameplay 에서 Layer_UI_MapSelect 로 생성한다.
    //  여기서는 그 창을 찾아 보관하고, 단계 전환/스폰 위치 확정에 사용한다.
    void     Cache_MapSelect();             // Layer_UI_MapSelect 에서 창 포인터 확보
    void     Apply_SpawnLaunch();           // 선택(또는 기본) 위치로 플레이어 포물선 발사

private:
    class CGameInstance* m_pGameInstance = nullptr;

    GAME_PHASE  m_ePhase = GAME_PHASE::PHASE_CHARSELECT;

    _int        m_iRound = 1;

    // 캐릭터 선택창에 필요한 정보
    class CCharSelect_Pig* m_pCSPreviewMe = nullptr;   // 중앙(나) 프리뷰 (Pig)
    class CCharSelect_Chick* m_pCSPreviewChick = nullptr;   // 중앙(나) 프리뷰 (Chick)
    class CCharSelect_Fish* m_pCSPreviewFish = nullptr;   // 중앙(나) 프리뷰 (Fish)
    class CUI_Panel* m_pCSFacePanel[3] = {nullptr, nullptr, nullptr};
    class CUI_Texture* m_pCSFaceIcon[3] = {nullptr, nullptr, nullptr};
    _float4 m_vCSFaceRect[3] = {};   // (x,y,w,h) 픽셀
    _float4 m_vCSReadyRect = {};
    _int    m_iCSMyCharacter = 0;
    _bool   m_bCSReady = false;
    _bool   m_bCSBuilt = false;

    // ---- 내 팀/번호 + 시작 지점 (대기방 g_MatchSetup 에서 읽어옴) ----
    //  iMyTeam   : 0 = RED, 1 = BLUE
    //  iMyNumber : 1~3
    //  vMySpot   : 캐릭터 선택~1인칭 시작 시 내가 서 있을 월드 좌표
    _int    m_iMyTeam = 0;
    _int    m_iMyNumber = 1;
    _float3 m_vMySpot = _float3(0.f, 0.f, 0.f);
    _float  m_fMyYaw = 0.f;     // 스폰 시 바라볼 yaw(원점 방향, 라디안)

    // 팀 라운드 승수 (인덱스 0: 팀A, 1: 팀B)
    _int        m_iTeamScore[2] = {0, 0};
    _int        m_iLastRoundWinner = -1;   // 직전 라운드 승리 팀 (0=팀A, 1=팀B, 2=무승부, -1=없음)

    // 상점
    _bool       m_bShopOpen = false;        // E키 토글 상태
    _float      m_fShopTimer = 0.f;         // 구매 단계 남은 시간

    // 플레이 단계: 남은 라운드 시간 (HUD 중앙 타이머)
    _float      m_fRoundTimer = 0.f;

    // 스코어보드 단계 타임아웃(모두 로드 안 끝나도 강제 진행)
    _float      m_fScoreboardTimer = 0.f;

    // ---- 선택 구간 글로벌 카운트다운 ----
    //  캐릭터 선택 → 상황판 → 스폰(맵) 선택을 하나의 30초 타이머로 묶는다.
    //  CharSelect 진입 시 시작, 세 단계 동안 리셋 없이 계속 감소.
    //  0 이 되면 어느 단계든 즉시 PLAYING 으로 강제 진입(미선택은 기본값 처리).
    _float      m_fSelectTimer = 0.f;
    _bool       m_bSelectExpired = false;   // 타임아웃으로 강제 진행됐는지

    //  모든 Phase 공통 "남은 시간". Layer_UI_Timer 에 단 하나만 둔다.
    //  내용만 Phase 에 따라 바꿔 끼운다(선택구간=선택타이머 / 플레이=라운드타이머).
    class CUI_Text* m_pTimerText = nullptr;


    // 더미 플레이어 스탯
    vector<PLAYER_STAT> m_vStats;

    // ---- UI 텍스트 포인터 (push 갱신 대상) ----
    //  데이터 주인(Game_Manager)이 자신의 UI 도 직접 보관한다.
    class CUI_Text* m_pScoreText = nullptr;            // 상단 점수판
    class CUI_Text* m_pPlayerRowText[6] = {nullptr}; // 플레이어 6명 행 (슬롯 인덱스 = 배열 인덱스)
    static constexpr _int MAX_PLAYER = 6;

    // ---- 인게임 HUD 포인터 ----
    //  중앙 상단 바: [팀A 생존박스] [팀A 승수] [타이머] [팀B 승수] [팀B 생존박스]
    //  생존=흰색 박스 / 사망=검정 박스. 박스 인덱스 = 슬롯(= m_vStats 인덱스).
    class CUI_Text* m_pHUDTeamScoreText[2] = {nullptr}; // [0]=팀A 승수, [1]=팀B 승수
    class CUI_Panel* m_pHUDPlayerBox[MAX_PLAYER] = {nullptr};

    // ---- 하단 HUD (내 HP / 남은 탄약) ----
    class CUI_Text* m_pHUDHealthText = nullptr;   // 좌하단 HP
    class CUI_Text* m_pHUDAmmoText = nullptr;    // 우하단 탄약

    // ---- 상점 무기 슬롯 ----
    //  슬롯 인덱스 = 무기 인덱스 (0=케첩건/빨강, 1=마요네즈건/하양).
    //  히트테스트를 위해 슬롯의 픽셀 사각형(좌상단 x,y / 크기 w,h)을 보관.
    static constexpr _int SHOP_SLOT_COUNT = 2;
    class CUI_Panel* m_pShopSlot[SHOP_SLOT_COUNT] = {nullptr};
    _float4          m_vShopSlotRect[SHOP_SLOT_COUNT] = {}; // (x, y, w, h) 픽셀

    // ---- 맵 선택 창 ----
    //  상점 대신 띄우는 맵 정보/스폰 선택 창. (Level_Gameplay 에서 생성)
    class CMapSelect* m_pMapSelect = nullptr;

    // ---- 라운드 결과 배너 (Layer_UI_Result, Apply_PhaseVisibility 미관리) ----
    class CUI_Panel* m_pResultPanel = nullptr;  // 반투명 배경 박스
    class CUI_Text*  m_pResultText  = nullptr;  // "YOU WIN" / "YOU LOSE" / "DRAW"
    _float           m_fResultBannerTimer = 0.f; // 배너 남은 표시 시간 (0=숨김)

    // 아무 곳도 클릭 안 하고 시간이 끝났을 때 사용할 기본 스폰 위치(코드에서 지정).
    _float3          m_vDefaultSpawn = _float3(0.f, 0.f, 0.f);

    // 스폰 발사 시 포물선 정점 추가 높이(현재/목표 중 높은 y + 이 값).
    static constexpr _float SPAWN_ARC_HEIGHT = 3.f;

    // 상점 단계를 쓸지 여부. (false = 맵 선택 창 사용 / true = 예전 상점 사용)
    //  나중에 상점을 되살리려면 이 값을 true 로 바꾸면 된다.
    static constexpr _bool USE_SHOP = false;

    // ---- 상수 ----
    static constexpr _int   MAX_ROUND = 10;
    static constexpr _float SHOP_DURATION = 15.f;  // 구매 시간(초)
    static constexpr _float SCOREBOARD_TIMEOUT = 8.f;   // 스코어보드 최대 대기(초)
    static constexpr _float ROUND_DURATION = 100.f; // 한 라운드 제한 시간(초)

    // 선택 구간(캐릭터+상황판+스폰) 전체 제한 시간. 0 이 되면 강제 시작.
    static constexpr _float SELECT_TOTAL_DURATION = 30.f;


private:
    _bool m_bMouseCaptured = false;
public:
    void Set_MouseCaptured(_bool bCaptured); // 플레이=true(숨김+가둠), 메뉴=false
    void Update_MouseClip();                 // 매 프레임 호출(포커스 변화 자가 복구)

public:
    static CGame_Manager* Create();
    virtual void Free() override;
};