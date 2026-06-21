// HelloDinner.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include "HelloDinner.h"

#include "MainApp.h"
#include "GameInstance.h"
#include "NetworkClient.h"

// 로비 / 대기방 (Win32 + GDI+)
#include "LobbyWindow.h"
#include "RoomWindow.h"

#include "Game_Manager.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#define MAX_LOADSTRING 100

// 전역 변수:
HINSTANCE           g_hInst;                                  // 현재 인스턴스입니다.
HWND                g_hWnd;
MATCH_SETUP         g_MatchSetup;                             // 대기방에서 고른 팀/번호(게임플레이로 전달)
WCHAR               szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR               szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// ---------------------------------------------------------------------
//  방 코드 입력 창 (GDI+ 테마 — 로비/대기방과 동일한 느낌)
//   - [방 들어가기] 클릭 시 먼저 띄운다.
//   - [접속] 누르면 입력 코드를 정수로 반환, [취소]/닫기면 -1 반환.
//   - 자체 모달 루프를 돈다(대기방 DoModal 과 동일한 패턴).
// ---------------------------------------------------------------------
namespace
{
    constexpr int IDC_ROOMCODE_EDIT = 3101;
    constexpr int IDC_ROOMCODE_OK = 3102;
    constexpr int IDC_ROOMCODE_CANCEL = 3103;

    constexpr int ROOMCODE_W = 420;
    constexpr int ROOMCODE_H = 240;

    // 창 ↔ 모달 루프 사이 상태 공유.
    struct ROOMCODE_STATE
    {
        int    iResultCode = -1;   // 확정된 코드(>=0) 또는 취소(-1)
        bool   bDone = false;       // 모달 종료 플래그
        HWND   hEdit = nullptr;
        HFONT  hFontUI = nullptr;   // 버튼/에디트용
        void* pBack = nullptr;     // Gdiplus::Bitmap* 더블버퍼
    };

    void RoomCode_Paint(HWND hWnd)
    {
        using namespace Gdiplus;

        ROOMCODE_STATE* pState = reinterpret_cast<ROOMCODE_STATE*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rc; GetClientRect(hWnd, &rc);
        int W = rc.right - rc.left, H = rc.bottom - rc.top;
        if (W <= 0 || H <= 0) { EndPaint(hWnd, &ps); return; }

        Bitmap* pBack = pState ? static_cast<Bitmap*>(pState->pBack) : nullptr;
        if (!pBack || (int)pBack->GetWidth() != W || (int)pBack->GetHeight() != H)
        {
            if (pBack) delete pBack;
            pBack = new Bitmap(W, H, PixelFormat32bppPARGB);
            if (pState) pState->pBack = pBack;
        }

        {
            Graphics g(pBack);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintAntiAlias);

            FontFamily ff(L"맑은 고딕");

            // 배경 (대기방과 동일 톤)
            SolidBrush bgBrush(Color(255, 22, 24, 32));
            g.FillRectangle(&bgBrush, 0, 0, W, H);

            // 헤더 바 (대기방과 동일 그라데이션)
            LinearGradientBrush headBrush(
                Point(0, 0), Point(0, 64),
                Color(255, 46, 80, 130), Color(255, 32, 56, 96));
            g.FillRectangle(&headBrush, 0, 0, W, 64);

            Gdiplus::Font hTitle(&ff, 24, FontStyleBold, UnitPixel);
            SolidBrush wbr(Color(255, 240, 244, 250));
            g.DrawString(L"방 들어가기", -1, &hTitle, PointF(24, 18), &wbr);

            // 안내 라벨
            Gdiplus::Font lblF(&ff, 16, FontStyleRegular, UnitPixel);
            SolidBrush lblBr(Color(220, 205, 212, 226));
            g.DrawString(L"방 코드를 입력하세요 (숫자)", -1, &lblF, PointF(24, 92), &lblBr);

            // 에디트 박스 배경(테두리 느낌) — 실제 입력은 자식 EDIT 컨트롤이 담당.
            REAL ex = 24.f, ey = 122.f, ew = (REAL)W - 48.f, eh = 38.f;
            SolidBrush eBg(Color(255, 14, 15, 22));
            g.FillRectangle(&eBg, ex, ey, ew, eh);
            Pen ePen(Color(255, 70, 90, 130), 1.5f);
            g.DrawRectangle(&ePen, ex, ey, ew, eh);
        }

