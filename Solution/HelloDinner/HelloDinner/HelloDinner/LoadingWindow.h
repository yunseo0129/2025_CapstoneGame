#pragma once

#include "Base.h"

class CLoadingWindow final: public CBase
{
public:
    static const _uint WIDTH = 480;
    static const _uint HEIGHT = 200;

private:
    CLoadingWindow();
    virtual ~CLoadingWindow() = default;

public:
    HRESULT Initialize(HINSTANCE hInstance, HWND hParent);

    // 매 프레임 호출. 텍스트/진행률 갱신 + InvalidateRect로 다시 그리기 요청
    // fProgress: 0.0 ~ 1.0 범위. 음수면 진행바 미표시
    void   Update(const _tchar* pText, _float fProgress = -1.f);

    // 윈도우 닫기
    void   Close();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void                    OnPaint(HWND hWnd);

private:
    HWND       m_hWnd = nullptr;
    HINSTANCE  m_hInstance = nullptr;

    _tchar     m_szText[MAX_PATH] = {};
    _float     m_fProgress = -1.f;

    static const _tchar* WND_CLASS_NAME;

public:
    static CLoadingWindow* Create(HINSTANCE hInstance, HWND hParent);
    virtual void Free() override;
};