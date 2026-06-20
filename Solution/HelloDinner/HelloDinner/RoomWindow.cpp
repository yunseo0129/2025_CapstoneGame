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
            // 방장: 서버에 게임 시작 요청 — SC_REDIRECT를 받으면 OnPollTimer가 창을 닫음
            Commit_Selection();
            NetworkClient::GetInstance()->Send_StartGame();
        }
        else
        {
            // 참가자: 준비 토글 (표시용)
            m_bReady = !m_bReady;
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
            if (snap.members[i].id != m_cachedMembers[i].id) { changed = true; break; }
        }
    }
    if (changed) {
        m_cachedHostId = snap.host_id;
        m_cachedMembers.clear();
        for (auto& m : snap.members) {
            RoomMemberCache c;
            c.id = m.id;
            strcpy_s(c.name, m.name);
            m_cachedMembers.push_back(c);
        }
        // 방 코드 업데이트 (참가자의 경우 JOIN 후 수신)
        if (snap.code != 0)
            m_iRoomCode = snap.code;
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void CRoomWindow::OnLButtonDown(int x, int y)
{
    // 팀/번호 블럭 격자 히트테스트. 6개 블럭 중 하나를 누르면
    //  내 팀(team)과 번호(number)가 동시에 정해진다.
    POINT pt = {(LONG)x, (LONG)y};
    for (int t = 0; t < 2; ++t)
    {
        for (int n = 0; n < 3; ++n)
        {
            if (PtInRect(&m_rcBlock[t][n], pt))
            {
                m_iSelTeam = t;
                m_iSelNumber = n + 1;
                InvalidateRect(m_hWnd, nullptr, FALSE);
                return;
            }
        }
    }
}

void CRoomWindow::Commit_Selection()
{
    // 게임플레이(CGame_Manager)가 읽어갈 전역 캐리어에 기록.
    g_MatchSetup.iTeam = m_iSelTeam;
    g_MatchSetup.iNumber = m_iSelNumber;
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

        // 내 슬롯(선택된 팀/번호)인지 판정용
        Gdiplus::Font blkF(&ff, 18, FontStyleBold, UnitPixel);
        Gdiplus::Font blkSubF(&ff, 14, FontStyleRegular, UnitPixel);

        const REAL blocksTop = gridTop + hdrH + 8.f;
        for (int t = 0; t < 2; ++t)
        {
            for (int n = 0; n < 3; ++n)
            {
                REAL bx = colX[t];
                REAL by = blocksTop + n * (blockH + blockGap);

                // 히트박스 저장 (클라이언트 좌표, 정수)
                m_rcBlock[t][n].left = (LONG)bx;
                m_rcBlock[t][n].top = (LONG)by;
                m_rcBlock[t][n].right = (LONG)(bx + colW);
                m_rcBlock[t][n].bottom = (LONG)(by + blockH);

                const bool bMine = (t == m_iSelTeam) && (n + 1 == m_iSelNumber);

                // 블럭 배경: 선택되면 팀색으로 진하게, 아니면 어둡게.
                Color fill = bMine
                    ? ((t == 0) ? Color(255, 120, 40, 44) : Color(255, 40, 64, 120))
                    : Color(255, 30, 33, 44);
                SolidBrush bbg(fill);
                g.FillRectangle(&bbg, bx, by, colW, blockH);

                // 테두리: 선택되면 밝은 팀색.
                Color edge = bMine
                    ? ((t == 0) ? Color(255, 240, 120, 120) : Color(255, 130, 180, 255))
                    : Color(255, 60, 64, 80);
                Pen ep(edge, bMine ? 3.f : 1.5f);
                g.DrawRectangle(&ep, bx, by, colW, blockH);

                // 왼쪽: 번호 라벨
                SolidBrush numBr(Color(255, 235, 240, 248));
                _tchar szNo[8]; swprintf_s(szNo, L"%d", n + 1);
                StringFormat sfL; sfL.SetAlignment(StringAlignmentNear); sfL.SetLineAlignment(StringAlignmentCenter);
                g.DrawString(szNo, -1, &blkF, RectF(bx + 16.f, by, 30.f, blockH), &sfL, &numBr);

                // 가운데: 점유 상태("ME" / "빈 자리"). 네트워크 연동 시 다른 이름 표시.
                const _tchar* szWho = bMine ? L"ME" : L"빈 자리";
                Color whoCol = bMine ? Color(255, 255, 255, 255) : Color(255, 120, 126, 140);
                SolidBrush whoBr(whoCol);
                StringFormat sfW; sfW.SetAlignment(StringAlignmentCenter); sfW.SetLineAlignment(StringAlignmentCenter);
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
                    bool isHost = (m_cachedMembers[i].id == m_cachedHostId);
                    Color col = isHost ? Color(255, 255, 220, 80) : Color(255, 200, 210, 230);
                    SolidBrush mb(col);
                    // 이름을 wchar로 변환
                    WCHAR wname[NAME_SIZE] = {};
                    MultiByteToWideChar(CP_ACP, 0, m_cachedMembers[i].name, -1, wname, NAME_SIZE);
                    StringFormat sfC; sfC.SetAlignment(StringAlignmentCenter); sfC.SetLineAlignment(StringAlignmentCenter);
                    _tchar disp[NAME_SIZE + 8] = {};
                    swprintf_s(disp, isHost ? L"[방장]\n%s" : L"%s", wname);
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