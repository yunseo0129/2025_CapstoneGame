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

void CLevel_Manager::Bind_CameraBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex, CAMERA_TYPE _eType)
{
	if (nullptr != m_pCurrentLevel)
		m_pCurrentLevel->Bind_CameraBuffer(pCmdList, _eIndex, _eType);
}

XMFLOAT4X4 CLevel_Manager::Get_CurrentCameraView ()
{
	if (nullptr != m_pCurrentLevel)
		return m_pCurrentLevel->Get_CurrentCameraView();
	return XMFLOAT4X4 ();
}

XMFLOAT4X4 CLevel_Manager::Get_CurrentCameraProjection ()
{
	if (nullptr != m_pCurrentLevel)
		return m_pCurrentLevel->Get_CurrentCameraProjection();
	return XMFLOAT4X4 ();
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
	Safe_Release(m_pGameInstance);
	Safe_Release(m_pCurrentLevel);

	__super::Free();
}
