#include "Level_Manager.h"

#include "Level.h"
#include "Camera.h"
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

void CLevel_Manager::Set_CurrentCamera(CAMERA_TYPE _etype)
{
	if (nullptr != m_pCurrentLevel)
		m_pCurrentLevel->Set_CurrentCamera(_etype);
}

void CLevel_Manager::Bind_CameraBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex, CAMERA_TYPE _eType)
{
	if (nullptr != m_pCurrentLevel)
		m_pCurrentLevel->Bind_CameraBuffer(pCmdList, _eIndex, _eType);
}

void CLevel_Manager::Bind_LightBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex)
{
	if (nullptr != m_pCurrentLevel)
		m_pCurrentLevel->Bind_LightBuffer(pCmdList, _eIndex);
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

CCamera* CLevel_Manager::Get_CurrentCamera()
{
    if (nullptr == m_pCurrentLevel)
        return nullptr;
    return m_pCurrentLevel->Get_CurrentCamera();
}

void CLevel_Manager::Update_Shadows(_float fTimeDelta)
{
    if (m_pCurrentLevel) m_pCurrentLevel->Update_Shadows(fTimeDelta);
}

void CLevel_Manager::Begin_ShadowPass(ID3D12GraphicsCommandList* cmdList)
{
    if (m_pCurrentLevel) m_pCurrentLevel->Begin_ShadowPass(cmdList);
}

void CLevel_Manager::End_ShadowPass(ID3D12GraphicsCommandList* cmdList)
{
    if (m_pCurrentLevel) m_pCurrentLevel->End_ShadowPass(cmdList);
}

_bool CLevel_Manager::IsSphereInShadowFrustum(const _float3& vCenter, _float fRadius)
{
    if (!m_bShadowCullingEnabled) return true;
    if (!m_pCurrentLevel)          return true;
    return m_pCurrentLevel->IsSphereInShadowFrustum(vCenter, fRadius);
}

void CLevel_Manager::Add_CullStat_Main(_bool b) { ++m_iCullTotal;   if (b) ++m_iCullRendered; }
void CLevel_Manager::Add_CullStat_Shadow(_bool b) { ++m_iShadowTotal; if (b) ++m_iShadowRendered; }

void CLevel_Manager::Reset_CullStats()
{
    m_iCullTotal = m_iCullRendered = 0;
    m_iShadowTotal = m_iShadowRendered = 0;
}

_bool CLevel_Manager::IsSphereInFrustum(const _float3& vCenter, _float fRadius)
{
    if (!m_bCullingEnabled)
        return true;  // OFF: 다 통과

    CCamera* pCamera = Get_CurrentCamera();
    if (nullptr == pCamera)
        return true;  // 카메라 없으면 안전하게 통과

    return pCamera->IsSphereInFrustum(vCenter, fRadius);
}

const DirectX::BoundingFrustum* CLevel_Manager::Get_CurrentFrustum()
{
    return m_pCurrentLevel ? m_pCurrentLevel->Get_CurrentFrustum() : nullptr;
}
const DirectX::BoundingSphere* CLevel_Manager::Get_ShadowBounds()
{
    return m_pCurrentLevel ? m_pCurrentLevel->Get_ShadowBounds() : nullptr;
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
