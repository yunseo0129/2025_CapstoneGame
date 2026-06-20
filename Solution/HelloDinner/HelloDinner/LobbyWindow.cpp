#include "stdafx.h"
#include "LobbyWindow.h"
#include "NetworkClient.h"

#include <gdiplus.h>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")

// =====================================================================
//  여기에 로비 메인 이미지 경로를 넣으세요. (GIF면 자동 애니메이션)
//  - 비워두면( L"" ) placeholder 화면이 뜹니다.
//  - 예) L"Resources/UI/Lobby.gif"  또는  L"Resources/Textures/Lobby.png"
//    (작업 디렉터리 기준 상대경로. 기존 리소스들과 같은 위치에 두면 됩니다.)
// =====================================================================
static const _tchar* LOBBY_IMAGE_PATH = L"";

// 버튼 컨트롤 ID
namespace
{
    constexpr _uint IDC_BTN_START = 1001; // 게임 시작
    constexpr _uint IDC_BTN_CREATE = 1002; // 방 만들기
    constexpr _uint IDC_BTN_JOIN = 1003; // 방 들어가기

    constexpr UINT_PTR TIMER_ANIM = 1;
}

const _tchar* CLobbyWindow::WND_CLASS_NAME = TEXT("HelloDinnerLobbyWnd");

CLobbyWindow::CLobbyWindow()
{
}

HRESULT CLobbyWindow::Initialize(HINSTANCE hInstance)
{
    m_hInstance = hInstance;

    // 윈도우 클래스 등록 (이미 등록되어 있으면 무시됨)
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr; // 배경은 직접 그림 (WM_ERASEBKGND 처리)
    wcex.lpszClassName = WND_CLASS_NAME;
    RegisterClassEx(&wcex);

    // 고정 크기(리사이즈 불가) 스타일 + 자식 클리핑(버튼 깜빡임 방지)
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;

    RECT rc = {0, 0, (LONG)WIDTH, (LONG)HEIGHT};
    AdjustWindowRect(&rc, dwStyle, FALSE);
    int iW = rc.right - rc.left;
    int iH = rc.bottom - rc.top;

    // 화면 중앙
    int iX = (GetSystemMetrics(SM_CXSCREEN) - iW) / 2;
    int iY = (GetSystemMetrics(SM_CYSCREEN) - iH) / 2;

    m_hWnd = CreateWindowEx(
        0, WND_CLASS_NAME, TEXT("HelloDinner"),
        dwStyle,
        iX, iY, iW, iH,
        nullptr, nullptr, hInstance,
        this);              // lpParam 으로 this 전달 → WM_NCCREATE 에서 회수

    if (nullptr == m_hWnd)
        return E_FAIL;

    return S_OK;
}

