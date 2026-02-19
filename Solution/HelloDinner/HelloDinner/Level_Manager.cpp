#include "Level_Manager.h"

#include "Level.h"
#include "GameInstance.h"

CLevel_Manager::CLevel_Manager()
	: m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);
}

HRESULT CLevel_Manager::Open_Level(_int iLevelIndex, CLevel* pNewLevel)
{
	/* 오브젝트 매니져에 추가 해놓은 기존 레벨용 객체들을 삭제한다. */
	/* 컴포넌트 매니져에 추가 해놓은 기존 레벨용 객체들을 삭제한다. */

	if (nullptr != m_pCurrentLevel)
	{


		//m_pGameInstance->Clear(m_iCurrentLevelID);
	}




	Safe_Release(m_pCurrentLevel);

	m_pCurrentLevel = pNewLevel;

	m_iCurrentLevelID = iLevelIndex;

	return S_OK;
}

void CLevel_Manager::Update(_float fTimeDelta)
{
	if (nullptr != m_pCurrentLevel)
		m_pCurrentLevel->Update(fTimeDelta);
}

HRESULT CLevel_Manager::Render()
{
	if (nullptr != m_pCurrentLevel)
		m_pCurrentLevel->Render();

	return S_OK;
}

CLevel_Manager* CLevel_Manager::Create()
{
	return new CLevel_Manager();

}


void CLevel_Manager::Free()
{
	__super::Free();

	Safe_Release(m_pGameInstance);
	Safe_Release(m_pCurrentLevel);
}
