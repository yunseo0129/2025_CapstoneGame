#include "stdafx.h"
#include "RoomWindow.h"
#include "NetworkClient.h"

#include <gdiplus.h>
#include <windowsx.h>
#include <cstdlib>
#include <cstdio>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")

namespace
{
    constexpr _uint IDC_BTN_PRIMARY = 2001; // 방장: 게임 시작 / 참가자: 준비
    constexpr _uint IDC_BTN_LEAVE   = 2002; // 나가기
    constexpr UINT_PTR TIMER_POLL   = 1;    // 서버 방 상태 폴링 (100ms)
}

const _tchar* CRoomWindow::WND_CLASS_NAME = TEXT("HelloDinnerRoomWnd");

CRoomWindow::CRoomWindow()
{
}

HRESULT CRoomWindow::Initialize(HINSTANCE hInstance, _bool bIsHost, int serverRoomCode)
{
    m_hInstance = hInstance;
    m_bIsHost   = bIsHost;

    // 서버가 발급한 코드 사용 (0이면 스냅샷에서 읽기)
    if (serverRoomCode != 0) {
        m_iRoomCode = serverRoomCode;
    } else {
        auto snap   = NetworkClient::GetInstance()->GetRoomSnapshot();
        m_iRoomCode = (snap.code != 0) ? snap.code : 0;
    }

    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszClassName = WND_CLASS_NAME;
    RegisterClassEx(&wcex);

    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;

    RECT rc = {0, 0, (LONG)WIDTH, (LONG)HEIGHT};
    AdjustWindowRect(&rc, dwStyle, FALSE);
    int iW = rc.right - rc.left;
    int iH = rc.bottom - rc.top;
    int iX = (GetSystemMetrics(SM_CXSCREEN) - iW) / 2;
    int iY = (GetSystemMetrics(SM_CYSCREEN) - iH) / 2;

    const _tchar* szTitle = m_bIsHost ? TEXT("방 만들기 - 대기방") : TEXT("방 들어가기 - 대기방");

    m_hWnd = CreateWindowEx(
        0, WND_CLASS_NAME, szTitle,
        dwStyle, iX, iY, iW, iH,
        nullptr, nullptr, hInstance, this);

    if (nullptr == m_hWnd)
        return E_FAIL;

    return S_OK;
}

