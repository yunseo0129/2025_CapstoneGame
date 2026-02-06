#pragma once

#include "Base.h"

class CGameInstance;

class CMainApp : public CBase
{
public:
	CMainApp();
	~CMainApp() = default;

	void FrameAdvance();

private:
	void Update(_float fTimeDelta);
	HRESULT Render();
	HRESULT Initialize();

private:
	CGameInstance* m_pGameInstance = { nullptr };
	wstring m_strMainWndCaption = L"HelloDinner";

public:
	static CMainApp* Create();
	virtual void Free() override;
};

