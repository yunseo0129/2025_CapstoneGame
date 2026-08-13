#include "stdafx.h"
#include "Game_Manager.h"
#include "NetworkClient.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "UI_Text.h"
#include "UI_Panel.h"
#include "UI_Texture.h"
#include "Controller.h"
#include "Player_1rd.h"
#include "CharSelect_Pig.h"
#include "CharSelect_Chick.h"
#include "CharSelect_Fish.h"
#include "MapSelect.h"
#include "Particle_System.h"
#include "Map.h"

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

// ===== 마우스 캡처 유틸 =====================================
// ShowCursor 는 카운터 방식이라, 실제 가시 상태를 확인하고 필요할 때만 토글한다.
static void SetCursorVisible(bool bShow)
{
    CURSORINFO ci {sizeof(CURSORINFO)};
    GetCursorInfo(&ci);
    const bool bVisible = (ci.flags & CURSOR_SHOWING) != 0;
    if (bShow && !bVisible)  while (ShowCursor(TRUE) < 0) {}
    if (!bShow && bVisible)  while (ShowCursor(FALSE) >= 0) {}
}

// 커서를 클라이언트 영역(스크린 좌표)에 가둔다.
static void ClipCursorToClient(HWND hWnd)
{
    if (nullptr == hWnd) return;
    RECT rc {};
    GetClientRect(hWnd, &rc);
    POINT lt {rc.left, rc.top};
    POINT rb {rc.right, rc.bottom};
    ClientToScreen(hWnd, &lt);   // 클라이언트 → 스크린 좌표
    ClientToScreen(hWnd, &rb);
    RECT rcScreen {lt.x, lt.y, rb.x, rb.y};
    ClipCursor(&rcScreen);
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

    // 대기방에서 고른 팀/번호 → 내 시작 지점 확정 (더미 셋업보다 먼저)
    Set_MySpot_From_Setup();

    Setup_DummyPlayers();

    // 더미 데이터가 준비된 뒤 스코어보드 텍스트 생성 (포인터 보관)
    Ready_ScoreboardText();

    // 인게임 HUD(중앙 상단 라운드 점수 + 생존/사망 박스) 생성
    Ready_HUD();

    // 하단 HUD(내 HP / 남은 탄약) 생성
    Ready_PlayerHUD();

    // 상점 무기 슬롯(클릭 → 무기 교체) 생성
    Ready_ShopSlots();

    // 맵 선택 창 포인터 확보(상점 대신 사용). Level_Gameplay 가 미리 생성해 둠.
    Cache_MapSelect();

    // 라운드 결과 배너 (화면 정중앙 WIN/LOSE/DRAW, 숨긴 상태로 생성)
    Ready_ResultBanner();

    Ready_Timer();

    m_fSelectTimer   = 0.f;     // 서버 SC_TIMER_SYNC 수신 시 세팅
    m_bSelectExpired = false;

    // 서버 SC_PHASE_CHANGE 수신 전까지 UI 전부 숨김 상태로 대기
    m_ePhase = GAME_PHASE::PHASE_END;
    Apply_PhaseVisibility(GAME_PHASE::PHASE_END);
}

