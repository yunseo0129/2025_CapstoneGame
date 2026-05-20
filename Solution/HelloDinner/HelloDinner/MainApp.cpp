#include "stdafx.h"
#include "MainApp.h"
#include "GameInstance.h"
#include <WindowsX.h>

#include "Level_Loading.h"

CMainApp::CMainApp()
	: m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);

}


HRESULT CMainApp::Initialize()
{
	EngineDesc.hInstance = g_hInst;
	EngineDesc.hWnd = g_hWnd;
	EngineDesc.isWindowed = true;
	EngineDesc.iNumLevels = LEVEL_END;
	EngineDesc.iViewportWidth = Client::g_iWinSizeX;
	EngineDesc.iViewportHeight = Client::g_iWinSizeY;

	EngineContext.cmdList = nullptr;
	EngineContext.cmdQueue = nullptr;
	EngineContext.device = nullptr;
	EngineContext.rtvHeap = nullptr;
	EngineContext.dsvHeap = nullptr;
	EngineContext.srvHeap = nullptr;

	if (FAILED(m_pGameInstance->Initialize_Engine(EngineDesc, &EngineContext)))
		return E_FAIL;

	m_pGameInstance->ResetCmdList ();

	if (FAILED(SetUp_StartLevel(LEVEL_GAMEPLAY)))
		return E_FAIL;

	m_pGameInstance->CloseCmdList ();

	m_pGameInstance->ReleaseUploadBuffers (LEVEL_GAMEPLAY);

	return S_OK;
}


void CMainApp::Update(_float fTimeDelta)
{
	m_isLoading = m_pGameInstance->Get_CurrentLevelID() == LEVEL_LOADING;

	if (!m_isLoading) {	// Loading 중이면 Render 작업 하지 않음
		if (FAILED(m_pGameInstance->Render_Begin(_float4 {0.0f, 1.0f, 0.0f, 0.0f})))
			return;
	}

	if (nullptr != m_pGameInstance)
		m_pGameInstance->Update_Engine(fTimeDelta);

	if (m_pGameInstance->Key_Down(DIK_ESCAPE))
		PostQuitMessage(0);

	static float acc = { 0.f };
	static int FPS = { 0 };

	acc += fTimeDelta;

    // culling 되는 지 확인용
    if (m_pGameInstance->Key_Down(DIK_F1))
        m_pGameInstance->Set_CullingEnabled(!m_pGameInstance->Is_CullingEnabled());
    if (m_pGameInstance->Key_Down(DIK_F2))
        m_pGameInstance->Set_ShadowCullingEnabled(!m_pGameInstance->Is_ShadowCullingEnabled());
    if (m_pGameInstance->Key_Down(DIK_F3))
        m_pGameInstance->Set_BVHEnabled(!m_pGameInstance->Is_BVHEnabled());
    if (m_pGameInstance->Key_Down(DIK_F4))
        m_pGameInstance->Set_CullingBVHEnabled(!m_pGameInstance->Is_CullingBVHEnabled());


	if (acc >= 1.f)
	{
        /*
		acc -= 1.f;
		std::wstring title = TEXT("HelloDinner / FPS : ") + std::to_wstring(FPS);
		SetWindowText(g_hWnd, title.c_str());
		FPS = 0;
        */
        acc -= 1.f;
        const wchar_t* szMain = m_pGameInstance->Is_CullingEnabled() ? L"ON" : L"OFF";
        const wchar_t* szShadow = m_pGameInstance->Is_ShadowCullingEnabled() ? L"ON" : L"OFF";
        const wchar_t* szBVH = m_pGameInstance->Is_BVHEnabled() ? L"ON" : L"OFF";
        const wchar_t* szCBVH = m_pGameInstance->Is_CullingBVHEnabled() ? L"ON" : L"OFF";
        _uint iMainR = m_pGameInstance->Get_CullStat_MainRendered();
        _uint iMainT = m_pGameInstance->Get_CullStat_MainTotal();
        _uint iShdR = m_pGameInstance->Get_CullStat_ShadowRendered();
        _uint iShdT = m_pGameInstance->Get_CullStat_ShadowTotal();
        _int iNodes = m_pGameInstance->Get_BVHNodeCount();
        _int iDepth = m_pGameInstance->Get_BVHMaxDepth();
        _int iQ = m_pGameInstance->Get_BVHLastQueryCandidates();
        _int iPrim = m_pGameInstance->Get_BVHPrimitiveCount();
        _int iFC = m_pGameInstance->Get_LastFrustumCandidates();
        _int iSC = m_pGameInstance->Get_LastShadowCandidates();

        std::wstring title = TEXT("HelloDinner / FPS:") + std::to_wstring(FPS)
            + TEXT("  Cull[") + szMain + TEXT("] ") + std::to_wstring(iMainR) + TEXT("/") + std::to_wstring(iMainT)
            + TEXT("  Shadow[") + szShadow + TEXT("] ") + std::to_wstring(iShdR) + TEXT("/") + std::to_wstring(iShdT)
            + TEXT("  BVH[") + szBVH + TEXT("] N=") + std::to_wstring(iNodes) + TEXT(" D=") + std::to_wstring(iDepth)
            + TEXT(" Q=") + std::to_wstring(iQ) + TEXT("/") + std::to_wstring(iPrim)
            + TEXT("  CBVH[") + szCBVH + TEXT("] F=") + std::to_wstring(iFC) + TEXT(" S=") + std::to_wstring(iSC);
        SetWindowText(g_hWnd, title.c_str());
        FPS = 0;
	}

	++FPS;
}

HRESULT CMainApp::Render()
{
	// Loading 중이면 스킵
	if (m_isLoading)
		return S_OK;

	if (nullptr == m_pGameInstance)
		return E_FAIL;

	m_pGameInstance->Draw();

	if (FAILED(m_pGameInstance->Render_End()))
		return E_FAIL;

	return S_OK;
}

HRESULT CMainApp::SetUp_StartLevel(LEVELID eLevelID)
{
	if (FAILED(m_pGameInstance->Open_Level(LEVEL_LOADING,
		CLevel_Loading::Create(&EngineContext, eLevelID))))
		return E_FAIL;

	return S_OK;
}

CMainApp* CMainApp::Create()
{
	CMainApp* pInstance = new CMainApp();

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : CMainApp");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CMainApp::Free()
{

	CGameInstance::Release_Engine();
	Safe_Release(m_pGameInstance);
	/* 부모 멤버를 정리한다. */
	__super::Free();

}


