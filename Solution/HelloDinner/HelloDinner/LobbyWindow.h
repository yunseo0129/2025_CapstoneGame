#pragma once

#include "Base.h"

// =====================================================================
//  CLobbyWindow
//  - 게임 실행 시 가장 먼저 뜨는 로비 창 (Win32 + GDI+)
//  - 메인 이미지를 크게 띄우며, GIF면 자동으로 애니메이션 재생
//  - 우하단에 버튼 3개: [게임 시작] / [방 만들기] / [방 들어가기]
//  - DoModal() 로 띄우고, 사용자가 버튼을 누르면 결과를 반환
// =====================================================================

// 로비에서 사용자가 선택한 결과
enum LOBBY_RESULT
{
    LOBBY_NONE,         // 아직 선택 안 함 (모달 루프 진행 중)
    LOBBY_START_GAME,   // [게임 시작] - 곧바로 게임 화면으로
    LOBBY_CREATE_ROOM,  // [방 만들기] - 대기방(방장)으로
    LOBBY_JOIN_ROOM,    // [방 들어가기] - 대기방(참가자)으로
    LOBBY_EXIT,         // 창을 닫음 - 프로그램 종료
};

class CLobbyWindow final: public CBase
{
public:
    // 로비 창 크기 (게임 해상도와 동일하게)
    static const _uint WIDTH = 1280;
    static const _uint HEIGHT = 720;

private:
    CLobbyWindow();
    virtual ~CLobbyWindow() = default;

public:
    HRESULT      Initialize(HINSTANCE hInstance);

    // 창을 띄우고 버튼을 누를 때까지 메시지를 펌프한다. 선택 결과를 반환.
    LOBBY_RESULT DoModal();

    void         Close();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void  OnCreate(HWND hWnd);
    void  OnPaint(HWND hWnd);
    void  OnCommand(_uint iCtrlID);

    // 배경 이미지/움짤 로드 (성공 시 true). GIF면 프레임 정보까지 읽음.
    bool  LoadBackground(const _tchar* szPath);

private:
    HWND         m_hWnd = nullptr;
    HINSTANCE    m_hInstance = nullptr;
    LOBBY_RESULT m_eResult = LOBBY_NONE;

    HFONT        m_hFontBtn = nullptr;

    // ---- GDI+ 배경(애니메이션) ----
    // 헤더에 <gdiplus.h> 를 끌어오지 않으려고 void* 로 보관 (cpp 에서 캐스팅)
    void* m_pImage = nullptr;  // Gdiplus::Image*
    void* m_pBackBuffer = nullptr;  // Gdiplus::Bitmap*  (더블버퍼)
    _uint        m_iFrameCount = 0;        // 1 이면 정지 이미지
    _uint        m_iCurFrame = 0;
    _uint* m_pFrameDelays = nullptr; // 프레임별 지연(ms)
    _float       m_fAnimTime = 0.f;     // 이미지가 없을 때 placeholder 연출용

    static const _tchar* WND_CLASS_NAME;

public:
    static CLobbyWindow* Create(HINSTANCE hInstance);
    virtual void Free() override;
};