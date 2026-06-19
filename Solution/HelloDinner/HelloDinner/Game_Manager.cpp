#include "stdafx.h"
#include "Game_Manager.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "UI_Text.h"
#include "UI_Panel.h"
#include "Controller.h"
#include "Player_1rd.h"

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

    // 인게임 HUD(중앙 상단 라운드 점수 + 생존/사망 박스) 생성
    Ready_HUD();

    // 상점 무기 슬롯(클릭 → 무기 교체) 생성
    Ready_ShopSlots();

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
    Set_LayerVisible(L"Layer_UI_MiniMap", false);
    Set_LayerVisible(L"Layer_UI_HUD", false);

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
    Set_LayerVisible(L"Layer_UI_MiniMap", false);
    Set_LayerVisible(L"Layer_UI_HUD", false);

    // 상점에서는 클릭을 위해 커서를 보이게 하고, 플레이어 입력을 막는다.
    Set_ShopUIMode(true);
}

void CGame_Manager::OnEnter_Playing()
{
    m_bShopOpen = false;
    m_fRoundTimer = ROUND_DURATION;   // 라운드 제한 시간 시작

    GM_Log(L"Round %d - PLAYING (게임 진행)", m_iRound);

    // 전체화면 UI 모두 OFF, 인게임 HUD ON
    Set_LayerVisible(L"Layer_UI_Scoreboard", false);
    Set_LayerVisible(L"Layer_UI_Shop", false);
    Set_LayerVisible(L"Layer_UI_MiniMap", true);
    Set_LayerVisible(L"Layer_UI_HUD", true);

    // 플레이 중에는 커서를 숨기고(조준 모드) 입력을 허용한다.
    Set_ShopUIMode(false);

    // 라운드 점수/생존 박스 최신화
    Refresh_HUD();
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
        Set_ShopUIMode(m_bShopOpen); // 커서 표시 + 입력 차단 동기화
    }

    // 구매창이 열려 있으면 슬롯 클릭으로 무기 교체
    if (m_bShopOpen)
        Handle_ShopClick();

    // 구매 시간 카운트다운 → 게임 플레이로
    m_fShopTimer -= fTimeDelta;
    if (m_fShopTimer <= 0.f)
    {
        m_fShopTimer = 0.f;
        Enter_Phase(GAME_PHASE::PHASE_PLAYING);
    }
}

