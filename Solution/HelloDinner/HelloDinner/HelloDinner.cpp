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

        CRoomWindow* pRoom = CRoomWindow::Create(hInstance, bIsHost);
        if (nullptr == pRoom)
            return false;

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