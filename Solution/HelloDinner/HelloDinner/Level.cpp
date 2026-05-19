#include "Level.h"
#include "Camera.h"
#include "Light.h"
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
        pCamera->Late_Update(fTimeDelta);
	}
	for (auto& pLight : m_pLights) {
		if (pLight == nullptr)
			continue;
		if (m_pCurrentCamera == nullptr)
			continue;
		pLight->Update(fTimeDelta);
	}
}

void CLevel::Update_Shadows(_float fTimeDelta)
{
    if (m_pCurrentCamera == nullptr)
        return;
    for (auto& pLight : m_pLights) {
        if (pLight == nullptr) continue;
        pLight->Update_Shadow(m_pCurrentCamera);
    }
}

void CLevel::Begin_ShadowPass(ID3D12GraphicsCommandList* cmdList)
{
    if (m_pCurrentCamera == nullptr) return;
    for (auto& pLight : m_pLights) {
        if (pLight == nullptr) continue;
        pLight->Begin_ShadowPass(cmdList);
    }
}

void CLevel::End_ShadowPass(ID3D12GraphicsCommandList* cmdList)
{
    if (m_pCurrentCamera == nullptr) return;
    for (auto& pLight : m_pLights) {
        if (pLight == nullptr) continue;
        pLight->End_ShadowPass(cmdList);
    }
}

_bool CLevel::IsSphereInShadowFrustum(const _float3& vCenter, _float fRadius) const
{
    for (auto& pLight : m_pLights) {
        if (pLight && pLight->Has_Shadow())
            return pLight->IsSphereInShadowBounds(vCenter, fRadius);
    }
    return true;  // 그림자 광원 없음 → 통과 (안전)
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
	m_pCurrentCamera->Bind_CameraBuffer(pCmdList, _eIndex);

	Get_CameraMatrix(_eType);
}

void CLevel::Bind_LightBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex)
{
	if (_eIndex >= RootParameterIndex::End)
		return;
	for (auto& pLight : m_pLights) {
		if (pLight == nullptr)
			continue;
		pLight->Bind_LightBuffer(pCmdList, _eIndex);
	}
}

void CLevel::Get_CameraMatrix(CAMERA_TYPE _eType)
{
	if (m_pCamera[_eType] != nullptr)
	{
		m_pCurrentCamera = m_pCamera[_eType];
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