        Graphics screen(hdc);
        screen.DrawImage(pBack, 0, 0, W, H);
        EndPaint(hWnd, &ps);
    }

    LRESULT CALLBACK RoomCode_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (WM_NCCREATE == msg)
        {
            CREATESTRUCT* pCS = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pCS->lpCreateParams);
        }
        ROOMCODE_STATE* pState = reinterpret_cast<ROOMCODE_STATE*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        switch (msg)
        {
        case WM_CREATE:
        {
            RECT rc; GetClientRect(hWnd, &rc);
            int W = rc.right - rc.left;

            // 입력 EDIT (테마 박스 안쪽에 겹쳐 배치)
            pState->hEdit = CreateWindowEx(
                0, TEXT("EDIT"), TEXT(""),
                WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
                30, 130, W - 60, 24,
                hWnd, (HMENU)(UINT_PTR)IDC_ROOMCODE_EDIT,
                (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), nullptr);

            // 버튼 2개 ([접속]/[취소])
            const int btnW = 120, btnH = 40, gap = 16;
            const int by = 178;
            const int totalW = btnW * 2 + gap;
            int bx = (W - totalW) / 2;

            HWND hOK = CreateWindowEx(0, TEXT("BUTTON"), TEXT("접속"),
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                bx, by, btnW, btnH, hWnd, (HMENU)(UINT_PTR)IDC_ROOMCODE_OK,
                (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), nullptr);
            HWND hCancel = CreateWindowEx(0, TEXT("BUTTON"), TEXT("취소"),
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                bx + btnW + gap, by, btnW, btnH, hWnd, (HMENU)(UINT_PTR)IDC_ROOMCODE_CANCEL,
                (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), nullptr);

            // 공용 폰트 적용 (대기방 버튼과 동일 톤)
            pState->hFontUI = CreateFont(
                22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, TEXT("맑은 고딕"));
            if (pState->hFontUI)
            {
                SendMessage(pState->hEdit, WM_SETFONT, (WPARAM)pState->hFontUI, TRUE);
                SendMessage(hOK, WM_SETFONT, (WPARAM)pState->hFontUI, TRUE);
                SendMessage(hCancel, WM_SETFONT, (WPARAM)pState->hFontUI, TRUE);
            }
            SetFocus(pState->hEdit);
            return 0;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_ROOMCODE_OK:
            {
                _tchar szBuf[32] = {};
                GetWindowText(pState->hEdit, szBuf, _countof(szBuf));
                if (szBuf[0] == L'\0')
                {
                    MessageBox(hWnd, TEXT("방 코드를 입력하세요."), TEXT("알림"), MB_OK | MB_ICONINFORMATION);
                    return 0;
                }
                pState->iResultCode = _wtoi(szBuf);
                pState->bDone = true;
                return 0;
            }
            case IDC_ROOMCODE_CANCEL:
                pState->iResultCode = -1;
                pState->bDone = true;
                return 0;
            }
            return 0;

        case WM_PAINT:
            RoomCode_Paint(hWnd);
            return 0;

        case WM_CTLCOLOREDIT:
        {
            // 입력 박스를 어두운 테마로(흰 배경 대신).
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, RGB(235, 240, 248));
            SetBkColor(hdcEdit, RGB(14, 15, 22));
            static HBRUSH s_hEditBrush = nullptr;
            if (!s_hEditBrush) s_hEditBrush = CreateSolidBrush(RGB(14, 15, 22));
            return (LRESULT)s_hEditBrush;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_CLOSE:
            if (pState) { pState->iResultCode = -1; pState->bDone = true; }
            return 0;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    // GDI+ 테마 방코드 입력 창을 모달로 띄운다. 반환: 코드(>=0) 또는 -1.
    int Prompt_RoomCode(HINSTANCE hInstance, HWND hParent)
    {
        static const _tchar* CLS = TEXT("HelloDinnerRoomCodeWnd");
        static bool bRegistered = false;
        if (!bRegistered)
        {
            WNDCLASSEX wc = {};
            wc.cbSize = sizeof(wc);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = RoomCode_WndProc;
            wc.hInstance = hInstance;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            wc.lpszClassName = CLS;
            RegisterClassEx(&wc);
            bRegistered = true;
        }

        ROOMCODE_STATE state;

        DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
        RECT rc = {0, 0, ROOMCODE_W, ROOMCODE_H};
        AdjustWindowRect(&rc, dwStyle, FALSE);
        int wW = rc.right - rc.left, wH = rc.bottom - rc.top;
        int wX = (GetSystemMetrics(SM_CXSCREEN) - wW) / 2;
        int wY = (GetSystemMetrics(SM_CYSCREEN) - wH) / 2;

        HWND hWnd = CreateWindowEx(
            0, CLS, TEXT("방 들어가기"),
            dwStyle, wX, wY, wW, wH,
            hParent, nullptr, hInstance, &state);

        if (!hWnd)
            return -1;

        // 부모가 있으면 모달처럼 비활성화
        if (hParent) EnableWindow(hParent, FALSE);
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);
        SetForegroundWindow(hWnd);

        // 자체 모달 루프
        MSG msg;
        while (!state.bDone)
        {
            if (GetMessage(&msg, nullptr, 0, 0) <= 0) { state.iResultCode = -1; break; }
            // 엔터/ESC 및 탭 이동 처리
            if (!IsDialogMessage(hWnd, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        if (hParent) EnableWindow(hParent, TRUE);
        if (state.pBack) { delete static_cast<Gdiplus::Bitmap*>(state.pBack); state.pBack = nullptr; }
        if (state.hFontUI) { DeleteObject(state.hFontUI); state.hFontUI = nullptr; }
        DestroyWindow(hWnd);

        return state.iResultCode;
    }

    // 네트워크 연결 (스텁). 지금은 항상 성공으로 간주.
    //  TODO: NetworkClient::GetInstance()->ConnectWithConsole() 등으로 실제 접속/방 입장.
    bool Connect_ToRoom(int iRoomCode)
    {
        (void)iRoomCode;
        return true;
    }
}

// ---------------------------------------------------------------------
//  로비 → (대기방) → 게임 흐름.
//  반환값 true  : 게임을 시작해야 함 (이후 엔진/게임플레이 진입)
//  반환값 false : 사용자가 종료를 선택함
// ---------------------------------------------------------------------
static bool RunFrontend(HINSTANCE hInstance)
{
    while (true)
    {
        // ---- 로비 창 ----
        CLobbyWindow* pLobby = CLobbyWindow::Create(hInstance);
        if (nullptr == pLobby)
            return false;

        LOBBY_RESULT eLobby = pLobby->DoModal();
        Safe_Release(pLobby);

        if (LOBBY_EXIT == eLobby)
            return false;                         // 프로그램 종료
        if (LOBBY_START_GAME == eLobby)
        {
            // 대기방을 건너뛴 빠른 시작: 기본값 RED / 1번.
            g_MatchSetup.iTeam = 0;
            g_MatchSetup.iNumber = 1;
            return true;                          // 곧바로 게임 시작
        }

        // [방 만들기] / [방 들어가기] → 대기방
        bool bIsHost = (LOBBY_CREATE_ROOM == eLobby);

        int iJoinCode = -1;
        if (!bIsHost)
        {
            // [방 들어가기]: 먼저 방 코드 입력 → 네트워크 연결 → 대기방 입장.
            iJoinCode = Prompt_RoomCode(hInstance, nullptr);
            if (iJoinCode < 0)
                continue;                          // 취소 → 로비로 복귀

            if (!Connect_ToRoom(iJoinCode))
            {
                MessageBox(nullptr, TEXT("방에 연결하지 못했습니다."),
                    TEXT("연결 실패"), MB_OK | MB_ICONERROR);
                continue;                          // 연결 실패 → 로비로 복귀
            }
        }

        CRoomWindow* pRoom = CRoomWindow::Create(hInstance, bIsHost);
        if (nullptr == pRoom)
            return false;

        // 참가자는 입력한 코드를 대기방에 표시.
        if (!bIsHost && iJoinCode >= 0)
            pRoom->Set_RoomCode(iJoinCode);

        ROOM_RESULT eRoom = pRoom->DoModal();
        Safe_Release(pRoom);

        if (ROOM_START_GAME == eRoom)
            return true;                          // 대기방에서 게임 시작
        // ROOM_LEAVE → 로비로 돌아가서 반복
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hrCo))
        return FALSE;

    // 서버 접속 (콘솔 창)
    // ※ 로비/대기방 UI만 테스트하려면 아래 블록을 잠시 주석 처리하세요.
    NetworkClient* pNetwork = NetworkClient::GetInstance();
    if (!pNetwork->ConnectWithConsole()) {
        MessageBox(nullptr, L"서버 접속에 실패했습니다.", L"Error", MB_OK);
        NetworkClient::DestroyInstance();
        return FALSE;
    }

    g_hInst = hInstance;

    // GDI+ 시작 (로비/대기방 그리기에 사용)
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // ================= 로비 / 대기방 =================
    bool bStartGame = RunFrontend(hInstance);

    // 프론트엔드 종료 후 GDI+ 해제
    Gdiplus::GdiplusShutdown(gdiplusToken);

    if (!bStartGame)
    {
        // 게임을 시작하지 않고 종료
        NetworkClient::DestroyInstance();
        CoUninitialize();
        return 0;
    }
    // ================================================

    MSG msg = {};
    HACCEL hAccelTable;

    // 전역 문자열을 초기화합니다.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_HELLODINNER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 애플리케이션 초기화를 수행합니다 (DirectX 메인 윈도우 생성):
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    CMainApp* pMainApp = {nullptr};

    pMainApp = CMainApp::Create();
    if (nullptr == pMainApp)
        return FALSE;

    CGameInstance* pGameInstance = CGameInstance::GetInstance();
    if (nullptr == pGameInstance)
        return FALSE;

    Safe_AddRef(pGameInstance);

    if (FAILED(pGameInstance->Add_Timer(TEXT("Timer_Default"))))
        return E_FAIL;
    if (FAILED(pGameInstance->Add_Timer(TEXT("Timer_60"))))
        return E_FAIL;

    _float		fTimeAcc = {0.f};

    hAccelTable = ::LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_HELLODINNER));

    // 기본 메시지 루프입니다:
    while (true)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else {
            pGameInstance->Update_TimeDelta(TEXT("Timer_60"));
            pMainApp->Update(pGameInstance->Get_TimeDelta(TEXT("Timer_60")));
            pMainApp->Render();
        }
    }

    // 네트워크 해제
    NetworkClient::DestroyInstance();

    Safe_Release(pGameInstance);

    Safe_Release(pMainApp);

    //gGameFramework.OnDestroy();

    return (int)msg.wParam;
}



//
//  함수: MyRegisterClass()
//
//  용도: 창 클래스를 등록합니다.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HELLODINNER));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = 0;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   함수: InitInstance(HINSTANCE, int)
//
//   용도: 인스턴스 핸들을 저장하고 주 창을 만듭니다.
//
//   주석:
//
//        이 함수를 통해 인스턴스 핸들을 전역 변수에 저장하고
//        주 프로그램 창을 만든 다음 표시합니다.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    g_hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    g_hWnd = hWnd;

    return TRUE;
}

//
//  함수: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  용도: 주 창의 메시지를 처리합니다.
//
//  WM_COMMAND  - 애플리케이션 메뉴를 처리합니다.
//  WM_PAINT    - 주 창을 그립니다.
//  WM_DESTROY  - 종료 메시지를 게시하고 반환합니다.
//

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ACTIVATE:
        break;
        if (CGame_Manager::GetInstance())
            CGame_Manager::GetInstance()->Update_MouseClip();
        break;
    case WM_SIZE:
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        break;
    case WM_CHAR:
    case WM_KEYDOWN:
        break;
    case WM_DESTROY:
        ClipCursor(nullptr);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 정보 대화 상자의 메시지 처리기입니다.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}