LOBBY_RESULT CLobbyWindow::DoModal()
{
    if (nullptr == m_hWnd)
        return LOBBY_EXIT;

    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);
    SetForegroundWindow(m_hWnd);

    // 자체 모달 메시지 루프
    MSG msg;
    while (LOBBY_NONE == m_eResult)
    {
        if (GetMessage(&msg, nullptr, 0, 0) <= 0) // WM_QUIT 또는 에러
        {
            m_eResult = LOBBY_EXIT;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ShowWindow(m_hWnd, SW_HIDE);
    return m_eResult;
}

void CLobbyWindow::Close()
{
    if (m_hWnd)
        KillTimer(m_hWnd, TIMER_ANIM);

    // GDI+ 자원 정리
    if (m_pImage) { delete static_cast<Gdiplus::Image*>(m_pImage);   m_pImage = nullptr; }
    if (m_pBackBuffer) { delete static_cast<Gdiplus::Bitmap*>(m_pBackBuffer); m_pBackBuffer = nullptr; }
    if (m_pFrameDelays) { delete[] m_pFrameDelays; m_pFrameDelays = nullptr; }

    if (m_hWnd) { DestroyWindow(m_hWnd); m_hWnd = nullptr; } // 자식 버튼도 함께 파괴됨
    if (m_hFontBtn) { DeleteObject(m_hFontBtn); m_hFontBtn = nullptr; }
}

LRESULT CALLBACK CLobbyWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (WM_NCCREATE == msg)
    {
        CREATESTRUCT* pCS = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pCS->lpCreateParams);
    }

    CLobbyWindow* pSelf = reinterpret_cast<CLobbyWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

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
        return 1; // 배경은 OnPaint 에서 전부 그리므로 기본 지우기 생략 (깜빡임 방지)

    case WM_TIMER:
        if (pSelf && TIMER_ANIM == wParam)
        {
            if (pSelf->m_pImage && pSelf->m_iFrameCount > 1)
            {
                // GIF 다음 프레임
                pSelf->m_iCurFrame = (pSelf->m_iCurFrame + 1) % pSelf->m_iFrameCount;
                InvalidateRect(hWnd, nullptr, FALSE);
                // 다음 프레임의 지연 시간으로 타이머 갱신
                SetTimer(hWnd, TIMER_ANIM, pSelf->m_pFrameDelays[pSelf->m_iCurFrame], nullptr);
            }
            else if (!pSelf->m_pImage)
            {
                // 이미지가 없을 때 placeholder 연출
                pSelf->m_fAnimTime += 0.033f;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        }
        return 0;

    case WM_CLOSE:
        if (pSelf) pSelf->m_eResult = LOBBY_EXIT; // 창 닫기 = 종료. 실제 파괴는 Close()에서.
        return 0;

    case WM_DESTROY:
        return 0; // 메인 윈도우가 아니므로 PostQuitMessage 안 함
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void CLobbyWindow::OnCreate(HWND hWnd)
{
    m_hWnd = hWnd;

    // 버튼 폰트
    m_hFontBtn = CreateFont(
        24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, TEXT("맑은 고딕"));

    // ---- 우하단 버튼 3개 (위에서부터 게임시작/방만들기/방들어가기) ----
    const int btnW = 220;
    const int btnH = 56;
    const int gap = 14;
    const int marginR = 40;
    const int marginB = 44;

    const int x = (int)WIDTH - marginR - btnW;
    const int yJoin = (int)HEIGHT - marginB - btnH;          // 맨 아래
    const int yCreate = yJoin - gap - btnH;
    const int yStart = yCreate - gap - btnH;                  // 맨 위

    struct { const _tchar* text; int y; _uint id; } btns[] = {
        {TEXT("게임 시작"), yStart, IDC_BTN_START},
        {TEXT("방 만들기"), yCreate, IDC_BTN_CREATE},
        {TEXT("방 들어가기"), yJoin, IDC_BTN_JOIN},
    };

    for (auto& b : btns)
    {
        HWND hBtn = CreateWindowEx(
            0, TEXT("BUTTON"), b.text,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, b.y, btnW, btnH,
            hWnd, (HMENU)(UINT_PTR)b.id, m_hInstance, nullptr);

        if (hBtn && m_hFontBtn)
            SendMessage(hBtn, WM_SETFONT, (WPARAM)m_hFontBtn, TRUE);
    }

    // ---- 배경 로드 & 애니메이션 타이머 ----
    LoadBackground(LOBBY_IMAGE_PATH);

    if (m_pImage && m_iFrameCount > 1 && m_pFrameDelays)
        SetTimer(hWnd, TIMER_ANIM, m_pFrameDelays[0], nullptr); // GIF 재생
    else if (!m_pImage)
        SetTimer(hWnd, TIMER_ANIM, 33, nullptr);                // placeholder 연출
    // 정지 이미지면 타이머 불필요 (한 번만 그림)
}

// 4자리 방 코드 입력을 받는 간단한 팝업 다이얼로그
// 성공 시 code에 값 저장 후 true 반환, 취소 시 false 반환
static bool PromptRoomCode(HWND hParent, int& out_code)
{
    // 다이얼로그 윈도우 클래스 등록
    static bool s_registered = false;
    static const TCHAR* CLASS_NAME = TEXT("RoomCodeDlg");
    if (!s_registered) {
        WNDCLASSEX wc = {};
        wc.cbSize        = sizeof(WNDCLASSEX);
        wc.lpfnWndProc   = DefWindowProc;
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = CLASS_NAME;
        RegisterClassEx(&wc);
        s_registered = true;
    }

    const int DLG_W = 320, DLG_H = 150;
    int x = (GetSystemMetrics(SM_CXSCREEN) - DLG_W) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - DLG_H) / 2;

    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME, CLASS_NAME, TEXT("방 코드 입력"),
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, DLG_W, DLG_H, hParent, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hDlg) return false;

    // 안내 문구
    CreateWindowEx(0, TEXT("STATIC"), TEXT("방 코드 4자리를 입력하세요:"),
        WS_CHILD | WS_VISIBLE,
        20, 20, 280, 24, hDlg, nullptr, GetModuleHandle(nullptr), nullptr);

    // 숫자 전용 EDIT 컨트롤
    HWND hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER,
        80, 52, 160, 28, hDlg, (HMENU)100, GetModuleHandle(nullptr), nullptr);
    SendMessage(hEdit, EM_SETLIMITTEXT, 4, 0);

    // 확인 버튼
    HWND hOK = CreateWindowEx(0, TEXT("BUTTON"), TEXT("확인"),
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        80, 96, 80, 30, hDlg, (HMENU)IDOK, GetModuleHandle(nullptr), nullptr);
    // 취소 버튼
    CreateWindowEx(0, TEXT("BUTTON"), TEXT("취소"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        170, 96, 80, 30, hDlg, (HMENU)IDCANCEL, GetModuleHandle(nullptr), nullptr);

    SetFocus(hEdit);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    bool confirmed = false;
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_COMMAND) {
            WORD id = LOWORD(msg.wParam);
            if (id == IDOK || (msg.hwnd == hEdit && HIWORD(msg.wParam) == EN_CHANGE)) {
                if (id == IDOK) {
                    TCHAR buf[8] = {};
                    GetWindowText(hEdit, buf, 8);
                    int code = _ttoi(buf);
                    if (code >= 1000 && code <= 9999) {
                        out_code  = code;
                        confirmed = true;
                        break;
                    } else {
                        MessageBox(hDlg, TEXT("1000~9999 사이의 4자리 숫자를 입력하세요."),
                                   TEXT("오류"), MB_OK | MB_ICONWARNING);
                    }
                }
            } else if (id == IDCANCEL) {
                break;
            }
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyWindow(hDlg);
    return confirmed;
}

