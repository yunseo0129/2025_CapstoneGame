#include "stdafx.h"
#include "Game_Manager.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "UI_Text.h"

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

    // 더미 데이터가 준비된 뒤 스코어보드 텍스트 생성 (포인터 보관)
    Ready_ScoreboardText();

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

    // 최신 상태를 텍스트에 반영
    Refresh_Scoreboard();
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
//  UI 텍스트 (스코어보드)
// =====================================================================
HRESULT CGame_Manager::Ready_ScoreboardText()
{
    const _uint PROTO = LEVEL_STATIC;
    const _uint LV = LEVEL_GAMEPLAY;
    const _wstring SB = L"Layer_UI_Scoreboard";

    // ---- 상단 점수판 텍스트 (중앙 정렬) ----
    {
        CUI_Text::UI_TEXT_DESC d;
        d.fX = 640.f; d.fY = 62.f;
        d.fDepth = 0.3f;                       // 패널보다 앞
        d.strText = Make_ScoreString();
        d.strFontTag = L"Font_Default";
        d.vColor = _float4(1.f, 1.f, 1.f, 1.f);
        d.fTextScale = 0.7f;
        d.bCentered = true;
        m_pScoreText = static_cast<CUI_Text*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                PROTO, L"Prototype_GameObject_UI_Text", LV, SB, &d));
    }

    // ---- 플레이어 행 텍스트 6명 (슬롯 인덱스 = 배열 인덱스) ----
    //  팀 A(슬롯 0~2)는 왼쪽 패널, 팀 B(슬롯 3~5)는 오른쪽 패널 안쪽 좌표.
    for (_int i = 0; i < MAX_PLAYER && i < (_int)m_vStats.size(); ++i)
    {
        const PLAYER_STAT& stat = m_vStats[i];

        CUI_Text::UI_TEXT_DESC d;
        d.fDepth = 0.3f;
        d.strFontTag = L"Font_Default";
        d.fTextScale = 0.6f;
        d.bCentered = false;
        d.strText = Make_RowString(stat);

        if (stat.iTeam == 0) // 팀 A: 왼쪽
        {
            d.fX = 160.f;
            d.fY = 230.f + i * 64.f;
            d.vColor = _float4(0.85f, 0.92f, 1.f, 1.f);
        }
        else // 팀 B: 오른쪽 (슬롯 3,4,5 → 0,1,2 행)
        {
            d.fX = 704.f;
            d.fY = 230.f + (i - 3) * 64.f;
            d.vColor = _float4(1.f, 0.88f, 0.88f, 1.f);
        }

        m_pPlayerRowText[i] = static_cast<CUI_Text*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                PROTO, L"Prototype_GameObject_UI_Text", LV, SB, &d));
    }

    return S_OK;
}

void CGame_Manager::Refresh_Scoreboard()
{
    // 데이터 → 텍스트로 push. (데이터가 바뀐 시점에만 호출)
    if (m_pScoreText != nullptr)
        m_pScoreText->Set_Text(Make_ScoreString());

    for (_int i = 0; i < MAX_PLAYER && i < (_int)m_vStats.size(); ++i)
    {
        if (m_pPlayerRowText[i] != nullptr)
            m_pPlayerRowText[i]->Set_Text(Make_RowString(m_vStats[i]));
    }
}

_wstring CGame_Manager::Make_RowString(const PLAYER_STAT& stat) const
{
    // "이름    K / D / A    $돈"
    wchar_t buf[128];
    swprintf_s(buf, _countof(buf), L"%s    %d / %d / %d    $%d",
        stat.strName.c_str(), stat.iKill, stat.iDeath, stat.iAssist, stat.iMoney);
    return buf;
}

_wstring CGame_Manager::Make_ScoreString() const
{
    // "TEAM A   n : n   TEAM B      ROUND r/10"
    wchar_t buf[128];
    swprintf_s(buf, _countof(buf), L"TEAM A   %d : %d   TEAM B      ROUND %d/%d",
        m_iTeamScore[0], m_iTeamScore[1], m_iRound, MAX_ROUND);
    return buf;
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
    // UI_Text 들은 레이어가 소유(Return_Obj 가 AddRef 안 함).
    // 여기선 Release 하지 않고 참조만 끊는다. (레벨 해제 시 레이어가 실제 해제)
    m_pScoreText = nullptr;
    for (_int i = 0; i < MAX_PLAYER; ++i)
        m_pPlayerRowText[i] = nullptr;

    m_vStats.clear();
    Safe_Release(m_pGameInstance);
    __super::Free();
}