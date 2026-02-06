#pragma once

#include "Base.h"
#include "Timer.h"
#include "Scene.h"


class CMainApp : public CBase
{
public:
	CMainApp();
	~CMainApp();

public:
	
	// input
	void MouseEvent(HWND _hWnd, FLOAT _timeElapsed);
	void KeyboardEvent(FLOAT _timeElapsed);
	void MouseEvent(UINT _message, LPARAM _lParam);
	void KeyboardEvent(HWND _hWnd, UINT _message, WPARAM _wParam, LPARAM _lParam);


	//gameinstance
	void BuildObjects();


	CMainApp* Create();
	void SetActive(BOOL _isActive);
	void FrameAdvance();
	
protected:
	// level
	void CreateRootSignature();

private:
	void Update();
	HRESULT Render();
	bool Initialize(HINSTANCE _hInstance, HWND _hMainWnd);

private:
	wstring m_strMainWndCaption = L"HelloDinner";
};