// =====================================================================
//  매 프레임
// =====================================================================
void CGame_Manager::Update(_float fTimeDelta)
{
    // 서버 게임 상태 이벤트 처리
    auto* pNet = NetworkClient::GetInstance();
    if (pNet->IsInGame())
    {
        std::vector<NetworkClient::NetEvent> matchEvents;
        pNet->PopAllMatchEvents(matchEvents);
        for (auto& evt : matchEvents)
        {
            switch (evt.type)
            {
            case NetworkClient::NetEventType::PHASE_CHANGE:
                Apply_PhaseChange(evt.phase, evt.round);
                break;
            case NetworkClient::NetEventType::ROUND_START:
                Apply_RoundStart(evt.round, evt.duration_ms, evt.server_time_ms);
                break;
            case NetworkClient::NetEventType::ROUND_END:
                Apply_RoundEnd(evt.winner_team, evt.score_a, evt.score_b);
                break;
            case NetworkClient::NetEventType::SCORE_UPDATE:
                Apply_ScoreUpdate(evt.score_a, evt.score_b, evt.player_count, evt.stats);
                break;
            case NetworkClient::NetEventType::TIMER_SYNC:
                Apply_TimerSync(evt.time_ms);
                break;
            case NetworkClient::NetEventType::ROSTER_INFO:
                Apply_RosterInfo(evt.roster_count, evt.roster);
                break;
            case NetworkClient::NetEventType::MAP_LOADED:
                Apply_MapLoaded(evt.map_slot);
                break;
            case NetworkClient::NetEventType::CHAR_SELECT:
                Apply_CharSelect(evt.char_select_player_id, evt.char_select_type);
                break;
            case NetworkClient::NetEventType::SPAWN_SELECT:
                Apply_SpawnSelect(evt.spawn_select_player_id, _float3(evt.spawn_x, evt.spawn_y, evt.spawn_z));
                break;
            case NetworkClient::NetEventType::HIT:
                Apply_Hit(evt.hit_shooter_id, evt.hit_victim_id, evt.hit_victim_hp,
                          evt.hit_part_num, evt.hit_pos);
                break;
            case NetworkClient::NetEventType::DEATH:
                Apply_Death(evt.hit_victim_id, evt.death_killer_id);
                break;
            case NetworkClient::NetEventType::WALL_BREAK:
                Apply_WallBreak(evt.wall_break_id);
                break;
            default:
                break;
            }
        }
    }

    // 선택 페이즈: 로컬 카운트다운으로 UI를 부드럽게 유지
    // SC_TIMER_SYNC 수신 시 Apply_TimerSync가 서버 값으로 보정
    Update_MouseClip();
    // 선택 구간(캐릭터/상황판/스폰) 동안에는 글로벌 타이머를 먼저 굴린다.
    //  0 이 되면 어느 단계든 즉시 PLAYING 으로 강제 진입.
    const _bool bSelectPhase =
        (m_ePhase == GAME_PHASE::PHASE_CHARSELECT) ||
        (m_ePhase == GAME_PHASE::PHASE_SCOREBOARD) ||
        (m_ePhase == GAME_PHASE::PHASE_SHOP);

    if (bSelectPhase)
        Tick_SelectTimer(fTimeDelta);

    switch (m_ePhase)
    {
    case GAME_PHASE::PHASE_CHARSELECT:
        Update_CharSelect(fTimeDelta);
        break;
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

    // 라운드 결과 배너 타이머 카운트다운 (SC_ROUND_END 수신 시 m_fResultBannerTimer=5.f)
    if (m_fResultBannerTimer > 0.f)
    {
        m_fResultBannerTimer -= fTimeDelta;
        if (m_fResultBannerTimer <= 0.f)
        {
            m_fResultBannerTimer = 0.f;
            Set_LayerVisible(L"Layer_UI_Result", false);
        }
    }

    Refresh_Timer();
}

// =====================================================================
//  단계 진입
// =====================================================================
void CGame_Manager::Enter_Phase(GAME_PHASE eNext)
{
    if (m_ePhase == eNext)
        return;

    m_ePhase = eNext;

    Apply_PhaseVisibility(eNext);

    switch (eNext)
    {
    case GAME_PHASE::PHASE_CHARSELECT: OnEnter_CharSelect(); break;
    case GAME_PHASE::PHASE_SCOREBOARD: OnEnter_Scoreboard(); break;
    case GAME_PHASE::PHASE_SHOP:       OnEnter_Shop();       break;
    case GAME_PHASE::PHASE_PLAYING:    OnEnter_Playing();    break;
    case GAME_PHASE::PHASE_GAMEOVER:   OnEnter_GameOver();   break;
    default: break;
    }
}

void CGame_Manager::OnEnter_CharSelect()
{
    Ready_CharSelect();   // 프리뷰 + 선택 UI 생성(최초 1회)

    // 내 플레이어(1인칭 액터)를 시작 지점에 미리 세워 둔다.
    //  → Ready 로 1인칭 전환될 때 이 위치 그대로 시작.
    Place_PlayerAt_Spot();

    Set_MouseCaptured(false);
    if (CController* p = m_pGameInstance->Get_Controller())
        p->Set_BlockInput(true);

    m_bCSReady = false;

    // 진입 시에는 아무 캐릭터도 선택되지 않은 상태(둘 다 숨김).
    //  유저가 칸을 클릭해야 해당 프리뷰가 나타난다.
    m_iCSMyCharacter = -1;
    if (m_pCSPreviewMe)    m_pCSPreviewMe->SetOnOff(false);
    if (m_pCSPreviewChick) m_pCSPreviewChick->SetOnOff(false);

    Refresh_CharSelectFaces();
}

void CGame_Manager::OnEnter_Scoreboard()
{
    m_fScoreboardTimer = 0.f;
    m_bSelectExpired   = false;
    m_fSelectTimer     = SCOREBOARD_TIMEOUT;  // SC_TIMER_SYNC 도착 전 표시용 초기값
    Reset_RoundLoadFlags();

    // 맵 로드 완료를 서버에 통보 → 전원 완료 시 서버가 SCOREBOARD→SHOP 전환
    const unsigned char myFlatSlot = static_cast<unsigned char>(m_iMyTeam * 3 + (m_iMyNumber - 1));
    NetworkClient::GetInstance()->Send_MapLoaded(myFlatSlot);

    // 매 라운드 시작 시 내 플레이어를 시작 지점(캐릭터 선택 때의 위치)으로 되돌린다.
    //  다음 라운드는 CharSelect 를 건너뛰고 곧장 스코어보드로 진입하므로,
    //  여기서 직접 세워주지 않으면 이전 라운드 종료 위치에 그대로 남는다.
    Place_PlayerAt_Spot();

    GM_Log(L"Round %d/%d - SCOREBOARD (현재 게임 상황) | Score %d : %d",
        m_iRound, MAX_ROUND, m_iTeamScore[0], m_iTeamScore[1]);

    // 최신 상태를 텍스트에 반영
    Refresh_Scoreboard();
}

void CGame_Manager::OnEnter_Shop()
{
    m_fShopTimer     = SHOP_DURATION;
    m_bSelectExpired = false;
    m_fSelectTimer   = SHOP_DURATION;  // SC_TIMER_SYNC 도착 전 표시용 초기값
    m_bShopOpen = true; // 진입 시 구매창을 띄운 상태로 시작

    /*if (USE_SHOP)
    {
        GM_Log(L"Round %d - SHOP (상품 구매) | %.0f초", m_iRound, SHOP_DURATION);
    }*/

    if (!USE_SHOP && m_pMapSelect != nullptr)
        m_pMapSelect->Clear_Selection();   // 이전 라운드 선택 초기화

    // 라운드 종료 후 재진입 시 WIN/LOSE 배너가 남아 있을 수 있으므로 확실히 숨긴다.
    // (자체 5초 타이머로 사라지는 경우도 있지만, 타이밍 경계값 보호용 안전장치)
    if (m_fResultBannerTimer > 0.f)
    {
        m_fResultBannerTimer = 0.f;
        Set_LayerVisible(L"Layer_UI_Result", false);
    }

    GM_Log(L"Round %d - SPAWN SELECT | %.0f초", m_iRound, SHOP_DURATION);

    // 상점에서는 클릭을 위해 커서를 보이게 하고, 플레이어 입력을 막는다.
    Set_ShopUIMode(true);
}

void CGame_Manager::OnEnter_Playing()
{
    m_bShopOpen = false;
    m_fRoundTimer = ROUND_DURATION;   // 서버 SC_ROUND_START 수신 시 정확한 값으로 보정됨

    GM_Log(L"Round %d - PLAYING", m_iRound);

    // 플레이 중에는 커서를 숨기고(조준 모드) 입력을 허용한다.
    Set_ShopUIMode(false);

    // 부활/포물선 발사는 SC_ROUND_START(Apply_RoundStart) 수신 시 처리한다.
    // 이유: 전멸 후 다음 라운드는 PHASE_PLAYING 유지 상태에서 SC_ROUND_START만 다시 오므로,
    //       OnEnter_Playing이 다시 호출되지 않는다. Apply_RoundStart에서 일괄 처리해야 한다.
    // (첫 라운드도 SC_ROUND_START가 항상 도착하므로 중복 발사 없음)

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

void CGame_Manager::Update_CharSelect(_float fTimeDelta)
{
    if (m_bCSReady)
    {
        Set_MouseCaptured(false);
        if (CController* p = m_pGameInstance->Get_Controller())
            p->Set_BlockInput(false);

        // Ready 클릭 시 서버에 CS_CHAR_SELECT + CS_PHASE_READY(0) 즉시 전송 (중복 방지)
        if (!m_bSelectExpired)
        {
            m_bSelectExpired = true;
            const unsigned char ct = (m_iCSMyCharacter >= 0) ? (unsigned char)m_iCSMyCharacter : 0u;
            NetworkClient::GetInstance()->Send_CharSelect(ct);
            NetworkClient::GetInstance()->Send_PhaseReady(0);
        }
        // 페이즈 전환은 서버 SC_PHASE_CHANGE(SCOREBOARD) 수신 대기
        return;
    }
    Handle_CharSelectClick();
}

void CGame_Manager::Update_Scoreboard(_float fTimeDelta)
{
    m_fScoreboardTimer += fTimeDelta;
    // 페이즈 전환은 서버 SC_PHASE_CHANGE 수신으로만 처리
}

void CGame_Manager::Update_Shop(_float fTimeDelta)
{
    if (USE_SHOP)
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

        // 구매 시간 카운트다운 → 게임 플레이로 (상점 모드에서만 사용)
        m_fShopTimer -= fTimeDelta;
        if (m_fShopTimer <= 0.f)
        {
            m_fShopTimer = 0.f;
            Enter_Phase(GAME_PHASE::PHASE_PLAYING);
        }
    }
    else
    {
        // 맵(스폰) 선택 단계: 단계 종료는 글로벌 타이머가 관리(여기선 전환 없음).
        //  E 키는 맵 선택 창의 표시/숨김만 토글한다(뒤로 게임 화면이 비침).
        if (m_pGameInstance->Key_Down(DIK_E))
        {
            m_bShopOpen = !m_bShopOpen;
            GM_Log(L"MapSelect %s", m_bShopOpen ? L"SHOW" : L"HIDE");
            Set_LayerVisible(L"Layer_UI_MapSelect", m_bShopOpen);
            Set_LayerVisible(L"Layer_UI_Crosshair", !m_bShopOpen);
            Set_ShopUIMode(m_bShopOpen); // 창이 보일 때만 커서 표시 + 입력 차단
        }
    }
}

void CGame_Manager::Update_Playing(_float fTimeDelta)
{
    // 결과 배너 표시 중에는 라운드 타이머를 멈춰 HUD 시계 튐 방지
    if (m_fResultBannerTimer <= 0.f)
    {
        // 남은 라운드 시간 카운트다운 (HUD 표시 전용 — 실제 종료는 서버 SC_ROUND_END로 제어)
        if (m_fRoundTimer > 0.f)
        {
            m_fRoundTimer -= fTimeDelta;
            if (m_fRoundTimer < 0.f)
                m_fRoundTimer = 0.f;
        }
    }
    Refresh_HUD();
    // 라운드 종료는 서버 SC_ROUND_END 수신으로만 처리
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


// =====================================================================
//  서버 주도 이벤트 (Phase 1)
// =====================================================================

void CGame_Manager::Apply_PhaseChange(unsigned char phase, unsigned char round)
{
    // 서버 페이즈 번호(0~4) → GAME_PHASE enum 변환
    static const GAME_PHASE s_map[] = {
        GAME_PHASE::PHASE_CHARSELECT,
        GAME_PHASE::PHASE_SCOREBOARD,
        GAME_PHASE::PHASE_SHOP,
        GAME_PHASE::PHASE_PLAYING,
        GAME_PHASE::PHASE_GAMEOVER,
    };
    if (phase >= 5) return;

    m_iRound = static_cast<_int>(round);
    Enter_Phase(s_map[phase]);
}

void CGame_Manager::Apply_RoundStart(unsigned char round, unsigned int duration_ms,
                                      unsigned int /*server_time_ms*/)
{
    // 라운드 번호는 SC_PHASE_CHANGE 에서 이미 설정. 타이머만 서버 값으로 맞춤.
    m_iRound      = static_cast<_int>(round);
    m_fRoundTimer = static_cast<_float>(duration_ms) / 1000.f;
    GM_Log(L"Round %d 시작 | 타이머 %.0fs", m_iRound, m_fRoundTimer);

    // 결과 배너가 떠 있으면 즉시 숨김 (서버가 다음 라운드를 열었으므로)
    if (m_fResultBannerTimer > 0.f)
    {
        m_fResultBannerTimer = 0.f;
        Set_LayerVisible(L"Layer_UI_Result", false);
    }

    // 전원 부활 (사망 상태·애니메이션 초기화, HP 500 복구)
    CController* pController = m_pGameInstance->Get_Controller();
    if (pController)
        pController->Revive_AllPlayers();

    // 선택한(또는 기본) 위치로 포물선 발사 (싱크대 → 식탁 연출)
    if (m_ePhase == GAME_PHASE::PHASE_PLAYING && !USE_SHOP)
        Apply_SpawnLaunch();

    Refresh_HUD();
}

void CGame_Manager::Apply_RoundEnd(unsigned char winner_team,
                                    unsigned char score_a, unsigned char score_b)
{
    m_iTeamScore[0]    = static_cast<_int>(score_a);
    m_iTeamScore[1]    = static_cast<_int>(score_b);
    m_iLastRoundWinner = static_cast<_int>(winner_team);
    GM_Log(L"서버 라운드 종료 | winner=%d  score %d:%d", winner_team, score_a, score_b);
    Refresh_HUD();  // 인게임 HUD 상단 승점 즉시 갱신

    // 화면 정중앙 WIN/LOSE/DRAW 배너 표시
    if (m_pResultText != nullptr)
    {
        const _wstring strResult =
            (winner_team == 2u)             ? L"DRAW"
          : (static_cast<_int>(winner_team) == m_iMyTeam) ? L"YOU WIN"
          :                                   L"YOU LOSE";

        const _float4 vColor =
            (strResult == L"YOU WIN") ? _float4(0.3f, 1.f, 0.3f, 1.f)   // 초록
          : (strResult == L"YOU LOSE") ? _float4(1.f, 0.3f, 0.3f, 1.f)  // 빨강
          :                              _float4(1.f, 1.f, 1.f, 1.f);    // 흰색(DRAW)

        m_pResultText->Set_Text(strResult);
        m_pResultText->Set_Color(vColor);
        Set_LayerVisible(L"Layer_UI_Result", true);
        m_fResultBannerTimer = 5.f;

        GM_Log(L"결과 배너: %s (내 팀=%d, 승리팀=%d)", strResult.c_str(), m_iMyTeam, winner_team);
    }
}

void CGame_Manager::Apply_TimerSync(unsigned int time_ms)
{
    _float t = static_cast<_float>(time_ms) / 1000.f;

    if (m_ePhase == GAME_PHASE::PHASE_PLAYING)
    {
        m_fRoundTimer = t;
    }
    else if (m_ePhase == GAME_PHASE::PHASE_CHARSELECT ||
             m_ePhase == GAME_PHASE::PHASE_SCOREBOARD  ||
             m_ePhase == GAME_PHASE::PHASE_SHOP)
    {
        m_fSelectTimer = t;
        // SC_TIMER_SYNC(0) = 서버 타이머 만료 신호 → CS_PHASE_READY 전송
        if (time_ms == 0 && !m_bSelectExpired)
        {
            m_bSelectExpired = true;
            unsigned char phase_byte =
                (m_ePhase == GAME_PHASE::PHASE_CHARSELECT) ? 0
              : (m_ePhase == GAME_PHASE::PHASE_SCOREBOARD) ? 1
              : 2; // SHOP
            // SHOP 만료 시 선택(또는 기본) 스폰 좌표를 TCP 순서 보장으로 PhaseReady 전에 전송
            if (m_ePhase == GAME_PHASE::PHASE_SHOP)
            {
                _float3 tgt = m_vDefaultSpawn;
                if (m_pMapSelect != nullptr && m_pMapSelect->Has_Selection())
                    tgt = m_pMapSelect->Get_SelectedWorld();
                float wp[3] = { tgt.x, tgt.y, tgt.z };
                NetworkClient::GetInstance()->Send_SpawnSelect(wp);
                // 본인 stat에도 즉시 반영 (에코 대기 없이)
                Apply_SpawnSelect(NetworkClient::GetInstance()->GetMyId(), tgt);
            }
            NetworkClient::GetInstance()->Send_PhaseReady(phase_byte);
        }
    }
}

void CGame_Manager::Apply_ScoreUpdate(unsigned char score_a, unsigned char score_b,
                                       unsigned char /*player_count*/,
                                       const PlayerStatBrief* /*stats*/)
{
    m_iTeamScore[0] = static_cast<_int>(score_a);
    m_iTeamScore[1] = static_cast<_int>(score_b);
    // Phase 3에서 개인 K/D/A stats 연동 예정
    Refresh_Scoreboard();
    Refresh_HUD();
}

void CGame_Manager::Apply_RosterInfo(unsigned char count, const RosterEntry* entries)
{
    Setup_DummyPlayers();  // 6슬롯 더미로 초기화 후 실제 이름 덮어쓰기

    auto* pNet = NetworkClient::GetInstance();
    const int myId = pNet->GetMyId();

    for (int i = 0; i < count && i < ROOM_MAX_PLAYER; ++i)
    {
        const auto& e = entries[i];

        // 좌석 선택이 있으면 team*3+(slot-1), 없으면 순서대로
        int iSlot;
        if (e.team <= 1 && e.slot >= 1 && e.slot <= 3)
            iSlot = e.team * 3 + (e.slot - 1);
        else
            iSlot = i;

        if (iSlot < 0 || iSlot >= (_int)m_vStats.size()) continue;

        m_vStats[iSlot].iPlayerId = e.player_id;

        if (e.player_id == myId)
        {
            m_vStats[iSlot].strName = L"Me";

            // 서버가 확정한 팀/슬롯으로 내 팀 정보를 권위적으로 갱신
            // (방 팀선택이 정상이면 기존 Set_MySpot_From_Setup 값과 일치;
            //  0xFF 폴백 배정 시에도 배너 WIN/LOSE 판정이 올바르게 동작)
            if (e.team <= 1)
            {
                m_iMyTeam   = static_cast<_int>(e.team);
                if (e.slot >= 1 && e.slot <= 3)
                    m_iMyNumber = static_cast<_int>(e.slot);
                GM_Log(L"Apply_RosterInfo: 내 팀=%d 슬롯=%d 확정", m_iMyTeam, m_iMyNumber);
            }
        }
        else
        {
            wchar_t wname[NAME_SIZE];
            mbstowcs_s(nullptr, wname, e.name, NAME_SIZE - 1);
            wname[NAME_SIZE - 1] = L'\0';
            m_vStats[iSlot].strName = wname;
        }
    }

    GM_Log(L"Apply_RosterInfo: %d 명 로스터 적용", count);
}

void CGame_Manager::Apply_MapLoaded(unsigned char slot)
{
    Notify_MapLoaded(static_cast<_int>(slot));
}

void CGame_Manager::Apply_CharSelect(int player_id, unsigned char char_type)
{
    for (auto& stat : m_vStats)
    {
        if (stat.iPlayerId == player_id)
        {
            stat.iCharType = static_cast<_int>(char_type);
            break;
        }
    }

    // 본인이 아닌 원격 플레이어만 — 선택한 캐릭터 모델로 재생성 요청
    if (player_id != NetworkClient::GetInstance()->GetMyId())
    {
        if (CController* pController = m_pGameInstance->Get_Controller())
            pController->Set_OtherPlayerCharType(player_id, char_type);
    }
}

void CGame_Manager::Apply_SpawnSelect(int player_id, _float3 vSpawnPos)
{
    for (auto& stat : m_vStats)
    {
        if (stat.iPlayerId == player_id)
        {
            stat.vSpawnPos = vSpawnPos;
            stat.bSpawnSet = true;
            break;
        }
    }
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

    // 슬롯 규칙: 0~2 = RED(팀0), 3~5 = BLUE(팀1). 팀 내 번호 = 슬롯%3 + 1.
    //  내 슬롯 = iMyTeam*3 + (iMyNumber-1). 그 자리에 "Me", 나머지는 더미.
    const _int iMySlot = m_iMyTeam * 3 + (m_iMyNumber - 1);

    for (_int i = 0; i < 6; ++i)
    {
        PLAYER_STAT stat;
        stat.iSlot     = i;
        stat.iPlayerId = -1;   // Apply_RosterInfo에서 실제 ID로 교체
        stat.iCharType = 0;    // 기본 Pig
        stat.iTeam = (i < 3) ? 0 : 1;   // 앞 3명 RED, 뒤 3명 BLUE

        if (i == iMySlot)
        {
            stat.strName = L"Me";
        }
        else
        {
            // 같은 팀이면 Ally, 다른 팀이면 Enemy 로 번호 붙여 표시
            const _int iNumInTeam = (i % 3) + 1;
            wchar_t buf[32];
            if (stat.iTeam == m_iMyTeam)
                swprintf_s(buf, _countof(buf), L"Ally_%d", iNumInTeam);
            else
                swprintf_s(buf, _countof(buf), L"Enemy_%d", iNumInTeam);
            stat.strName = buf;
        }

        stat.iKill = 0;
        stat.iDeath = 0;
        stat.iAssist = 0;
        stat.iMoney = 800;               // 시작 자금(더미)
        stat.bMapLoaded = false;
        m_vStats.push_back(stat);
    }

    // 본인 슬롯은 즉시 로드 완료 처리(테스트 편의)
    if (iMySlot >= 0 && iMySlot < (_int)m_vStats.size())
        m_vStats[iMySlot].bMapLoaded = true;
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
//  Phase 별 레이어 가시성 (손토글 전부 여기로 일원화)
// =====================================================================
void CGame_Manager::Apply_PhaseVisibility(GAME_PHASE ePhase)
{
    const _bool bCharSel = (ePhase == GAME_PHASE::PHASE_CHARSELECT);
    const _bool bScore = (ePhase == GAME_PHASE::PHASE_SCOREBOARD);
    const _bool bSpawn = (ePhase == GAME_PHASE::PHASE_SHOP);     // 스폰(맵) 선택
    const _bool bPlaying = (ePhase == GAME_PHASE::PHASE_PLAYING);

    // 실제 액터(1인칭/3인칭): 캐릭터 선택 때만 프리뷰로 대체하여 숨김.
    const _bool bWorldActors = !bCharSel;
    Set_LayerVisible(L"Layer_Player", bWorldActors);
    Set_LayerVisible(L"Layer_Other_Player", bWorldActors);

    // 캐릭터 선택 화면
    Set_LayerVisible(L"Layer_CharSelect_Preview", bCharSel);
    Set_LayerVisible(L"Layer_CharSelect_UI", bCharSel);

    // 상황판(스코어보드)
    Set_LayerVisible(L"Layer_UI_Scoreboard", bScore);

    // 스폰(맵) 선택 창 — 진입 시 ON. 이후 E 키로 런타임 토글(Update_Shop).
    Set_LayerVisible(L"Layer_UI_MapSelect", bSpawn);

    // 인게임 HUD / 미니맵 — 플레이 중에만
    Set_LayerVisible(L"Layer_UI_HUD", bPlaying);
    Set_LayerVisible(L"Layer_UI_MiniMap", bPlaying);

    // 에임: 플레이 중엔 항상 ON. 스폰 선택 단계에선 "창이 꺼졌을 때만" 보이므로
    //  여기선 OFF 로 두고, 창 토글(Update_Shop)에서 창과 반대로 켠다.
    Set_LayerVisible(L"Layer_UI_Crosshair", bPlaying);

    // 레거시 상점(USE_SHOP=false): 항상 OFF.
    Set_LayerVisible(L"Layer_UI_Shop", false);

    // Layer_UI_Timer 는 항상 ON: 여기서 건드리지 않는다.
}

// =====================================================================
//  통합 타이머 (모든 Phase 공통)
//   - 위치/색은 기존 HUD 중앙 타이머와 동일하게 잡아 PLAYING 화면을 유지.
//   - 내용만 Phase 에 따라 선택타이머 / 라운드타이머로 바꿔 끼운다.
// =====================================================================
HRESULT CGame_Manager::Ready_Timer()
{
    const _uint PROTO = LEVEL_STATIC;
    const _uint LV = LEVEL_GAMEPLAY;
    const _wstring TM = L"Layer_UI_Timer";

    CUI_Text::UI_TEXT_DESC d;
    d.fX = 640.f; d.fY = 30.f;           // 상단 중앙(기존 HUD 타이머 자리)
    d.fDepth = 0.2f;
    d.strText = Make_SelectTimerString();
    d.strFontTag = L"Font_Default";
    d.vColor = _float4(1.f, 1.f, 1.f, 1.f);
    d.fTextScale = 0.85f;
    d.bCentered = true;
    m_pTimerText = static_cast<CUI_Text*>(
        m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
            PROTO, L"Prototype_GameObject_UI_Text", LV, TM, &d));

    return S_OK;   // 항상 ON. Apply_PhaseVisibility 에서 끄지 않는다.
}

void CGame_Manager::Refresh_Timer()
{
    if (m_pTimerText == nullptr)
        return;

    _wstring s;
    switch (m_ePhase)
    {
    case GAME_PHASE::PHASE_CHARSELECT:
    case GAME_PHASE::PHASE_SCOREBOARD:
    case GAME_PHASE::PHASE_SHOP:        // 스폰 선택 (게임 시작 전 30초 구간)
        s = Make_SelectTimerString();
        break;
    case GAME_PHASE::PHASE_PLAYING:     // 라운드 시간 (100초 = 1:40)
        s = Make_TimerString();
        break;
    default:                            // GAMEOVER 등
        s = Make_TimerString();
        break;
    }
    m_pTimerText->Set_Text(s);
}

// =====================================================================
//  UI 텍스트 (캐릭터 선택)
// =====================================================================
HRESULT CGame_Manager::Ready_CharSelect()
{
    if (m_bCSBuilt) return S_OK;
    m_bCSBuilt = true;

    const _uint PROTO_UI = LEVEL_STATIC;
    const _uint LV = LEVEL_GAMEPLAY;
    const _wstring PV = L"Layer_CharSelect_Preview";
    const _wstring UI = L"Layer_CharSelect_UI";

    // ---- 3D 프리뷰 (나 1명만) ----
    //  네트워크 연동 시 옆자리 플레이어는 각자 자기 캐릭터로 보이므로
    //  여기서는 "나"만 시작 지점에 세운다.
    {
        CCharSelect_Pig::CHARSELECT_PIG_DESC d;
        d.vPos = m_vMySpot;                       // 내 팀/번호 시작 지점
        d.vRotation = _float3(0.f, XM_PI, 0.f);   // 카메라 보도록
        d.vScale = _float3(1.f, 1.f, 1.f);
        d.strModelTag = L"Prototype_Component_Pig_3rd";
        d.iModelLevelIndex = LEVEL_GAMEPLAY;
        d.iAnimIndex = 0;
        m_pCSPreviewMe = static_cast<CCharSelect_Pig*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                LV, L"Prototype_GameObject_CharSelect_Pig", LV, PV, &d));
        if (m_pCSPreviewMe) m_pCSPreviewMe->SetOnOff(false);   // 진입 시 선택값에 맞춰 표시
    }

    // ---- 3D 프리뷰 (Chick, 같은 자리) ----
    //  Chick 칸(1번) 클릭 시에만 보이도록 시작은 숨김 처리.
    {
        CCharSelect_Chick::CHARSELECT_CHICK_DESC d;
        d.vPos = m_vMySpot;
        d.vRotation = _float3(0.f, XM_PI, 0.f);
        d.vScale = _float3(1.f, 1.f, 1.f);
        d.strModelTag = L"Prototype_Component_Chick_3rd";
        d.iModelLevelIndex = LEVEL_GAMEPLAY;
        d.iAnimIndex = 8;   // 닭 idle
        m_pCSPreviewChick = static_cast<CCharSelect_Chick*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                LV, L"Prototype_GameObject_CharSelect_Chick", LV, PV, &d));
    }

    // ---- 3D 프리뷰 (Fish, 같은 자리) ----
    //  Fish 칸(2번) 클릭 시에만 보이도록 시작은 숨김 처리.
    {
        CCharSelect_Fish::CHARSELECT_FISH_DESC d;
        d.vPos = m_vMySpot;
        d.vRotation = _float3(0.f, XM_PI, 0.f);
        d.vScale = _float3(1.f, 1.f, 1.f);
        d.strModelTag = L"Prototype_Component_Fish_3rd";
        d.iModelLevelIndex = LEVEL_GAMEPLAY;
        d.iAnimIndex = 8;   // 물고기 idle
        m_pCSPreviewFish = static_cast<CCharSelect_Fish*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                LV, L"Prototype_GameObject_CharSelect_Fish", LV, PV, &d));
    }

    // 시작 표시 상태를 명시: 기본 선택은 Pig(0) → Pig 만 보이고 나머지는 숨김.
    m_iCSMyCharacter = 0;
    if (m_pCSPreviewMe)    m_pCSPreviewMe->SetOnOff(true);
    if (m_pCSPreviewChick) m_pCSPreviewChick->SetOnOff(false);
    if (m_pCSPreviewFish)  m_pCSPreviewFish->SetOnOff(false);

    auto AddText = [&](float x, float y, const _wstring& s, float scale)
        {
            CUI_Text::UI_TEXT_DESC d;
            d.fX = x; d.fY = y; d.fDepth = 0.2f;
            d.strText = s; d.strFontTag = L"Font_Default";
            d.vColor = _float4(1, 1, 1, 1); d.fTextScale = scale; d.bCentered = true;
            return m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(PROTO_UI, L"Prototype_GameObject_UI_Text", LV, UI, &d);
        };
    auto AddPanel = [&](float x, float y, float w, float h, _float4 col, float depth) -> CUI_Panel*
        {
            CUI_Panel::UI_PANEL_DESC d;
            d.fX = x; d.fY = y; d.fSizeX = w; d.fSizeY = h; d.fDepth = depth; d.vColor = col;
            return static_cast<CUI_Panel*>(
                m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(PROTO_UI, L"Prototype_GameObject_UI_Panel", LV, UI, &d));
        };
    auto AddFace = [&](float x, float y, float w, float h, float depth, const _wstring& texTag) -> CUI_Texture*
        {
            CUI_Texture::UI_TEXTURE_DESC d;
            d.fX = x; d.fY = y; d.fSizeX = w; d.fSizeY = h; d.fDepth = depth;
            d.strTextureProtoTag = texTag;
            d.iTextureLevelIndex = LV;   // 텍스처는 LEVEL_GAMEPLAY 에 등록
            return static_cast<CUI_Texture*>(
                m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                    PROTO_UI, L"Prototype_GameObject_UI_Texture", LV, UI, &d));
        };

    AddText(640.f, 70.f, L"Character Select", 1.4f);

    const float FW = 84.f, FH = 84.f, FGAP = 24.f, FY = 596.f;
    const float FTOT = 3 * FW + 2 * FGAP;
    const float FX0 = (1280.f - FTOT) * 0.5f;
    auto FaceX = [&](int i) { return FX0 + i * (FW + FGAP); };

    AddPanel(FX0 - 20.f, FY - 20.f, FTOT + 40.f, FH + 40.f, _float4(0.08f, 0.10f, 0.14f, 0.78f), 0.7f); // 바 배경
    const _wstring caps[3] = {L"Pig", L"Chick", L"Fish"};
    const _wstring faceTex[3] = {
        L"Prototype_Component_Texture_SelectPig",
        L"Prototype_Component_Texture_SelectChick",
        L"Prototype_Component_Texture_SelectFish"
    };
    const float RING = 3.f;   // 패널을 아이콘보다 이만큼 크게 → 선택 링
    for (int i = 0; i < 3; ++i)
    {
        // 뒤 패널(선택 링/배경). 아이콘보다 RING 만큼 크게.
        m_pCSFacePanel[i] = AddPanel(FaceX(i) - RING, FY - RING, FW + 2.f * RING, FH + 2.f * RING,
            _float4(0.4f, 0.4f, 0.42f, 0.95f), 0.4f);
        m_vCSFaceRect[i] = _float4(FaceX(i), FY, FW, FH);   // 클릭 판정은 아이콘 영역 기준

        // 칸을 꽉 채우는 아이콘(패널 0.4 앞 0.3). 포인터 저장 → 틴트 대상.
        if (!faceTex[i].empty())
            m_pCSFaceIcon[i] = AddFace(FaceX(i), FY, FW, FH, 0.3f, faceTex[i]);

        AddText(FaceX(i) + FW * 0.5f, FY + FH + 12.f, caps[i], 0.7f);
    }

    const float RW = 200.f, RH = 56.f, RX = 1280.f - RW - 40.f, RY = 720.f - RH - 40.f;
    AddPanel(RX, RY, RW, RH, _float4(40 / 255.f, 130 / 255.f, 80 / 255.f, 0.95f), 0.5f);
    m_vCSReadyRect = _float4(RX, RY, RW, RH);
    AddText(RX + RW * 0.5f, RY + RH * 0.5f - 2.f, L"Ready", 0.95f);

    return S_OK;
}