void CLobbyWindow::OnCommand(_uint iCtrlID)
{
    switch (iCtrlID)
    {
    case IDC_BTN_START:
        m_eResult = LOBBY_START_GAME;
        break;
    case IDC_BTN_CREATE:
        m_eResult = LOBBY_CREATE_ROOM;
        break;
    case IDC_BTN_JOIN: {
        // 코드 입력 팝업 → 서버에 입장 요청 → 결과 대기
        int code = 0;
        if (!PromptRoomCode(m_hWnd, code))
            return;  // 취소 — 로비로 복귀

        NetworkClient::GetInstance()->Send_JoinRoomCode(code);

        // SC_ROOM_JOIN_RESULT 대기 (최대 3초)
        for (int i = 0; i < 300; ++i) {
            auto snap = NetworkClient::GetInstance()->GetRoomSnapshot();
            if (!snap.join_pending) {
                if (snap.join_result == RJR_OK) {
                    m_eResult = LOBBY_JOIN_ROOM;
                } else {
                    const TCHAR* msg = TEXT("알 수 없는 오류");
                    if (snap.join_result == RJR_NOT_FOUND) msg = TEXT("존재하지 않는 방입니다.");
                    else if (snap.join_result == RJR_FULL)    msg = TEXT("방이 가득 찼습니다.");
                    else if (snap.join_result == RJR_STARTED) msg = TEXT("이미 시작된 방입니다.");
                    MessageBox(m_hWnd, msg, TEXT("입장 실패"), MB_OK | MB_ICONINFORMATION);
                }
                break;
            }
            Sleep(10);
            MSG tmpMsg;
            while (PeekMessage(&tmpMsg, nullptr, 0, 0, PM_REMOVE))
                DispatchMessage(&tmpMsg);
        }
        break;
    }
    default: return;
    }

    PostMessage(m_hWnd, WM_NULL, 0, 0);
}

