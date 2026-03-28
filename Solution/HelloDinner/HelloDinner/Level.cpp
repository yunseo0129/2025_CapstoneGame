#include "Level.h"
#include "Camera.h"
#include "GameInstance.h"

CLevel::CLevel( EngineContext* pContext)
	: m_pContext{ pContext }
	, m_pGameInstance{ CGameInstance::GetInstance() }
{
	m_pCamera.resize(CAMERA_END, nullptr);
	Safe_AddRef(m_pContext->cmdList);
	Safe_AddRef(m_pContext->device);
	Safe_AddRef(m_pContext->dsvHeap);
	Safe_AddRef(m_pContext->cmdQueue);
	Safe_AddRef(m_pContext->rtvHeap);
	Safe_AddRef(m_pContext->srvHeap);
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
}

void CLevel::Free()
{
	for (auto& pCamera : m_pCamera)
		Safe_Release(pCamera);
	m_pCamera.clear();

	Safe_Release(m_pGameInstance);
	Safe_Release(m_pContext->cmdList);
	Safe_Release(m_pContext->device);
	Safe_Release(m_pContext->dsvHeap);
	Safe_Release(m_pContext->cmdQueue);
	Safe_Release(m_pContext->rtvHeap);
	Safe_Release(m_pContext->srvHeap);
	__super::Free();
}