void CGame_Manager::Handle_CharSelectClick()
{
    if (!m_pGameInstance->Mouse_Down(Engine::DIM_LB))
        return;

    POINT pt;
    if (!GetCursorPos(&pt)) return;
    if (g_hWnd != nullptr) ScreenToClient(g_hWnd, &pt);

    _float mx = (_float)pt.x, my = (_float)pt.y;   // 클라이언트 → 1280x720 환산 (상점과 동일)
    if (g_hWnd != nullptr)
    {
        RECT rc;
        if (GetClientRect(g_hWnd, &rc))
        {
            const _float cw = (_float)(rc.right - rc.left), ch = (_float)(rc.bottom - rc.top);
            if (cw > 0.f && ch > 0.f) { mx = (_float)pt.x * (1280.f / cw); my = (_float)pt.y * (720.f / ch); }
        }
    }

    auto In = [&](const _float4& r) { return mx >= r.x && mx <= r.x + r.z && my >= r.y && my <= r.y + r.w; };

    for (int i = 0; i < 3; ++i)
        if (In(m_vCSFaceRect[i]))
        {
            m_iCSMyCharacter = i;
            // 0 = Pig, 1 = Chick, 2 = Fish. 선택한 칸의 프리뷰만 표시.
            if (m_pCSPreviewMe)    m_pCSPreviewMe->SetOnOff(i == 0);
            if (m_pCSPreviewChick) m_pCSPreviewChick->SetOnOff(i == 1);
            if (m_pCSPreviewFish)  m_pCSPreviewFish->SetOnOff(i == 2);
            Refresh_CharSelectFaces();
            return;
        }

    if (In(m_vCSReadyRect))
        m_bCSReady = true;   // Update_CharSelect 에서 전환
}

