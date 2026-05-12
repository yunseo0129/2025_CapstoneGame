#include "stdafx.h"
#include "LoadingWindow.h"

#pragma comment(lib, "gdi32.lib")

const _tchar* CLoadingWindow::WND_CLASS_NAME = TEXT("HelloDinnerLoadingWnd");

CLoadingWindow::CLoadingWindow()
{
}

HRESULT CLoadingWindow::Initialize(HINSTANCE hInstance, HWND hParent)
{
    m_hInstance = hInstance;

    // 윈도우 클래스 등록 (이미 등록되어 있어도 GetLastError로 확인하지 않고 그냥 진행)
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = WND_CLASS_NAME;
    RegisterClassEx(&wcex);

    // 부모 윈도우(또는 화면) 중앙 좌표 계산
    RECT rcParent;
    if (hParent && GetWindowRect(hParent, &rcParent))
    {
        // 부모 좌표 그대로 사용
    }
    else
    {
        rcParent.left = 0; rcParent.top = 0;
        rcParent.right = GetSystemMetrics(SM_CXSCREEN);
        rcParent.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int x = (rcParent.left + rcParent.right) / 2 - WIDTH / 2;
    int y = (rcParent.top + rcParent.bottom) / 2 - HEIGHT / 2;

    // 팝업 스타일 (타이틀바 없음, 항상 위)
    m_hWnd = CreateWindowEx(
        WS_EX_TOPMOST,
        WND_CLASS_NAME,
        TEXT("Loading"),
        WS_POPUP | WS_BORDER | WS_VISIBLE,
        x, y, WIDTH, HEIGHT,
        nullptr,    // 부모 X (탑레벨로 띄움)
        nullptr,
        hInstance,
        this);

    if (nullptr == m_hWnd)
        return E_FAIL;

    // WndProc에서 this 포인터 회수할 수 있도록 GWLP_USERDATA에 저장
    SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);

    UpdateWindow(m_hWnd);
    return S_OK;
}

void CLoadingWindow::Update(const _tchar* pText, _float fProgress)
{
    if (pText)
        lstrcpyn(m_szText, pText, MAX_PATH);
    m_fProgress = fProgress;

    // WM_PAINT 트리거
    if (m_hWnd)
        InvalidateRect(m_hWnd, nullptr, FALSE);
}

void CLoadingWindow::Close()
{
    if (m_hWnd)
    {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

LRESULT CALLBACK CLoadingWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CLoadingWindow* pSelf = (CLoadingWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_PAINT:
        if (pSelf)
            pSelf->OnPaint(hWnd);
        return 0;

    case WM_DESTROY:
        // 메인 윈도우가 아니므로 PostQuitMessage 호출 안 함
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void CLoadingWindow::OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rc;
    GetClientRect(hWnd, &rc);

    // ---- 배경 ----
    HBRUSH hBgBrush = CreateSolidBrush(RGB(40, 44, 52));
    FillRect(hdc, &rc, hBgBrush);
    DeleteObject(hBgBrush);

    // ---- 텍스트 ----
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220, 220, 220));

    HFONT hFont = CreateFont(
        20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH, TEXT("맑은 고딕"));
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);

    RECT rcText = rc;
    rcText.bottom = rc.bottom - 50;
    DrawText(hdc, m_szText, -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // ---- 진행바 (fProgress >= 0일 때만) ----
    if (m_fProgress >= 0.f)
    {
        const int barMargin = 30;
        const int barHeight = 14;
        RECT rcBarBg = {rc.left + barMargin, rc.bottom - 35,
            rc.right - barMargin, rc.bottom - 35 + barHeight};

        // 배경
        HBRUSH hBarBg = CreateSolidBrush(RGB(80, 80, 80));
        FillRect(hdc, &rcBarBg, hBarBg);
        DeleteObject(hBarBg);

        // 채워진 부분
        RECT rcBar = rcBarBg;
        int width = rcBar.right - rcBar.left;
        _float fProg = (m_fProgress > 1.f) ? 1.f : m_fProgress;
        rcBar.right = rcBar.left + (LONG)(width * fProg);

        HBRUSH hBarFg = CreateSolidBrush(RGB(80, 200, 120));
        FillRect(hdc, &rcBar, hBarFg);
        DeleteObject(hBarFg);
    }

    SelectObject(hdc, hOld);
    DeleteObject(hFont);

    EndPaint(hWnd, &ps);
}

CLoadingWindow* CLoadingWindow::Create(HINSTANCE hInstance, HWND hParent)
{
    CLoadingWindow* pInstance = new CLoadingWindow();

    if (FAILED(pInstance->Initialize(hInstance, hParent)))
    {
        MSG_BOX("Failed to Created : CLoadingWindow");
        Safe_Release(pInstance);
        return nullptr;
    }

    return pInstance;
}

void CLoadingWindow::Free()
{
    Close();
    __super::Free();
}