void CGame_Manager::Update_Playing(_float fTimeDelta)
{
    // 남은 라운드 시간 카운트다운 + HUD 타이머 갱신
    if (m_fRoundTimer > 0.f)
    {
        m_fRoundTimer -= fTimeDelta;
        if (m_fRoundTimer < 0.f)
            m_fRoundTimer = 0.f;
    }
    Refresh_HUD();   // 타이머가 매 프레임 갱신되도록

    // 시간 초과 시 라운드 종료 (승패 판정은 추후 생존 시스템으로 교체)
    if (m_fRoundTimer <= 0.f)
    {
        // TODO(생존 시스템): 생존 인원이 많은 팀 승. 지금은 더미로 팀A 승.
        End_Round(0);
        return;
    }

    // 라운드 종료 조건은 추후(전멸 등).
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
//  인게임 HUD (중앙 상단 바)
//   [팀A 생존박스] [팀A 승수] [타이머] [팀B 승수] [팀B 생존박스]
//   - 생존=흰색 박스 / 사망=검정 박스 (박스 인덱스 = 슬롯)
//   - 좌우 대칭, 화면 중앙(X=640) 기준
// =====================================================================
HRESULT CGame_Manager::Ready_HUD()
{
    const _uint PROTO = LEVEL_STATIC;
    const _uint LV = LEVEL_GAMEPLAY;
    const _wstring HUD = L"Layer_UI_HUD";

    // 화면(1280x720) 중앙 상단 기준 좌표
    const _float fCenterX = 640.f;
    const _float fRowY = 30.f;   // 텍스트/박스가 놓이는 기준 Y
    const _float fBox = 26.f;   // 생존 박스 한 변
    const _float fBoxGap = 8.f;    // 박스 간 간격
    const _float fScoreOff = 70.f;   // 중앙에서 팀 승수까지의 X 오프셋
    const _float fBoxBlockOff = 130.f;  // 중앙에서 박스 그룹 안쪽 끝까지의 X 오프셋

    // 한 팀(3명) 박스 그룹의 전체 너비
    const _int   iPerTeam = 3;
    const _float fGroupW = iPerTeam * fBox + (iPerTeam - 1) * fBoxGap;

    // ---- 중앙: 남은 라운드 시간 타이머 ----
    {
        CUI_Text::UI_TEXT_DESC d;
        d.fX = fCenterX; d.fY = fRowY;
        d.fDepth = 0.3f;
        d.strText = Make_TimerString();
        d.strFontTag = L"Font_Default";
        d.vColor = _float4(1.f, 1.f, 1.f, 1.f);
        d.fTextScale = 0.85f;
        d.bCentered = true;
        m_pHUDTimerText = static_cast<CUI_Text*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                PROTO, L"Prototype_GameObject_UI_Text", LV, HUD, &d));
    }

    // ---- 팀 승수 (타이머 좌우) ----
    {
        // 팀 A (왼쪽, 파랑)
        CUI_Text::UI_TEXT_DESC d;
        d.fDepth = 0.3f;
        d.strFontTag = L"Font_Default";
        d.fTextScale = 0.85f;
        d.bCentered = true;
        d.fY = fRowY;

        wchar_t buf[16];

        d.fX = fCenterX - fScoreOff;
        d.vColor = _float4(0.55f, 0.78f, 1.f, 1.f);
        swprintf_s(buf, _countof(buf), L"%d", m_iTeamScore[0]);
        d.strText = buf;
        m_pHUDTeamScoreText[0] = static_cast<CUI_Text*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                PROTO, L"Prototype_GameObject_UI_Text", LV, HUD, &d));

        // 팀 B (오른쪽, 빨강)
        d.fX = fCenterX + fScoreOff;
        d.vColor = _float4(1.f, 0.6f, 0.6f, 1.f);
        swprintf_s(buf, _countof(buf), L"%d", m_iTeamScore[1]);
        d.strText = buf;
        m_pHUDTeamScoreText[1] = static_cast<CUI_Text*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                PROTO, L"Prototype_GameObject_UI_Text", LV, HUD, &d));
    }

    // ---- 생존/사망 박스 (양쪽 끝) ----
    //  팀A(슬롯 0~2): 왼쪽 그룹, 안쪽 끝이 (center - fBoxBlockOff)에서 왼쪽으로.
    //  팀B(슬롯 3~5): 오른쪽 그룹, 안쪽 끝이 (center + fBoxBlockOff)에서 오른쪽으로.
    const _float fBoxY = fRowY - 2.f; // 텍스트와 세로 정렬 보정(필요시 조정)

    // 팀A 그룹 시작 X (왼쪽 그룹의 가장 왼쪽 박스)
    const _float fA_StartX = (fCenterX - fBoxBlockOff) - fGroupW;
    // 팀B 그룹 시작 X (오른쪽 그룹의 가장 왼쪽 박스)
    const _float fB_StartX = (fCenterX + fBoxBlockOff);

    const _int iCount = (m_vStats.size() < (size_t)MAX_PLAYER) ? (_int)m_vStats.size() : MAX_PLAYER;
    for (_int i = 0; i < iCount; ++i)
    {
        const _int iTeam = m_vStats[i].iTeam;           // 0 또는 1
        const _int iIdxInTeam = (iTeam == 0) ? i : (i - iPerTeam); // 팀 내 0,1,2
        const _float fGroupStart = (iTeam == 0) ? fA_StartX : fB_StartX;

        CUI_Panel::UI_PANEL_DESC d;
        d.fX = fGroupStart + iIdxInTeam * (fBox + fBoxGap);
        d.fY = fBoxY;
        d.fSizeX = fBox; d.fSizeY = fBox;
        d.fDepth = 0.4f;
        // 초기색은 Refresh_HUD 가 생존 여부로 다시 칠한다.
        d.vColor = _float4(1.f, 1.f, 1.f, 1.f);
        m_pHUDPlayerBox[i] = static_cast<CUI_Panel*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                PROTO, L"Prototype_GameObject_UI_Panel", LV, HUD, &d));
    }

    // 생성 직후엔 PLAYING 단계가 아니므로 꺼둔다.
    Set_LayerVisible(HUD, false);

    return S_OK;
}