void CGame_Manager::Refresh_CharSelectFaces()
{
    // 선택 = 원본 밝기(또렷) / 비선택 = 강하게 딤.  뒤 패널은 선택 링.
    const _float4 ICON_SEL = _float4(1.f, 1.f, 1.f, 1.f);   // 원본 색 그대로
    const _float4 ICON_DIM = _float4(0.38f, 0.38f, 0.42f, 1.f);   // 어둡게(비선택)
    const _float4 RING_SEL = _float4(1.f, 0.86f, 0.30f, 1.f);   // 노란 하이라이트 링
    const _float4 RING_DIM = _float4(0.16f, 0.17f, 0.20f, 0.95f); // 어두운 테두리

    for (int i = 0; i < 3; ++i)
    {
        const bool sel = (i == m_iCSMyCharacter);

        if (m_pCSFacePanel[i])
            m_pCSFacePanel[i]->Set_Color(sel ? RING_SEL : RING_DIM);

        if (m_pCSFaceIcon[i])                 // Pig/Chick: 밝기로 선택 표시
            m_pCSFaceIcon[i]->Set_Color(sel ? ICON_SEL : ICON_DIM);
        // Fish(2번)는 아직 아이콘 텍스처가 없어 패널 색(노랑/어둠)만으로 선택 표시됨
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

    // ---- 화면 위 "남은 시간" (글로벌 타이머, 캐릭터선택/스폰선택과 공유) ----
    {
        CUI_Text::UI_TEXT_DESC d;
        d.fX = 640.f; d.fY = 24.f;
        d.fDepth = 0.3f;
        d.strText = Make_SelectTimerString();
        d.strFontTag = L"Font_Default";
        d.vColor = _float4(1.f, 0.95f, 0.6f, 1.f);
        d.fTextScale = 0.85f;
        d.bCentered = true;
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

    // 하단 HUD(HP/탄약)도 함께 갱신
    Refresh_PlayerHUD();
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
//  라운드 결과 배너 (화면 정중앙 WIN/LOSE/DRAW)
//   - Layer_UI_Result (Apply_PhaseVisibility 대상 아님 — 수동 토글)
//   - 반투명 배경 패널 + 중앙 정렬 텍스트 1개
//   - Apply_RoundEnd 수신 시 표시, 5초 후(또는 SC_ROUND_START 수신 시) 숨김
// =====================================================================
HRESULT CGame_Manager::Ready_ResultBanner()
{
    const _uint PROTO = LEVEL_STATIC;
    const _uint LV    = LEVEL_GAMEPLAY;
    const _wstring LAYER = L"Layer_UI_Result";

    const _float fCX = 640.f;   // 화면 중앙 X
    const _float fCY = 360.f;   // 화면 중앙 Y

    // ── 반투명 배경 패널 (가로 400, 세로 120) ──────────────────────
    {
        CUI_Panel::UI_PANEL_DESC d;
        d.fX     = fCX - 200.f;   // 좌상단 X
        d.fY     = fCY - 60.f;    // 좌상단 Y
        d.fSizeX = 400.f;
        d.fSizeY = 120.f;
        d.fDepth = 0.5f;           // HUD보다 앞(depth 낮을수록 앞)
        d.vColor = _float4(0.f, 0.f, 0.f, 0.65f);   // 반투명 검정
        m_pResultPanel = static_cast<CUI_Panel*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                PROTO, L"Prototype_GameObject_UI_Panel", LV, LAYER, &d));
    }

    // ── 중앙 정렬 결과 텍스트 ──────────────────────────────────────
    {
        CUI_Text::UI_TEXT_DESC d;
        d.fX         = fCX;
        d.fY         = fCY - 20.f;  // 패널 세로 중앙 근처
        d.fDepth     = 0.45f;
        d.strText    = L"YOU WIN";
        d.strFontTag = L"Font_Default";
        d.vColor     = _float4(0.3f, 1.f, 0.3f, 1.f);   // 초록(기본)
        d.fTextScale = 1.6f;
        d.bCentered  = true;
        m_pResultText = static_cast<CUI_Text*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                PROTO, L"Prototype_GameObject_UI_Text", LV, LAYER, &d));
    }

    // 생성 직후 숨김
    Set_LayerVisible(LAYER, false);

    return S_OK;
}

