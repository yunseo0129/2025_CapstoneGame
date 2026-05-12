#pragma once

#include "Base.h"

class CGameInstance;

class CMainApp : public CBase
{
public:
	CMainApp();
	~CMainApp() = default;

public:
	HRESULT Initialize();
	void Update(_float fTimeDelta);
	HRESULT Render();

private:
	CGameInstance* m_pGameInstance = { nullptr };
	wstring m_strMainWndCaption = L"HelloDinner";

	ENGINE_DESC EngineDesc = {};
	EngineContext EngineContext = {};

	bool m_isLoading = {false};

private:
	HRESULT SetUp_StartLevel(LEVELID eLevelID);

public:
	static CMainApp* Create();
	virtual void Free() override;
};