bool CLobbyWindow::LoadBackground(const _tchar* szPath)
{
    using namespace Gdiplus;

    if (!szPath || szPath[0] == L'\0')
        return false;

    Image* pImg = new Image(szPath);
    if (pImg->GetLastStatus() != Ok)
    {
        delete pImg;
        return false;
    }
    m_pImage = pImg;

    // 프레임(움짤) 개수 확인
    UINT iDimCount = pImg->GetFrameDimensionsCount();
    if (iDimCount > 0)
    {
        GUID dimID;
        pImg->GetFrameDimensionsList(&dimID, 1); // 첫 차원 (GIF: FrameDimensionTime)
        m_iFrameCount = pImg->GetFrameCount(&dimID);
    }

    if (m_iFrameCount <= 1)
    {
        m_iFrameCount = 1; // 정지 이미지
        return true;
    }

    // 프레임별 지연 시간 읽기 (PropertyTagFrameDelay: 1/100초 단위 배열)
    UINT iSize = pImg->GetPropertyItemSize(PropertyTagFrameDelay);
    if (iSize > 0)
    {
        PropertyItem* pItem = (PropertyItem*)malloc(iSize);
        if (pItem && pImg->GetPropertyItem(PropertyTagFrameDelay, iSize, pItem) == Ok)
        {
            UINT* pRaw = (UINT*)pItem->value;
            m_pFrameDelays = new _uint[m_iFrameCount];
            for (_uint i = 0; i < m_iFrameCount; ++i)
            {
                _uint ms = pRaw[i] * 10;        // 1/100초 → ms
                if (ms < 20) ms = 100;          // 0 또는 너무 짧으면 보정 (브라우저 관례)
                m_pFrameDelays[i] = ms;
            }
        }
        if (pItem) free(pItem);
    }

    // 지연 정보를 못 읽었으면 기본 100ms
    if (!m_pFrameDelays)
    {
        m_pFrameDelays = new _uint[m_iFrameCount];
        for (_uint i = 0; i < m_iFrameCount; ++i)
            m_pFrameDelays[i] = 100;
    }

    return true;
}

void CLobbyWindow::OnPaint(HWND hWnd)
{
    using namespace Gdiplus;

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    if (W <= 0 || H <= 0) { EndPaint(hWnd, &ps); return; }

    // 더블버퍼 백버퍼 (크기 바뀔 때만 재생성)
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
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);

        if (m_pImage)
        {
            Image* pImg = static_cast<Image*>(m_pImage);

            if (m_iFrameCount > 1)
                pImg->SelectActiveFrame(&FrameDimensionTime, m_iCurFrame);

            // 창을 꽉 채우도록 'cover' 비율 계산 (넘치는 부분은 잘림)
            REAL iw = (REAL)pImg->GetWidth();
            REAL ih = (REAL)pImg->GetHeight();
            REAL sx = (REAL)W / iw;
            REAL sy = (REAL)H / ih;
            REAL scale = (sx > sy) ? sx : sy;
            REAL dw = iw * scale;
            REAL dh = ih * scale;
            REAL dx = (W - dw) * 0.5f;
            REAL dy = (H - dh) * 0.5f;
            g.DrawImage(pImg, RectF(dx, dy, dw, dh));
        }
        else
        {
            // ---- 이미지가 없을 때 placeholder ----
            LinearGradientBrush bg(
                Point(0, 0), Point(0, H),
                Color(255, 26, 28, 38), Color(255, 12, 13, 20));
            g.FillRectangle(&bg, 0, 0, W, H);

            FontFamily ff(L"맑은 고딕");
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);

            // 타이틀 (알파를 살짝 맥동시켜 애니메이션 동작 확인)
            BYTE a = (BYTE)(200 + 55 * sinf(m_fAnimTime * 2.0f));
            Gdiplus::Font titleFont(&ff, 64, FontStyleBold, UnitPixel);
            SolidBrush titleBrush(Color(a, 235, 238, 245));
            g.DrawString(L"HelloDinner", -1, &titleFont,
                RectF(0, (REAL)H * 0.34f, (REAL)W, 90), &sf, &titleBrush);

            Gdiplus::Font hintFont(&ff, 18, FontStyleRegular, UnitPixel);
            SolidBrush hintBrush(Color(190, 200, 205, 215));
            g.DrawString(L"여기에 로비 메인 이미지(GIF/PNG)를 넣으면 자동으로 재생됩니다",
                -1, &hintFont, RectF(0, (REAL)H * 0.49f, (REAL)W, 40), &sf, &hintBrush);
        }

    }

    // 백버퍼 → 화면
    Graphics screen(hdc);
    screen.DrawImage(pBack, 0, 0, W, H);

    EndPaint(hWnd, &ps);
}

CLobbyWindow* CLobbyWindow::Create(HINSTANCE hInstance)
{
    CLobbyWindow* pInstance = new CLobbyWindow();

    if (FAILED(pInstance->Initialize(hInstance)))
    {
        MSG_BOX("Failed to Created : CLobbyWindow");
        Safe_Release(pInstance);
        return nullptr;
    }

    return pInstance;
}

void CLobbyWindow::Free()
{
    Close();
    __super::Free();
}