// =====================================================================
//  하단 HUD (내 HP / 남은 탄약)
//   - 좌하단 HP, 우하단 탄약을 텍스트로 표시. (이후 텍스처로 교체 예정)
//   - 상단 HUD와 같은 Layer_UI_HUD 에 올려 PLAYING 단계에서만 보이게 한다.
//   - 좌표는 1280x720 디자인 기준.
// =====================================================================
HRESULT CGame_Manager::Ready_PlayerHUD()
{
    const _uint PROTO = LEVEL_STATIC;
    const _uint LV = LEVEL_GAMEPLAY;
    const _wstring HUD = L"Layer_UI_HUD";

    // ---- 좌하단 HP ----
    {
        CUI_Text::UI_TEXT_DESC d;
        d.fX = 60.f; d.fY = 660.f;
        d.fDepth = 0.3f;
        d.strText = L"HP 100";
        d.strFontTag = L"Font_Default";
        d.vColor = _float4(0.4f, 1.f, 0.5f, 1.f);   // 초록(체력)
        d.fTextScale = 0.9f;
        d.bCentered = false;
        m_pHUDHealthText = static_cast<CUI_Text*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                PROTO, L"Prototype_GameObject_UI_Text", LV, HUD, &d));
    }

    // ---- 우하단 탄약 ----
    {
        CUI_Text::UI_TEXT_DESC d;
        d.fX = 1100.f; d.fY = 660.f;
        d.fDepth = 0.3f;
        d.strText = L"30 / 30";
        d.strFontTag = L"Font_Default";
        d.vColor = _float4(1.f, 0.95f, 0.6f, 1.f);  // 노랑(탄약)
        d.fTextScale = 0.9f;
        d.bCentered = false;
        m_pHUDAmmoText = static_cast<CUI_Text*>(
            m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                PROTO, L"Prototype_GameObject_UI_Text", LV, HUD, &d));
    }

    return S_OK;
}

