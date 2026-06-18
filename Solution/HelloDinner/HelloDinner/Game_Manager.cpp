#include "stdafx.h"
#include "Game_Manager.h"
#include "GameInstance.h"
#include "GameObject.h"

IMPLEMENT_SINGLETON(CGame_Manager)

namespace
{
    // 디버그 로그 (타이틀바 대신 출력창으로 단계 전환 확인)
    void GM_Log(const wchar_t* fmt, ...)
    {
#ifdef _DEBUG
        wchar_t buf[256];
        va_list args;
        va_start(args, fmt);
        _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, args);
        va_end(args);
        OutputDebugStringW(L"[Game_Manager] ");
        OutputDebugStringW(buf);
        OutputDebugStringW(L"\n");
#else
        (void)fmt;
#endif
    }
}

CGame_Manager::CGame_Manager()
    : m_pGameInstance {CGameInstance::GetInstance()}
{
    Safe_AddRef(m_pGameInstance);
}

HRESULT CGame_Manager::Initialize()
{
    Start_Match();
    return S_OK;
}

void CGame_Manager::Start_Match()
{
    m_iRound = 1;
    m_iTeamScore[0] = 0;
    m_iTeamScore[1] = 0;
    m_bShopOpen = false;
    m_fShopTimer = 0.f;
    m_fScoreboardTimer = 0.f;

    Setup_DummyPlayers();

    // 첫 라운드는 스코어보드 화면부터 시작
    m_ePhase = GAME_PHASE::PHASE_END; // Enter_Phase가 동작하도록 다른 값으로
    Enter_Phase(GAME_PHASE::PHASE_SCOREBOARD);
}

// =====================================================================
//  매 프레임
// =====================================================================
void CGame_Manager::Update(_float fTimeDelta)
{
    switch (m_ePhase)
    {
    case GAME_PHASE::PHASE_SCOREBOARD:
        Update_Scoreboard(fTimeDelta);
        break;
    case GAME_PHASE::PHASE_SHOP:
        Update_Shop(fTimeDelta);
        break;
    case GAME_PHASE::PHASE_PLAYING:
        Update_Playing(fTimeDelta);
        break;
    case GAME_PHASE::PHASE_GAMEOVER:
        // 종료 화면 대기 (입력으로 로비 복귀 등은 추후)
        break;
    default:
        break;
    }
}

// =====================================================================
//  단계 진입
// =====================================================================
void CGame_Manager::Enter_Phase(GAME_PHASE eNext)
{
    if (m_ePhase == eNext)
        return;

    m_ePhase = eNext;

    switch (eNext)
    {
    case GAME_PHASE::PHASE_SCOREBOARD: OnEnter_Scoreboard(); break;
    case GAME_PHASE::PHASE_SHOP:       OnEnter_Shop();       break;
    case GAME_PHASE::PHASE_PLAYING:    OnEnter_Playing();    break;
    case GAME_PHASE::PHASE_GAMEOVER:   OnEnter_GameOver();   break;
    default: break;
    }
}

void CGame_Manager::OnEnter_Scoreboard()
{
    m_fScoreboardTimer = 0.f;
    Reset_RoundLoadFlags();

    GM_Log(L"Round %d/%d - SCOREBOARD (현재 게임 상황) | Score %d : %d",
        m_iRound, MAX_ROUND, m_iTeamScore[0], m_iTeamScore[1]);

    // 스코어보드 UI ON, 나머지 OFF
    Set_LayerVisible(L"Layer_UI_Scoreboard", true);
    Set_LayerVisible(L"Layer_UI_Shop", false);
}

void CGame_Manager::OnEnter_Shop()
{
    m_fShopTimer = SHOP_DURATION;
    m_bShopOpen = true; // 진입 시 구매창을 띄운 상태로 시작

    GM_Log(L"Round %d - SHOP (상품 구매) | %.0f초", m_iRound, SHOP_DURATION);

    // 스코어보드 OFF, 상점 ON (게임은 뒤에서 계속)
    Set_LayerVisible(L"Layer_UI_Scoreboard", false);
    Set_LayerVisible(L"Layer_UI_Shop", true);
}

void CGame_Manager::OnEnter_Playing()
{
    m_bShopOpen = false;

    GM_Log(L"Round %d - PLAYING (게임 진행)", m_iRound);

    // 전체화면 UI 모두 OFF (인게임 HUD 는 별도 레이어로 추후)
    Set_LayerVisible(L"Layer_UI_Scoreboard", false);
    Set_LayerVisible(L"Layer_UI_Shop", false);
}

void CGame_Manager::OnEnter_GameOver()
{
    _int iWinner = (m_iTeamScore[0] > m_iTeamScore[1]) ? 0 :
        (m_iTeamScore[1] > m_iTeamScore[0]) ? 1 : -1;

    GM_Log(L"GAME OVER | Final %d : %d | Winner: %s",
        m_iTeamScore[0], m_iTeamScore[1],
        (iWinner == 0) ? L"TeamA" : (iWinner == 1) ? L"TeamB" : L"DRAW");

    // TODO(UI): 최종 결과 화면.
}

// =====================================================================
//  단계별 갱신
// =====================================================================
void CGame_Manager::Update_Scoreboard(_float fTimeDelta)
{
    m_fScoreboardTimer += fTimeDelta;

    // 모든 플레이어가 맵 로드를 끝냈거나, 타임아웃이면 상점 단계로
    if (Are_AllLoaded() || m_fScoreboardTimer >= SCOREBOARD_TIMEOUT)
    {
        Enter_Phase(GAME_PHASE::PHASE_SHOP);
    }
}