ROOM_RESULT CRoomWindow::DoModal()
{
    if (nullptr == m_hWnd)
        return ROOM_LEAVE;

    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);
    SetForegroundWindow(m_hWnd);

    MSG msg;
    while (ROOM_NONE == m_eResult)
    {
        if (GetMessage(&msg, nullptr, 0, 0) <= 0)
        {
            m_eResult = ROOM_LEAVE; // 창 닫힘 = 로비로
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ShowWindow(m_hWnd, SW_HIDE);
    return m_eResult;
}

void CRoomWindow::Close()
{
    if (m_hWnd) KillTimer(m_hWnd, TIMER_POLL);
    if (m_pBackBuffer) { delete static_cast<Gdiplus::Bitmap*>(m_pBackBuffer); m_pBackBuffer = nullptr; }
    if (m_hWnd) { DestroyWindow(m_hWnd); m_hWnd = nullptr; }
    if (m_hFontBtn) { DeleteObject(m_hFontBtn); m_hFontBtn = nullptr; }
}

LRESULT CALLBACK CRoomWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (WM_NCCREATE == msg)
    {
        CREATESTRUCT* pCS = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pCS->lpCreateParams);
    }

    CRoomWindow* pSelf = reinterpret_cast<CRoomWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_CREATE:
        if (pSelf) pSelf->OnCreate(hWnd);
        return 0;

    case WM_COMMAND:
        if (pSelf) pSelf->OnCommand(LOWORD(wParam));
        return 0;

    case WM_PAINT:
        if (pSelf) pSelf->OnPaint(hWnd);
        return 0;

    case WM_LBUTTONDOWN:
        if (pSelf) pSelf->OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER:
        if (pSelf && TIMER_POLL == wParam)
            pSelf->OnPollTimer();
        return 0;

    case WM_CLOSE:
        if (pSelf) {
            NetworkClient::GetInstance()->Send_LeaveRoom();
            pSelf->m_eResult = ROOM_LEAVE;
        }
        return 0;

    case WM_DESTROY:
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void CRoomWindow::OnCreate(HWND hWnd)
{
    m_hWnd = hWnd;

    m_hFontBtn = CreateFont(
        24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, TEXT("맑은 고딕"));

    const int btnW = 180;
    const int btnH = 54;
    const int gap = 16;
    const int marginB = 36;
    const int yBtn = (int)HEIGHT - marginB - btnH;

    // 우측: 주 버튼(게임시작/준비), 좌측: 나가기
    const int xPrimary = (int)WIDTH - 40 - btnW;
    const int xLeave = xPrimary - gap - btnW;

    const _tchar* szPrimary = m_bIsHost ? TEXT("게임 시작") : TEXT("준비");

    m_hBtnPrimary = CreateWindowEx(
        0, TEXT("BUTTON"), szPrimary,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        xPrimary, yBtn, btnW, btnH,
        hWnd, (HMENU)(UINT_PTR)IDC_BTN_PRIMARY, m_hInstance, nullptr);

    m_hBtnLeave = CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("나가기"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        xLeave, yBtn, btnW, btnH,
        hWnd, (HMENU)(UINT_PTR)IDC_BTN_LEAVE, m_hInstance, nullptr);

    if (m_hFontBtn)
    {
        if (m_hBtnPrimary) SendMessage(m_hBtnPrimary, WM_SETFONT, (WPARAM)m_hFontBtn, TRUE);
        if (m_hBtnLeave)   SendMessage(m_hBtnLeave, WM_SETFONT, (WPARAM)m_hFontBtn, TRUE);
    }

    // 서버 방 상태 폴링 타이머 시작 (100ms)
    SetTimer(hWnd, TIMER_POLL, 100, nullptr);
}

void CRoomWindow::OnCommand(_uint iCtrlID)
{
    switch (iCtrlID)
    {
    case IDC_BTN_LEAVE:
        NetworkClient::GetInstance()->Send_LeaveRoom();
        m_eResult = ROOM_LEAVE;
        PostMessage(m_hWnd, WM_NULL, 0, 0);
        break;

    case IDC_BTN_PRIMARY:
        if (m_bIsHost)
        {
            // 비방장 멤버 전원이 준비 완료인지 확인
            int myId = NetworkClient::GetInstance()->GetMyId();
            bool allReady = !m_cachedMembers.empty();
            for (auto& m : m_cachedMembers) {
                if (m.id == myId) continue;  // 방장 자신은 제외
                if (!m.ready) { allReady = false; break; }
            }
            if (!allReady) {
                MessageBox(m_hWnd, TEXT("모든 플레이어가 준비 완료 상태여야 시작할 수 있습니다."),
                           TEXT("게임 시작 불가"), MB_OK | MB_ICONWARNING);
                break;
            }
            Commit_Selection();
            NetworkClient::GetInstance()->Send_StartGame();
        }
        else
        {
            // 참가자: 준비 토글 → 서버에 패킷 전송
            m_bReady = !m_bReady;
            NetworkClient::GetInstance()->Send_PlayerReady(m_bReady);
            SetWindowText(m_hBtnPrimary, m_bReady ? TEXT("준비 완료") : TEXT("준비"));
            InvalidateRect(m_hWnd, nullptr, FALSE);
        }
        break;

    default:
        break;
    }
}