void CGame_Manager::Refresh_PlayerHUD()
{
    // 내 플레이어를 컨트롤러에서 받아 현재 HP/탄약을 읽는다.
    CController* pController = m_pGameInstance->Get_Controller();
    CPlayer_1rd* pPlayer = pController ? pController->Get_Player() : nullptr;
    if (pPlayer == nullptr)
        return;

    if (m_pHUDHealthText != nullptr)
    {
        wchar_t buf[32];
        swprintf_s(buf, _countof(buf), L"HP %d", pPlayer->Get_Health());
        m_pHUDHealthText->Set_Text(buf);
    }

    if (m_pHUDAmmoText != nullptr)
    {
        wchar_t buf[32];
        if (pPlayer->Is_Reloading())
            swprintf_s(buf, _countof(buf), L"RELOADING");
        else
            swprintf_s(buf, _countof(buf), L"%u / %u",
                pPlayer->Get_Ammo(), pPlayer->Get_MaxAmmo());
        m_pHUDAmmoText->Set_Text(buf);
    }
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
    Set_MouseCaptured(!bOpen);

    // 2) 플레이어 입력 차단: 창이 열려 있는 동안 시점/이동/사격을 막는다.
    CController* pController = m_pGameInstance->Get_Controller();
    if (pController != nullptr)
        pController->Set_BlockInput(bOpen);
}

