#pragma once

#include "Base.h"
#include <vector>

// =====================================================================
//  CRoomWindow
//  - 로비에서 [방 만들기]/[방 들어가기]를 누르면 들어오는 대기방 (Win32 + GDI+)
//  - 지금은 화면/흐름만 구성 (네트워크 연동은 이후 단계)
//  - 방장(host): [게임 시작] / 참가자(guest): [준비]  + 공통 [나가기]
// =====================================================================

enum ROOM_RESULT
{
    ROOM_NONE,        // 진행 중
    ROOM_START_GAME,  // [게임 시작] (방장) - 게임 화면으로
    ROOM_LEAVE,       // [나가기] - 로비로 복귀
};

class CRoomWindow final: public CBase
{
public:
    static const _uint WIDTH = 760;
    static const _uint HEIGHT = 620;
    static const _uint MAX_SLOT = 4;   // 최대 인원 (네트워크 연동 시 조정)

private:
    CRoomWindow();
    virtual ~CRoomWindow() = default;

public:
    HRESULT     Initialize(HINSTANCE hInstance, _bool bIsHost, int serverRoomCode = 0);
    ROOM_RESULT DoModal();
    void        Close();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnCreate(HWND hWnd);
    void OnPaint(HWND hWnd);
    void OnCommand(_uint iCtrlID);
    void OnPollTimer();

    // 좌클릭 시 블럭 격자 히트테스트 → 팀/번호 선택
    void OnLButtonDown(int x, int y);
    // 현재 선택을 전역 캐리어(g_MatchSetup)에 기록 (게임 시작 직전 호출)
    void Commit_Selection();

private:
    HWND        m_hWnd = nullptr;
    HINSTANCE   m_hInstance = nullptr;
    ROOM_RESULT m_eResult = ROOM_NONE;

    _bool       m_bIsHost = true;   // true: 방 만들기(방장), false: 방 들어가기(참가자)
    _bool       m_bReady = false;  // 참가자 준비 상태 (현재는 표시용)
    _int        m_iRoomCode = 0;    // 방 코드 (스텁)

    // ---- 내가 고른 팀/번호 ----
    //  iTeam   : 0=RED, 1=BLUE, -1=미선택
    //  iNumber : 1~3 (팀 내 번호), 0=미선택
    _int        m_iSelTeam = -1;
    _int        m_iSelNumber = 0;

    // ---- 팀/번호 블럭 격자 히트박스 (RECT, 클라이언트 좌표) ----
    //  [team][number-1] 로 인덱싱. OnPaint 에서 채우고 OnLButtonDown 에서 사용.
    RECT        m_rcBlock[2][3] = {};

    void* m_pBackBuffer = nullptr; // Gdiplus::Bitmap* (더블버퍼)

    // ── 서버 방 상태 캐시 (WM_TIMER 폴링으로 갱신) ──
    struct RoomMemberCache { int id; char name[20]; unsigned char team; unsigned char slot; bool ready = false; };
    std::vector<RoomMemberCache> m_cachedMembers;
    int  m_cachedHostId = -1;

    HWND        m_hBtnPrimary = nullptr; // 방장: 게임시작 / 참가자: 준비
    HWND        m_hBtnLeave = nullptr; // 나가기
    HFONT       m_hFontBtn = nullptr;

    static const _tchar* WND_CLASS_NAME;

public:
    static CRoomWindow* Create(HINSTANCE hInstance, _bool bIsHost, int serverRoomCode = 0);
    virtual void Free() override;
};