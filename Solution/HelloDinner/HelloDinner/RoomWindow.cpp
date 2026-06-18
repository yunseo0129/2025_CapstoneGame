#include "stdafx.h"
#include "RoomWindow.h"

#include <gdiplus.h>
#include <cstdlib>
#include <cstdio>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")

namespace
{
    constexpr _uint IDC_BTN_PRIMARY = 2001; // 방장: 게임 시작 / 참가자: 준비
    constexpr _uint IDC_BTN_LEAVE = 2002; // 나가기
}

const _tchar* CRoomWindow::WND_CLASS_NAME = TEXT("HelloDinnerRoomWnd");

CRoomWindow::CRoomWindow()
{
}

HRESULT CRoomWindow::Initialize(HINSTANCE hInstance, _bool bIsHost)
{
    m_hInstance = hInstance;
    m_bIsHost = bIsHost;

    // 방 코드 스텁 (1000~9999). 네트워크 연동 시 서버가 내려준 값으로 교체.
    m_iRoomCode = 1000 + (rand() % 9000);

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

    case WM_ERASEBKGND:
        return 1;

    case WM_CLOSE:
        if (pSelf) pSelf->m_eResult = ROOM_LEAVE;
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
}

void CRoomWindow::OnCommand(_uint iCtrlID)
{
    switch (iCtrlID)
    {
    case IDC_BTN_LEAVE:
        m_eResult = ROOM_LEAVE;
        PostMessage(m_hWnd, WM_NULL, 0, 0);
        break;

    case IDC_BTN_PRIMARY:
        if (m_bIsHost)
        {
            // 방장: 게임 시작
            m_eResult = ROOM_START_GAME;
            PostMessage(m_hWnd, WM_NULL, 0, 0);
        }
        else
        {
            // 참가자: 준비 토글 (현재는 표시용)
            m_bReady = !m_bReady;
            SetWindowText(m_hBtnPrimary, m_bReady ? TEXT("준비 완료") : TEXT("준비"));
            InvalidateRect(m_hWnd, nullptr, FALSE);
        }
        break;

    default:
        break;
    }
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

        // ---- 플레이어 슬롯 리스트 ----
        const REAL listX = 28.f;
        const REAL listY = 100.f;
        const REAL rowW = (REAL)W - 56.f;
        const REAL rowH = 64.f;
        const REAL rowGap = 12.f;

        Gdiplus::Font slotName(&ff, 20, FontStyleBold, UnitPixel);
        Gdiplus::Font slotSub(&ff, 15, FontStyleRegular, UnitPixel);

        for (_uint i = 0; i < MAX_SLOT; ++i)
        {
            REAL ry = listY + i * (rowH + rowGap);

            bool bOccupied = (i == 0); // 지금은 본인만 입장 (네트워크 연동 시 채워짐)

            // 행 배경
            Color rowColor = bOccupied ? Color(255, 38, 44, 60) : Color(255, 28, 30, 40);
            SolidBrush rowBg(rowColor);
            g.FillRectangle(&rowBg, listX, ry, rowW, rowH);

            // 좌측 인덱스 원
            Color circleCol = bOccupied ? Color(255, 70, 130, 200) : Color(255, 60, 64, 78);
            SolidBrush cb(circleCol);
            REAL cd = 40.f;
            g.FillEllipse(&cb, listX + 14, ry + (rowH - cd) / 2, cd, cd);

            SolidBrush idxBr(Color(255, 235, 240, 248));
            Gdiplus::Font idxF(&ff, 18, FontStyleBold, UnitPixel);
            StringFormat sfC; sfC.SetAlignment(StringAlignmentCenter); sfC.SetLineAlignment(StringAlignmentCenter);
            _tchar szIdx[8]; swprintf_s(szIdx, L"%u", i + 1);
            g.DrawString(szIdx, -1, &idxF, RectF(listX + 14, ry + (rowH - cd) / 2, cd, cd), &sfC, &idxBr);

            if (bOccupied)
            {
                // 이름
                SolidBrush nameBr(Color(255, 240, 244, 250));
                const _tchar* szName = m_bIsHost ? L"나 (방장)" : L"나";
                g.DrawString(szName, -1, &slotName, PointF(listX + 70, ry + 12), &nameBr);

                // 상태 뱃지 (우측)
                bool bReadyState = m_bIsHost ? true : m_bReady;
                Color badgeBg = bReadyState ? Color(255, 46, 160, 90) : Color(255, 120, 100, 50);
                const _tchar* szState = bReadyState ? L"준비 완료" : L"대기 중";

                REAL badgeW = 110.f, badgeH = 32.f;
                REAL bx = listX + rowW - badgeW - 16;
                REAL by = ry + (rowH - badgeH) / 2;
                SolidBrush bbg(badgeBg);
                g.FillRectangle(&bbg, bx, by, badgeW, badgeH);
                SolidBrush btx(Color(255, 245, 248, 252));
                StringFormat sfM; sfM.SetAlignment(StringAlignmentCenter); sfM.SetLineAlignment(StringAlignmentCenter);
                g.DrawString(szState, -1, &slotSub, RectF(bx, by, badgeW, badgeH), &sfM, &btx);
            }
            else
            {
                SolidBrush emptyBr(Color(255, 120, 126, 140));
                g.DrawString(L"빈 자리", -1, &slotName, PointF(listX + 70, ry + 16), &emptyBr);
            }
        }

        // ---- 안내 문구 ----
        {
            Gdiplus::Font noteF(&ff, 14, FontStyleRegular, UnitPixel);
            SolidBrush noteBr(Color(160, 150, 156, 170));
            const _tchar* szNote = m_bIsHost
                ? L"※ 네트워크 연동 전 단계입니다. [게임 시작]을 누르면 게임 화면으로 넘어갑니다."
                : L"※ 네트워크 연동 전 단계입니다. 방장이 시작하면 게임이 시작됩니다.";
            g.DrawString(szNote, -1, &noteF, PointF(28, (REAL)H - 96), &noteBr);
        }

    }

    Graphics screen(hdc);
    screen.DrawImage(pBack, 0, 0, W, H);

    EndPaint(hWnd, &ps);
}

CRoomWindow* CRoomWindow::Create(HINSTANCE hInstance, _bool bIsHost)
{
    CRoomWindow* pInstance = new CRoomWindow();

    if (FAILED(pInstance->Initialize(hInstance, bIsHost)))
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