// =====================================================================
//  맵 선택 창 (상점 대신 사용)
// =====================================================================
void CGame_Manager::Cache_MapSelect()
{
    // Level_Gameplay 에서 Layer_UI_MapSelect 로 만들어 둔 CMapSelect 를 찾는다.
    m_pMapSelect = nullptr;
    list<CGameObject*> objs = m_pGameInstance->Get_List(LEVEL_GAMEPLAY, L"Layer_UI_MapSelect");
    for (auto* pObj : objs)
    {
        CMapSelect* pMS = dynamic_cast<CMapSelect*>(pObj);
        if (pMS != nullptr)
        {
            m_pMapSelect = pMS;
            break;
        }
    }

    // ---- 스폰 선택 화면 상단 "남은 시간"(글로벌 타이머 공유) + 확정 안내 ----
    //  같은 레이어(Layer_UI_MapSelect)에 올려 화면 전환과 함께 표시/숨김되게 함.
    {
        const _uint PROTO = LEVEL_STATIC;
        const _uint LV = LEVEL_GAMEPLAY;
        const _wstring MS = L"Layer_UI_MapSelect";

        CUI_Text::UI_TEXT_DESC d;
        d.fX = 640.f; d.fY = 24.f;
        d.fDepth = 0.3f;
        d.strText = Make_SelectTimerString();
        d.strFontTag = L"Font_Default";
        d.vColor = _float4(1.f, 0.95f, 0.6f, 1.f);
        d.fTextScale = 0.85f;
        d.bCentered = true;

        CUI_Text::UI_TEXT_DESC h;
        h.fX = 640.f; h.fY = 690.f;
        h.fDepth = 0.3f;
        h.strText = L"Click spawn point   |   [E] Toggle window";
        h.strFontTag = L"Font_Default";
        h.vColor = _float4(0.9f, 0.95f, 1.f, 1.f);
        h.fTextScale = 0.6f;
        h.bCentered = true;
        m_pGameInstance->Add_GameObject_ToLayer(PROTO, L"Prototype_GameObject_UI_Text", LV, MS, &h);
    }
}

void CGame_Manager::Apply_SpawnLaunch()
{
    // 컨트롤러에서 내 플레이어를 받는다.
    CController* pController = m_pGameInstance->Get_Controller();
    CPlayer_1rd* pPlayer = pController ? pController->Get_Player() : nullptr;
    if (pPlayer == nullptr)
        return;

    // 선택된 위치가 있으면 그곳, 없으면 식탁 윗면 기준 기본 위치.
    _float3 vTarget = m_vDefaultSpawn;
    if (m_pMapSelect != nullptr && m_pMapSelect->Has_Selection())
        vTarget = m_pMapSelect->Get_SelectedWorld();
    else if (m_pMapSelect != nullptr)
        vTarget.y = m_pMapSelect->Get_TableTopY(); // 식탁 윗면에 맞게 y 보정

    // 로컬 플레이어: 팀 테이블 위치(m_vMySpot)에서 선택 지점까지 포물선 발사
    // (싱크대 하드코딩 제거 — CHARSELECT 스테이징과 동일한 테이블에서 출발)
    pPlayer->Set_Position(m_vMySpot);
    pPlayer->Launch_To(vTarget, SPAWN_ARC_HEIGHT);

    GM_Log(L"Spawn launch: table(%.1f,%.1f,%.1f) → target(%.1f,%.1f,%.1f) %s",
        m_vMySpot.x, m_vMySpot.y, m_vMySpot.z,
        vTarget.x, vTarget.y, vTarget.z,
        (m_pMapSelect && m_pMapSelect->Has_Selection()) ? L"[선택]" : L"[기본]");

    // 원격 플레이어: 각자 팀 테이블에서 각자 선택 지점으로 발사
    // stat.iSlot = 전역 0..5, 팀 내 번호 = iSlot - team*3 + 1
    CController* pCtrl = m_pGameInstance->Get_Controller();
    int myId = NetworkClient::GetInstance()->GetMyId();
    for (auto& stat : m_vStats)
    {
        if (stat.iPlayerId < 0 || stat.iPlayerId == myId) continue;

        // 이 플레이어의 팀 테이블 스테이징 좌표를 계산
        MATCH_SETUP setup;
        setup.iTeam   = (stat.iTeam == 1) ? 1 : 0;
        setup.iNumber = stat.iSlot - stat.iTeam * 3 + 1; // 전역 0..5 → 팀 내 1..3
        XMFLOAT3 s   = setup.Get_TableSpot();
        _float3 vStart = { s.x, s.y, s.z };

        _float3 tgt = stat.bSpawnSet ? stat.vSpawnPos : m_vDefaultSpawn;
        if (!stat.bSpawnSet && m_pMapSelect != nullptr)
            tgt.y = m_pMapSelect->Get_TableTopY();
        if (pCtrl)
        {
            pCtrl->Teleport_OtherPlayer(stat.iPlayerId, vStart);
            pCtrl->Launch_OtherPlayer(stat.iPlayerId, tgt, SPAWN_ARC_HEIGHT);
        }
        GM_Log(L"Remote spawn launch [id=%d]: table(%.1f,%.1f,%.1f) → (%.1f, %.1f, %.1f) %s",
            stat.iPlayerId, vStart.x, vStart.y, vStart.z,
            tgt.x, tgt.y, tgt.z,
            stat.bSpawnSet ? L"[선택]" : L"[기본]");
    }
}