void CRoomWindow::OnPollTimer()
{
    // 게임 시작 신호 감지 (SC_REDIRECT 수신 후 NetworkClient가 세움)
    if (NetworkClient::GetInstance()->IsGameStarting()) {
        KillTimer(m_hWnd, TIMER_POLL);
        Commit_Selection();  // 게스트도 g_MatchSetup 반드시 기록
        m_eResult = ROOM_START_GAME;
        PostMessage(m_hWnd, WM_NULL, 0, 0);
        return;
    }

    // 멤버 목록 갱신
    auto snap = NetworkClient::GetInstance()->GetRoomSnapshot();
    bool changed = (snap.members.size() != m_cachedMembers.size())
                || (snap.host_id != m_cachedHostId);
    if (!changed) {
        for (int i = 0; i < (int)snap.members.size(); ++i) {
            if (snap.members[i].id   != m_cachedMembers[i].id   ||
                snap.members[i].team != m_cachedMembers[i].team ||
                snap.members[i].slot != m_cachedMembers[i].slot) {
                changed = true; break;
            }
        }
    }
    // ready 변화도 감지
    if (!changed) {
        for (int i = 0; i < (int)snap.members.size(); ++i) {
            if (snap.members[i].ready != m_cachedMembers[i].ready) { changed = true; break; }
        }
    }
    if (changed) {
        m_cachedHostId = snap.host_id;
        m_cachedMembers.clear();
        for (auto& m : snap.members) {
            RoomMemberCache c;
            c.id    = m.id;
            strcpy_s(c.name, m.name);
            c.team  = m.team;
            c.slot  = m.slot;
            c.ready = m.ready;
            m_cachedMembers.push_back(c);
        }
        // 서버 확인된 내 선택 및 준비 상태 동기화
        int myId = NetworkClient::GetInstance()->GetMyId();
        m_iSelTeam   = -1;
        m_iSelNumber = 0;
        for (auto& m : m_cachedMembers) {
            if (m.id == myId) {
                m_iSelTeam   = (m.team == 0xFF) ? -1 : (int)m.team;
                m_iSelNumber = (int)m.slot;
                if (!m_bIsHost) {
                    m_bReady = m.ready;
                    if (m_hBtnPrimary)
                        SetWindowText(m_hBtnPrimary, m_bReady ? TEXT("준비 완료") : TEXT("준비"));
                }
                break;
            }
        }
        // 방 코드 업데이트 (참가자의 경우 JOIN 후 수신)
        if (snap.code != 0)
            m_iRoomCode = snap.code;
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void CRoomWindow::OnLButtonDown(int x, int y)
{
    POINT pt = {(LONG)x, (LONG)y};
    int myId = NetworkClient::GetInstance()->GetMyId();

    for (int t = 0; t < 2; ++t)
    {
        for (int n = 0; n < 3; ++n)
        {
            if (!PtInRect(&m_rcBlock[t][n], pt)) continue;

            // 다른 플레이어가 이미 점유한 칸은 선택 불가
            for (auto& m : m_cachedMembers) {
                if (m.id != myId && (int)m.team == t && (int)m.slot == n + 1)
                    return;  // 막힘
            }

            // 내가 이미 선택한 칸을 다시 클릭 → 선택 해제
            if (m_iSelTeam == t && m_iSelNumber == n + 1) {
                m_iSelTeam   = -1;
                m_iSelNumber = 0;
                NetworkClient::GetInstance()->Send_SelectSeat(0xFF, 0);
                InvalidateRect(m_hWnd, nullptr, FALSE);
                return;
            }

            // 새 슬롯 선택
            m_iSelTeam   = t;
            m_iSelNumber = n + 1;
            NetworkClient::GetInstance()->Send_SelectSeat(
                (unsigned char)t, (unsigned char)(n + 1));
            InvalidateRect(m_hWnd, nullptr, FALSE);
            return;
        }
    }
}

void CRoomWindow::Commit_Selection()
{
    // 미선택 시 RED팀 1번으로 기본 처리
    g_MatchSetup.iTeam   = (m_iSelTeam   >= 0) ? m_iSelTeam   : 0;
    g_MatchSetup.iNumber = (m_iSelNumber >= 1) ? m_iSelNumber : 1;
}

void CRoomWindow::OnPaint(HWND hWnd)
{
    using namespace Gdiplus;

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    if (W <= 0 || H <= 0) { EndPaint(hWnd, &ps); return; }

    Bitmap* pBack = static_cast<Bitmap*>(m_pBackBuffer);
    if (!pBack || (int)pBack->GetWidth() != W || (int)pBack->GetHeight() != H)
    {
        if (pBack) delete pBack;
        pBack = new Bitmap(W, H, PixelFormat32bppPARGB);
        m_pBackBuffer = pBack;
    }

    {
        Graphics g(pBack);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);

        FontFamily ff(L"맑은 고딕");

        // ---- 배경 ----
        SolidBrush bgBrush(Color(255, 22, 24, 32));
        g.FillRectangle(&bgBrush, 0, 0, W, H);

        // ---- 헤더 바 ----
        LinearGradientBrush headBrush(
            Point(0, 0), Point(0, 72),
            Color(255, 46, 80, 130), Color(255, 32, 56, 96));
        g.FillRectangle(&headBrush, 0, 0, W, 72);

        {
            Gdiplus::Font hTitle(&ff, 26, FontStyleBold, UnitPixel);
            SolidBrush wbr(Color(255, 240, 244, 250));
            const _tchar* szT = m_bIsHost ? L"방 만들기" : L"방 들어가기";
            g.DrawString(szT, -1, &hTitle, PointF(28, 20), &wbr);

            Gdiplus::Font hCode(&ff, 18, FontStyleRegular, UnitPixel);
            SolidBrush cbr(Color(220, 200, 220, 245));
            _tchar szCode[64];
            swprintf_s(szCode, L"방 코드 : %04d", m_iRoomCode);
            StringFormat sfR; sfR.SetAlignment(StringAlignmentFar);
            g.DrawString(szCode, -1, &hCode, RectF(0, 24, (REAL)W - 28, 30), &sfR, &cbr);
        }

        // ===== 팀/번호 블럭 격자 =====
        //  레이아웃:
        //      RED        BLUE
        //    [1번블럭]  [1번블럭]
        //    [2번블럭]  [2번블럭]
        //    [3번블럭]  [3번블럭]
        //  6개 블럭 중 하나를 클릭하면 내 팀+번호가 동시에 정해진다(선택=하이라이트).
        const REAL gridTop = 96.f;
        const REAL colGap = 28.f;
        const REAL colW = ((REAL)W - 56.f - colGap) * 0.5f; // 좌우 28px 여백
        const REAL colXR = 28.f;                              // RED 열 시작 X
        const REAL colXB = 28.f + colW + colGap;              // BLUE 열 시작 X
        const REAL colX[2] = {colXR, colXB};

        const REAL hdrH = 36.f;       // 팀 헤더 높이
        const REAL blockH = 56.f;     // 블럭 높이
        const REAL blockGap = 12.f;   // 블럭 세로 간격

        // 팀 헤더 (RED / BLUE)
        Gdiplus::Font hdrF(&ff, 22, FontStyleBold, UnitPixel);
        StringFormat sfCC; sfCC.SetAlignment(StringAlignmentCenter); sfCC.SetLineAlignment(StringAlignmentCenter);

        const Color teamHdr[2] = {Color(255, 220, 80, 80), Color(255, 90, 140, 230)};
        const _tchar* teamName[2] = {L"RED", L"BLUE"};
        for (int t = 0; t < 2; ++t)
        {
            SolidBrush hb(teamHdr[t]);
            g.DrawString(teamName[t], -1, &hdrF, RectF(colX[t], gridTop, colW, hdrH), &sfCC, &hb);
        }

        Gdiplus::Font blkF(&ff, 18, FontStyleBold, UnitPixel);
        Gdiplus::Font blkSubF(&ff, 14, FontStyleRegular, UnitPixel);

        const REAL blocksTop = gridTop + hdrH + 8.f;
        int myId = NetworkClient::GetInstance()->GetMyId();

        for (int t = 0; t < 2; ++t)
        {
            for (int n = 0; n < 3; ++n)
            {
                REAL bx = colX[t];
                REAL by = blocksTop + n * (blockH + blockGap);

                m_rcBlock[t][n].left   = (LONG)bx;
                m_rcBlock[t][n].top    = (LONG)by;
                m_rcBlock[t][n].right  = (LONG)(bx + colW);
                m_rcBlock[t][n].bottom = (LONG)(by + blockH);

                // 서버 확인 점유자 탐색
                const RoomMemberCache* pOcc = nullptr;
                for (auto& m : m_cachedMembers) {
                    if ((int)m.team == t && (int)m.slot == n + 1) { pOcc = &m; break; }
                }

                bool bMine  = (pOcc && pOcc->id == myId);
                bool bOther = (pOcc && !bMine);
                // 서버 확인 전 낙관적 표시: 로컬 선택이 이 칸이고 아직 서버 미반영
                if (!pOcc && m_iSelTeam == t && m_iSelNumber == n + 1)
                    bMine = true;

                // ── 배경 ──
                Color fill;
                if      (bMine)  fill = (t == 0) ? Color(255, 120, 40, 44)  : Color(255, 40, 64, 120);
                else if (bOther) fill = (t == 0) ? Color(255, 72, 26, 28)   : Color(255, 26, 40, 80);
                else             fill = Color(255, 30, 33, 44);
                SolidBrush bbg(fill);
                g.FillRectangle(&bbg, bx, by, colW, blockH);

                // ── 테두리 ──
                Color edge;
                float edgeW;
                if      (bMine)  { edge = (t == 0) ? Color(255, 240, 120, 120) : Color(255, 130, 180, 255); edgeW = 3.f; }
                else if (bOther) { edge = (t == 0) ? Color(255, 180, 80,  80)  : Color(255, 80, 130, 200);  edgeW = 2.f; }
                else             { edge = Color(255, 60, 64, 80); edgeW = 1.5f; }
                Pen ep(edge, edgeW);
                g.DrawRectangle(&ep, bx, by, colW, blockH);

                // ── 번호 라벨 (좌측) ──
                SolidBrush numBr(Color(255, 235, 240, 248));
                _tchar szNo[8]; swprintf_s(szNo, L"%d", n + 1);
                StringFormat sfL;
                sfL.SetAlignment(StringAlignmentNear);
                sfL.SetLineAlignment(StringAlignmentCenter);
                g.DrawString(szNo, -1, &blkF, RectF(bx + 16.f, by, 30.f, blockH), &sfL, &numBr);

                // ── 닉네임 / 상태 (가운데) ──
                _tchar szWho[NAME_SIZE + 4] = {};
                Color whoCol;
                if (bMine) {
                    swprintf_s(szWho, L"ME");
                    whoCol = Color(255, 255, 255, 255);
                } else if (bOther) {
                    WCHAR wname[NAME_SIZE] = {};
                    MultiByteToWideChar(CP_ACP, 0, pOcc->name, -1, wname, NAME_SIZE);
                    swprintf_s(szWho, L"%s", wname);
                    whoCol = (t == 0) ? Color(255, 255, 180, 180) : Color(255, 160, 200, 255);
                } else {
                    swprintf_s(szWho, L"빈 자리");
                    whoCol = Color(255, 90, 96, 112);
                }
                SolidBrush whoBr(whoCol);
                StringFormat sfW;
                sfW.SetAlignment(StringAlignmentCenter);
                sfW.SetLineAlignment(StringAlignmentCenter);
                g.DrawString(szWho, -1, &blkF, RectF(bx + 46.f, by, colW - 46.f, blockH), &sfW, &whoBr);
            }
        }

        // ===== 멤버 목록 =====
        const REAL waitTop = blocksTop + 3 * (blockH + blockGap) + 14.f;
        const REAL waitX = 28.f;
        const REAL waitW = (REAL)W - 56.f;
        const REAL waitH = 70.f;
        {
            Gdiplus::Font wF(&ff, 15, FontStyleBold, UnitPixel);
            SolidBrush wlbl(Color(220, 210, 216, 230));
            g.DrawString(L"참가자 목록", -1, &wF, PointF(waitX, waitTop - 24.f), &wlbl);

            SolidBrush wbg(Color(255, 26, 28, 38));
            g.FillRectangle(&wbg, waitX, waitTop, waitW, waitH);
            Pen wpen(Color(255, 56, 60, 74), 1.5f);
            g.DrawRectangle(&wpen, waitX, waitTop, waitW, waitH);

            if (m_cachedMembers.empty()) {
                SolidBrush wtx(Color(255, 110, 116, 130));
                StringFormat sfWC; sfWC.SetAlignment(StringAlignmentCenter); sfWC.SetLineAlignment(StringAlignmentCenter);
                g.DrawString(L"서버에 연결 중...", -1, &blkSubF,
                    RectF(waitX, waitTop, waitW, waitH), &sfWC, &wtx);
            } else {
                Gdiplus::Font mF(&ff, 14, FontStyleRegular, UnitPixel);
                const REAL itemW = waitW / ROOM_MAX_PLAYER;
                for (int i = 0; i < (int)m_cachedMembers.size(); ++i) {
                    bool isHost  = (m_cachedMembers[i].id == m_cachedHostId);
                    bool isReady = m_cachedMembers[i].ready || isHost; // 방장은 항상 준비
                    Color col = isHost
                        ? Color(255, 255, 220,  80)   // 방장: 노란색
                        : (isReady
                            ? Color(255, 100, 220, 120) // 준비완료: 초록색
                            : Color(255, 160, 168, 180)); // 미준비: 회색
                    SolidBrush mb(col);
                    WCHAR wname[NAME_SIZE] = {};
                    MultiByteToWideChar(CP_ACP, 0, m_cachedMembers[i].name, -1, wname, NAME_SIZE);
                    StringFormat sfC; sfC.SetAlignment(StringAlignmentCenter); sfC.SetLineAlignment(StringAlignmentCenter);
                    _tchar disp[NAME_SIZE + 16] = {};
                    if (isHost)
                        swprintf_s(disp, L"[방장]\n%s", wname);
                    else if (isReady)
                        swprintf_s(disp, L"[준비완료]\n%s", wname);
                    else
                        swprintf_s(disp, L"[대기중]\n%s", wname);
                    g.DrawString(disp, -1, &mF,
                        RectF(waitX + i * itemW, waitTop, itemW, waitH), &sfC, &mb);
                }
            }
        }

        // ---- 하단 안내 문구 (버튼 위) ----
        {
            Gdiplus::Font noteF(&ff, 14, FontStyleRegular, UnitPixel);
            SolidBrush noteBr(Color(180, 168, 174, 188));
            const _tchar* szNote = m_bIsHost
                ? L"※ 블럭을 눌러 팀/번호를 고른 뒤 [게임 시작]을 누르세요."
                : L"※ 블럭을 눌러 자리를 고르세요. 방장이 시작하면 게임이 시작됩니다.";
            g.DrawString(szNote, -1, &noteF, PointF(28, (REAL)H - 76.f - 18.f), &noteBr);
        }

    }

    Graphics screen(hdc);
    screen.DrawImage(pBack, 0, 0, W, H);

    EndPaint(hWnd, &ps);
}

CRoomWindow* CRoomWindow::Create(HINSTANCE hInstance, _bool bIsHost, int serverRoomCode)
{
    CRoomWindow* pInstance = new CRoomWindow();

    if (FAILED(pInstance->Initialize(hInstance, bIsHost, serverRoomCode)))
    {
        MSG_BOX("Failed to Created : CRoomWindow");
        Safe_Release(pInstance);
        return nullptr;
    }

    return pInstance;
}

void CRoomWindow::Free()
{
    Close();
    __super::Free();
}