void CGame_Manager::Refresh_HUD()
{
    // 중앙 타이머 갱신
    if (m_pHUDTimerText != nullptr)
        m_pHUDTimerText->Set_Text(Make_TimerString());

    // 팀 승수 갱신
    for (_int t = 0; t < 2; ++t)
    {
        if (m_pHUDTeamScoreText[t] != nullptr)
        {
            wchar_t buf[16];
            swprintf_s(buf, _countof(buf), L"%d", m_iTeamScore[t]);
            m_pHUDTeamScoreText[t]->Set_Text(buf);
        }
    }

    // 생존/사망 박스 색: 생존=흰색, 사망=검정
    const _float4 vAlive = _float4(1.f, 1.f, 1.f, 1.f);   // 흰색
    const _float4 vDead = _float4(0.f, 0.f, 0.f, 1.f);   // 검정

    const _int iCount = (m_vStats.size() < (size_t)MAX_PLAYER) ? (_int)m_vStats.size() : MAX_PLAYER;
    for (_int i = 0; i < iCount; ++i)
    {
        if (m_pHUDPlayerBox[i] == nullptr)
            continue;

        // TODO(생존 시스템): 실제 "이번 라운드 생존" 플래그로 교체.
        //  지금은 더미로 항상 생존(흰색) 처리.
        const _bool bAlive = true;
        m_pHUDPlayerBox[i]->Set_Color(bAlive ? vAlive : vDead);
    }
}

_wstring CGame_Manager::Make_TimerString() const
{
    // 남은 라운드 시간 "m:ss"
    const _int iTotal = (_int)(m_fRoundTimer + 0.5f); // 반올림
    const _int iMin = iTotal / 60;
    const _int iSec = iTotal % 60;
    wchar_t buf[16];
    swprintf_s(buf, _countof(buf), L"%d:%02d", iMin, iSec);
    return buf;
}

// =====================================================================
//  상점 무기 슬롯 (클릭 → 무기 교체)
//   슬롯0 = 케첩건(빨강), 슬롯1 = 마요네즈건(하양)
//   - 지금은 텍스처 대신 단색 패널. (텍스처화는 다음 단계)
//   - 좌표는 1280x720 디자인 기준. 히트테스트용 사각형을 보관한다.
// =====================================================================
HRESULT CGame_Manager::Ready_ShopSlots()
{
    const _uint PROTO = LEVEL_STATIC;
    const _uint LV = LEVEL_GAMEPLAY;
    const _wstring SH = L"Layer_UI_Shop";

    // 슬롯 색: 케첩=빨강, 마요=하양
    const _float4 vKetchup = _float4(0.85f, 0.12f, 0.12f, 1.f); // 빨강
    const _float4 vMayo = _float4(0.95f, 0.95f, 0.95f, 1.f); // 하양

    // 두 칸 위치(1280x720 기준). 화면 가운데에 가로로 두 칸.
    const _float fSlotW = 220.f;
    const _float fSlotH = 220.f;
    const _float fGap = 60.f;
    const _float fTotalW = fSlotW * SHOP_SLOT_COUNT + fGap * (SHOP_SLOT_COUNT - 1);
    const _float fStartX = 640.f - fTotalW * 0.5f;
    const _float fSlotY = 260.f;

    const _float4 kColors[SHOP_SLOT_COUNT] = {vKetchup, vMayo};

    for (_int i = 0; i < SHOP_SLOT_COUNT; ++i)
    {
        const _float fX = fStartX + i * (fSlotW + fGap);

        CUI_Panel::UI_PANEL_DESC d;
        d.fX = fX; d.fY = fSlotY;
        d.fSizeX = fSlotW; d.fSizeY = fSlotH;
        d.fDepth = 0.45f;                  // 상점 헤더/오버레이보다 앞
        d.vColor = kColors[i];
        m_pShopSlot[i] = static_cast<CUI_Panel*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                PROTO, L"Prototype_GameObject_UI_Panel", LV, SH, &d));

        // 히트테스트용 사각형 저장 (x, y, w, h)
        m_vShopSlotRect[i] = _float4(fX, fSlotY, fSlotW, fSlotH);
    }

    // 생성 직후 상점은 꺼진 상태 (스코어보드부터 시작) → 슬롯도 꺼둔다.
    for (_int i = 0; i < SHOP_SLOT_COUNT; ++i)
        if (m_pShopSlot[i]) m_pShopSlot[i]->SetOnOff(false);

    return S_OK;
}