void CGame_Manager::Update_Shop(_float fTimeDelta)
{
    // E키로 구매창 토글 (게임은 계속 진행 중이라는 컨셉)
    if (m_pGameInstance->Key_Down(DIK_E))
    {
        m_bShopOpen = !m_bShopOpen;
        GM_Log(L"Shop %s", m_bShopOpen ? L"OPEN" : L"CLOSE");
        Set_LayerVisible(L"Layer_UI_Shop", m_bShopOpen);
    }

    // 구매 시간 카운트다운 → 게임 플레이로
    m_fShopTimer -= fTimeDelta;
    if (m_fShopTimer <= 0.f)
    {
        m_fShopTimer = 0.f;
        Enter_Phase(GAME_PHASE::PHASE_PLAYING);
    }
}

void CGame_Manager::Update_Playing(_float /*fTimeDelta*/)
{
    // 라운드 종료 조건은 추후(전멸/시간초과 등).
    // 지금은 임시로: 디버그 키로 라운드 종료 테스트.
#ifdef _DEBUG
    if (m_pGameInstance->Key_Down(DIK_F9))   // F9: 팀A 승으로 라운드 종료
        End_Round(0);
    else if (m_pGameInstance->Key_Down(DIK_F10)) // F10: 팀B 승으로 라운드 종료
        End_Round(1);
#endif
}

// =====================================================================
//  외부 신호
// =====================================================================
void CGame_Manager::Notify_MapLoaded(_int iSlot)
{
    for (auto& stat : m_vStats)
    {
        if (stat.iSlot == iSlot)
        {
            stat.bMapLoaded = true;
            break;
        }
    }
}

void CGame_Manager::Force_AllLoaded()
{
    for (auto& stat : m_vStats)
        stat.bMapLoaded = true;
}

void CGame_Manager::End_Round(_int iWinnerTeam)
{
    if (m_ePhase != GAME_PHASE::PHASE_PLAYING)
        return;

    if (iWinnerTeam == 0 || iWinnerTeam == 1)
        ++m_iTeamScore[iWinnerTeam];

    GM_Log(L"Round %d 종료 → Score %d : %d",
        m_iRound, m_iTeamScore[0], m_iTeamScore[1]);

    // 마지막 라운드였으면 게임 종료
    if (m_iRound >= MAX_ROUND)
    {
        Enter_Phase(GAME_PHASE::PHASE_GAMEOVER);
        return;
    }

    // 다음 라운드: 다시 스코어보드부터
    ++m_iRound;
    Enter_Phase(GAME_PHASE::PHASE_SCOREBOARD);
}

// =====================================================================
//  헬퍼
// =====================================================================
_int CGame_Manager::Get_TeamScore(_int iTeam) const
{
    if (iTeam != 0 && iTeam != 1)
        return 0;
    return m_iTeamScore[iTeam];
}

_bool CGame_Manager::Are_AllLoaded() const
{
    if (m_vStats.empty())
        return false;
    for (const auto& stat : m_vStats)
    {
        if (!stat.bMapLoaded)
            return false;
    }
    return true;
}

void CGame_Manager::Reset_RoundLoadFlags()
{
    for (auto& stat : m_vStats)
        stat.bMapLoaded = false;
}

void CGame_Manager::Setup_DummyPlayers()
{
    m_vStats.clear();

    // 로컬 더미: 팀당 3명씩 6명. 본인은 슬롯 0(팀A).
    const wchar_t* kNames[6] = {
        L"Me", L"Ally_1", L"Ally_2",
        L"Enemy_1", L"Enemy_2", L"Enemy_3"
    };

    for (_int i = 0; i < 6; ++i)
    {
        PLAYER_STAT stat;
        stat.iSlot = i;
        stat.iTeam = (i < 3) ? 0 : 1;   // 앞 3명 팀A, 뒤 3명 팀B
        stat.strName = kNames[i];
        stat.iKill = 0;
        stat.iDeath = 0;
        stat.iAssist = 0;
        stat.iMoney = 800;               // 시작 자금(더미)
        stat.bMapLoaded = false;
        m_vStats.push_back(stat);
    }

    // 본인(슬롯 0)은 즉시 로드 완료 처리(테스트 편의)
    if (!m_vStats.empty())
        m_vStats[0].bMapLoaded = true;
}

void CGame_Manager::Set_LayerVisible(const _wstring& strLayerTag, _bool bVisible)
{
    // UI 는 항상 GAMEPLAY 레벨에 생성되므로 고정
    list<CGameObject*> objs = m_pGameInstance->Get_List(LEVEL_GAMEPLAY, strLayerTag);
    for (auto* pObj : objs)
    {
        if (pObj != nullptr)
            pObj->SetOnOff(bVisible);
    }
}

// =====================================================================
CGame_Manager* CGame_Manager::Create()
{
    CGame_Manager* pInstance = new CGame_Manager();
    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Create : CGame_Manager");
        Safe_Release(pInstance);
    }
    return pInstance;
}

void CGame_Manager::Free()
{
    m_vStats.clear();
    Safe_Release(m_pGameInstance);
    __super::Free();
}