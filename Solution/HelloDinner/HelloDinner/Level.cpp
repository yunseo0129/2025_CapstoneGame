#include "Level.h"
#include "Camera.h"
#include "GameInstance.h"

CLevel::CLevel( EngineContext* pContext)
	: m_pContext{ pContext }
	, m_pGameInstance{ CGameInstance::GetInstance() }
{
	m_pCamera.resize(CAMERA_END, nullptr);
	Safe_AddRef(m_pGameInstance);
}

HRESULT CLevel::Initialize()
{
	return S_OK;
}

void CLevel::Update(_float fTimeDelta)
{
	for (auto& pCamera : m_pCamera) {
		if (pCamera == nullptr)
			continue;
		pCamera->Update(fTimeDelta);
	}
}

HRESULT CLevel::Render()
{
	return S_OK;
}

void CLevel::Add_Camera()
{
}

void CLevel::Bind_CameraBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex, CAMERA_TYPE _eType)
{
	if (_eIndex >= RootParameterIndex::End)
		return;
	if (m_pCamera[_eType] == nullptr)
		return;
	m_pCamera[_eType]->Bind_CameraBuffer(pCmdList, _eIndex);

	Get_CameraMatrix(_eType);
}

void CLevel::Get_CameraMatrix(CAMERA_TYPE _eType)
{
	if (m_pCamera[_eType] != nullptr)
	{
		m_xmf4x4CurrentView = m_pCamera[_eType]->Get_CameraView();
		m_xmf4x4CurrentProjection = m_pCamera[_eType]->Get_CameraProjection();
	}
}

void CLevel::Free()
{
	for (auto& pCamera : m_pCamera)
		Safe_Release(pCamera);
	m_pCamera.clear();

	Safe_Release(m_pGameInstance);

	__super::Free();
}