void CGame_Manager::Handle_ShopClick()
{
    // 좌클릭(누른 순간)만 처리
    if (!m_pGameInstance->Mouse_Down(Engine::DIM_LB))
        return;

    // ---- 1) 절대 커서 위치 → 클라이언트 픽셀 좌표 ----
    POINT pt;
    if (!GetCursorPos(&pt))
        return;
    if (g_hWnd != nullptr)
        ScreenToClient(g_hWnd, &pt);

    // ---- 2) 클라이언트 픽셀 → 1280x720 디자인 좌표로 스케일 ----
    //  창 크기가 디자인과 다를 수 있으므로 비율로 환산.
    _float fDesignX = (_float)pt.x;
    _float fDesignY = (_float)pt.y;
    if (g_hWnd != nullptr)
    {
        RECT rc;
        if (GetClientRect(g_hWnd, &rc))
        {
            const _float fClientW = (_float)(rc.right - rc.left);
            const _float fClientH = (_float)(rc.bottom - rc.top);
            if (fClientW > 0.f && fClientH > 0.f)
            {
                fDesignX = (_float)pt.x * (1280.f / fClientW);
                fDesignY = (_float)pt.y * (720.f / fClientH);
            }
        }
    }

    // ---- 3) 슬롯 사각형 히트테스트 → 무기 교체 ----
    for (_int i = 0; i < SHOP_SLOT_COUNT; ++i)
    {
        const _float fL = m_vShopSlotRect[i].x;
        const _float fT = m_vShopSlotRect[i].y;
        const _float fR = fL + m_vShopSlotRect[i].z;
        const _float fB = fT + m_vShopSlotRect[i].w;

        if (fDesignX >= fL && fDesignX <= fR && fDesignY >= fT && fDesignY <= fB)
        {
            // 플레이어를 컨트롤러에서 받아 무기 교체
            CController* pController = m_pGameInstance->Get_Controller();
            CPlayer_1rd* pPlayer = pController ? pController->Get_Player() : nullptr;
            if (pPlayer != nullptr)
            {
                pPlayer->Set_Weapon(i);
                GM_Log(L"Shop: 무기 %d 선택 (%s)", i, (i == 0) ? L"케첩건" : L"마요네즈건");
            }

            Highlight_ShopSlot(i);
            break; // 한 번에 한 슬롯만
        }
    }
}

void CGame_Manager::Highlight_ShopSlot(_int iWeapon)
{
    // 선택된 슬롯은 밝게, 나머지는 기본 색으로.
    //  (지금은 알파만 살짝 조절해 강조. 텍스처/테두리는 다음 단계.)
    const _float4 vKetchup = _float4(0.85f, 0.12f, 0.12f, 1.f);
    const _float4 vMayo = _float4(0.95f, 0.95f, 0.95f, 1.f);
    const _float4 kBase[SHOP_SLOT_COUNT] = {vKetchup, vMayo};

    for (_int i = 0; i < SHOP_SLOT_COUNT; ++i)
    {
        if (m_pShopSlot[i] == nullptr)
            continue;

        _float4 c = kBase[i];
        if (i != iWeapon)
            c.w = 0.45f; // 선택 안 된 슬롯은 반투명하게
        m_pShopSlot[i]->Set_Color(c);
    }
}

void CGame_Manager::Set_ShopUIMode(_bool bOpen)
{
    // 1) 커서: 상점 창이 열리면 보이고, 닫히면 숨긴다.
    ShowCursor(bOpen);

    // 2) 플레이어 입력 차단: 창이 열려 있는 동안 시점/이동/사격을 막는다.
    CController* pController = m_pGameInstance->Get_Controller();
    if (pController != nullptr)
        pController->Set_BlockInput(bOpen);
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

    // HUD 도 레이어 소유. 참조만 끊는다.
    m_pHUDTimerText = nullptr;
    m_pHUDTeamScoreText[0] = nullptr;
    m_pHUDTeamScoreText[1] = nullptr;
    for (_int i = 0; i < MAX_PLAYER; ++i)
        m_pHUDPlayerBox[i] = nullptr;

    // 상점 슬롯도 레이어 소유. 참조만 끊는다.
    for (_int i = 0; i < SHOP_SLOT_COUNT; ++i)
        m_pShopSlot[i] = nullptr;

    m_vStats.clear();
    Safe_Release(m_pGameInstance);
    __super::Free();
}