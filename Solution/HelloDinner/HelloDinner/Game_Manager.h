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
    _int    iTeam = 0;              // 0: 팀A, 1: 팀B
    _wstring strName = L"Player";
    _int    iKill = 0;
    _int    iDeath = 0;
    _int    iAssist = 0;
    _int    iMoney = 0;             // 보유 금액(상점에서 사용)
    _bool   bMapLoaded = false;     // 이 라운드 맵 로드 완료 여부
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

    const vector<PLAYER_STAT>& Get_Stats() const { return m_vStats; }
    vector<PLAYER_STAT>& Get_Stats_Mutable() { return m_vStats; } // 더미 조작용

public:
    // ---- 외부 신호 ----
    // 특정 플레이어의 맵 로드가 끝났음을 알림(스코어보드 단계 종료 판정용)
    void    Notify_MapLoaded(_int iSlot);
    // (테스트/네트워크용) 모든 플레이어 로드 완료를 강제
    void    Force_AllLoaded();
    // 라운드 결과를 반영하고 다음 라운드로 진행 (iWinnerTeam 팀 승)
    void    End_Round(_int iWinnerTeam);

private:
    // ---- 단계 진입(한 번) ----
    void    Enter_Phase(GAME_PHASE eNext);
    void    OnEnter_Scoreboard();
    void    OnEnter_Shop();
    void    OnEnter_Playing();
    void    OnEnter_GameOver();

    // ---- 단계별 매 프레임 ----
    void    Update_Scoreboard(_float fTimeDelta);
    void    Update_Shop(_float fTimeDelta);
    void    Update_Playing(_float fTimeDelta);

    // ---- 내부 헬퍼 ----
    _bool   Are_AllLoaded() const;          // 모든 플레이어 로드 완료?
    void    Reset_RoundLoadFlags();         // 라운드 시작 시 로드 플래그 초기화
    void    Setup_DummyPlayers();           // 로컬 더미 플레이어 생성

    // 레이어의 모든 UI 오브젝트를 켜고/끈다 (SetOnOff)
    void    Set_LayerVisible(const _wstring& strLayerTag, _bool bVisible);

private:
    class CGameInstance* m_pGameInstance = nullptr;

    GAME_PHASE  m_ePhase = GAME_PHASE::PHASE_SCOREBOARD;
    _int        m_iRound = 1;

    // 팀 라운드 승수 (인덱스 0: 팀A, 1: 팀B)
    _int        m_iTeamScore[2] = {0, 0};

    // 상점
    _bool       m_bShopOpen = false;        // E키 토글 상태
    _float      m_fShopTimer = 0.f;         // 구매 단계 남은 시간

    // 스코어보드 단계 타임아웃(모두 로드 안 끝나도 강제 진행)
    _float      m_fScoreboardTimer = 0.f;

    // 더미 플레이어 스탯
    vector<PLAYER_STAT> m_vStats;

    // ---- 상수 ----
    static constexpr _int   MAX_ROUND = 10;
    static constexpr _float SHOP_DURATION = 15.f;  // 구매 시간(초)
    static constexpr _float SCOREBOARD_TIMEOUT = 8.f;   // 스코어보드 최대 대기(초)

public:
    static CGame_Manager* Create();
    virtual void Free() override;
};