// =====================================================================
//  선택 구간 글로벌 타이머 + 시작 지점
// =====================================================================
void CGame_Manager::Set_MySpot_From_Setup()
{
    // 대기방에서 고른 팀/번호를 읽어 시작 지점을 계산.
    m_iMyTeam = (g_MatchSetup.iTeam == 1) ? 1 : 0;
    m_iMyNumber = g_MatchSetup.iNumber;
    if (m_iMyNumber < 1) m_iMyNumber = 1;
    if (m_iMyNumber > 3) m_iMyNumber = 3;

    // ── 스테이징 지점: 원형테이블 양 옆 팀/슬롯 정렬 ─────────────────────────
    //   RED(0)=Bar 앞쪽, BLUE(1)=Shelf_floor_4 뒤쪽 — 원형테이블을 사이에 두고 마주봄
    //   CHARSELECT/SCOREBOARD 단계에서 플레이어가 서 있을 위치.
    //   서버 RoomPhaseManager::ApplyTeamSpawnPositions 와 동일한 수치 사용.
    XMFLOAT3 table = g_MatchSetup.Get_TableSpot();
    m_vMySpot = _float3(table.x, table.y, table.z);
    m_fMyYaw  = g_MatchSetup.Get_TableYaw();  // 상대 팀 방향을 바라봄

    // ── launch fallback: PLAYING 진입 시 스폰 선택 안 했을 때 날아갈 바닥 위치 ──
    //   SHOP 종료 시 항상 Send_SpawnSelect 를 전송하므로 실제로는 이 값이
    //   사용될 일이 거의 없지만 안전망으로 바닥 좌표를 유지한다.
    XMFLOAT3 floor = g_MatchSetup.Get_SpawnSpot();
    m_vDefaultSpawn = _float3(floor.x, floor.y, floor.z);

    GM_Log(L"MatchSetup → Team %s, No.%d | Table(%.1f,%.1f,%.1f) Yaw %.2f | Floor(%.1f,%.1f,%.1f)",
        (m_iMyTeam == 0) ? L"RED" : L"BLUE", m_iMyNumber,
        m_vMySpot.x, m_vMySpot.y, m_vMySpot.z, m_fMyYaw,
        m_vDefaultSpawn.x, m_vDefaultSpawn.y, m_vDefaultSpawn.z);
}

void CGame_Manager::Place_PlayerAt_Spot()
{
    // 내 플레이어 transform 을 시작 지점에 즉시 세운다(캐릭터 선택 진입 시).
    CController* pController = m_pGameInstance->Get_Controller();
    CPlayer_1rd* pPlayer = pController ? pController->Get_Player() : nullptr;
    if (pPlayer != nullptr)
    {
        pPlayer->Set_Position(m_vMySpot);
        pPlayer->Set_Facing(m_fMyYaw);   // 원점을 바라보도록
    }
}

void CGame_Manager::Tick_SelectTimer(_float fTimeDelta)
{
    if (m_fSelectTimer > 0.f)
    {
        m_fSelectTimer -= fTimeDelta;
        if (m_fSelectTimer < 0.f)
            m_fSelectTimer = 0.f;
    }
}

_wstring CGame_Manager::Make_SelectTimerString() const
{
    // 올림(ceil) 처리: 0.1초라도 남아 있으면 1로 표시.
    _float t = (m_fSelectTimer > 0.f) ? m_fSelectTimer : 0.f;
    _int iSec = (_int)t;
    if (t > (_float)iSec) ++iSec;
    wchar_t buf[32];
    swprintf_s(buf, _countof(buf), L"TIME  %d", iSec);
    return buf;
}


// =====================================================================

void CGame_Manager::Set_MouseCaptured(_bool bCaptured)
{
    m_bMouseCaptured = bCaptured;
    SetCursorVisible(!bCaptured);   // 잡으면 숨김, 풀면 표시 (전환 시 1회)
    Update_MouseClip();             // 클립 상태 즉시 반영
}

void CGame_Manager::Update_MouseClip()
{
    // 우리 창이 포커스를 가진 동안에만 가둔다.
    //  → Alt-Tab/다른 창 전환 시 자동으로 풀려 커서가 자유로워진다.
    if (m_bMouseCaptured && GetForegroundWindow() == g_hWnd)
        ClipCursorToClient(g_hWnd);
    else
        ClipCursor(nullptr);
}


CGame_Manager* CGame_Manager::Create()
{
    CGame_Manager* pInstance = new CGame_Manager();
    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Create : CGame_Manager");
        Safe_Release(pInstance);
        return nullptr;
    }
    m_pInstance = pInstance;
    return pInstance;
}

void CGame_Manager::Apply_Hit(int shooter_id, int victim_id, short victim_hp,
                               unsigned char /*part_num*/, const float hit_pos[3])
{
    // 피격 파티클 (모든 클라이언트 동일 위치)
    if (CParticle_System* pPS = m_pGameInstance->Get_ParticleSystem())
    {
        _float3 vPos = { hit_pos[0], hit_pos[1], hit_pos[2] };

        CParticle_System::EMIT_DESC spray;
        spray.eType    = CParticle_System::KETCHUP_SPRAY;
        spray.vCenter  = vPos;
        spray.vExtents = { 0.1f, 0.1f, 0.1f };
        spray.iCount   = 16;
        pPS->Emit(spray);
    }

    // 내 HP 업데이트 (서버 권위 값으로 덮어씀)
    auto* pNet = NetworkClient::GetInstance();
    if (victim_id == pNet->GetMyId())
    {
        CController* pCtrl = m_pGameInstance->Get_Controller();
        CPlayer_1rd* pPlayer = pCtrl ? pCtrl->Get_Player() : nullptr;
        if (pPlayer)
            pPlayer->Set_Health(static_cast<_int>(victim_hp));
    }
}

void CGame_Manager::Apply_Death(int victim_id, int /*killer_id*/)
{
    auto* pNet  = NetworkClient::GetInstance();
    auto* pCtrl = m_pGameInstance->Get_Controller();
    if (!pCtrl) return;

    if (victim_id == pNet->GetMyId())
    {
        CPlayer_1rd* pPlayer = pCtrl->Get_Player();
        if (pPlayer && !pPlayer->Get_Die())
            pPlayer->Die(0.f);
    }
    else
    {
        pCtrl->Kill_OtherPlayer(victim_id);
    }
}

void CGame_Manager::Apply_WallBreak(int wall_id)
{
    // SC_WALL_BREAK 수신 시 호출 — Layer_Map 을 순회해 wall_id 가 일치하는 벽을 찾아 Break()
    // Break() 내부에 m_bBroken 멱등 가드가 있어 중복 호출해도 안전하다.
    list<CGameObject*> objs = m_pGameInstance->Get_List(LEVEL_GAMEPLAY, L"Layer_Map");
    for (auto* pObj : objs)
    {
        CMap* pMap = dynamic_cast<CMap*>(pObj);
        if (pMap && pMap->Is_Breakable() && pMap->Get_WallId() == (_uint)wall_id)
        {
            pMap->Break();
            break;
        }
    }
}

void CGame_Manager::Free()
{
    // UI_Text 들은 레이어가 소유(Return_Obj 가 AddRef 안 함).
    // 여기선 Release 하지 않고 참조만 끊는다. (레벨 해제 시 레이어가 실제 해제)
    m_pScoreText = nullptr;
    for (_int i = 0; i < MAX_PLAYER; ++i)
        m_pPlayerRowText[i] = nullptr;

    // HUD 도 레이어 소유. 참조만 끊는다.
    m_pHUDTeamScoreText[0] = nullptr;
    m_pHUDTeamScoreText[1] = nullptr;
    for (_int i = 0; i < MAX_PLAYER; ++i)
        m_pHUDPlayerBox[i] = nullptr;

    // 상점 슬롯도 레이어 소유. 참조만 끊는다.
    for (_int i = 0; i < SHOP_SLOT_COUNT; ++i)
        m_pShopSlot[i] = nullptr;

    // 맵 선택 창도 레이어 소유. 참조만 끊는다.
    m_pMapSelect = nullptr;

    // 캐릭터 선택 UI 도 레이어 소유. 참조만 끊는다.
    m_pCSPreviewMe = nullptr;
    for (_int i = 0; i < 3; ++i) m_pCSFacePanel[i] = nullptr;

    // 선택 구간 타이머 텍스트도 레이어 소유. 참조만 끊는다.
    m_pTimerText = nullptr;

    m_vStats.clear();
    Safe_Release(m_pGameInstance);
    __super::Free();
}