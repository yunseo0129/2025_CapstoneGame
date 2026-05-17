#include "stdafx.h"
#include "Level_Loading.h"

#include "Loader.h"
//#include "Level_Logo.h"
#include "LoadingWindow.h"
#include "Level_Gameplay.h"

#include "GameInstance.h"


CLevel_Loading::CLevel_Loading(EngineContext* pContext)
    : CLevel{ pContext }
{

}

HRESULT CLevel_Loading::Initialize(LEVELID eNextLevelID)
{
    m_eNextLevelID = eNextLevelID;

    // 메인 위에 윈도우 
    m_pLoadingWindow = CLoadingWindow::Create(g_hInst, g_hWnd);
    if (nullptr == m_pLoadingWindow)
        return E_FAIL;

   // 로딩
    m_pLoader = CLoader::Create(m_pContext, eNextLevelID);
    if (nullptr == m_pLoader)
        return E_FAIL;

    return S_OK;
}

void CLevel_Loading::Update(_float fTimeDelta)
{
    __super::Update(fTimeDelta);

    if (nullptr == m_pLoader)
        return;

    // CLoader의 현재 텍스트를 LoadingWindow에 전달
    if (m_pLoadingWindow)
    {
        _tchar szText[MAX_PATH] = {};
        m_pLoader->Get_LoadingText(szText, MAX_PATH);
        m_pLoadingWindow->Update(szText, -1.f);
        // 추후 진행률을 두번째 인자로 전달 (0.0 ~ 1.0)
    }

    // 로딩 완료
    if (m_pLoader->isFinished())
    {
        m_pGameInstance->ResetCmdList();

        // 다음 레벨 객체 생성
        CLevel* pNextLevel = Create_NextLevel();
        if (nullptr == pNextLevel) {
            m_pGameInstance->CloseCmdList();
            return;
        }

        m_pGameInstance->CloseCmdList();

        // 로딩 윈도우 닫기
        if (m_pLoadingWindow)
            m_pLoadingWindow->Close();

        m_pGameInstance->Open_Level(m_eNextLevelID, pNextLevel);
        return;
    }
}

HRESULT CLevel_Loading::Render()
{
    return S_OK;
}

CLevel* CLevel_Loading::Create_NextLevel()
{
    switch (m_eNextLevelID)
    {
    case LEVEL_GAMEPLAY:
        return CLevel_GamePlay::Create(m_pContext);

    //case LEVEL_LOGO:

    default:
        MSG_BOX("CLevel_Loading: Unknown next level ID");
        return nullptr;
    }
}

CLevel_Loading* CLevel_Loading::Create(EngineContext* pContext, LEVELID eNextLevelID)
{
    CLevel_Loading* pInstance = new CLevel_Loading(pContext);

    if (FAILED(pInstance->Initialize(eNextLevelID)))
    {
        MSG_BOX("Failed to Created : CLevel_Loading");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CLevel_Loading::Free()
{
    Safe_Release(m_pLoadingWindow);
    Safe_Release(m_pLoader);
    __super::Free();
}