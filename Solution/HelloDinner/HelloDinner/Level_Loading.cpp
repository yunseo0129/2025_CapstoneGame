#include "stdafx.h"
#include "Level_Loading.h"
#include "Loader_Map.h"

#include "GameInstance.h"

#include "VIBuffer_Cube.h"
#include "VIBuffer_Skybox.h"
#include "Texture.h"
#include "Cube.h"
#include "Skybox.h"
#include "Renderer.h"
#include "Camera_FPV.h"
#include "Model.h"
#include "Chick_3rd.h"

CLevel_Loading::CLevel_Loading(ID3D12Device* pDevice, EngineContext* pContext)
	: CLevel{ pDevice, pContext }
	, m_pDevice{ pDevice }
	, m_pContext{ pContext }
	, m_pGameInstance{ CGameInstance::GetInstance() }
{
	
}

HRESULT CLevel_Loading::Initialize(LEVELID eNextLevelID)
{
	m_eNextLevelID = eNextLevelID;


	if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_Component_VIBuffer_VtxCube",
		CVIBuffer_Cube::Create(m_pDevice, m_pContext->cmdList))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_Component_VIBuffer_Skybox",
		CVIBuffer_Skybox::Create(m_pDevice, m_pContext->cmdList))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_Component_Texture_Cube",
		CTexture::Create(m_pDevice, m_pContext->cmdList, L"Resources/Textures/Rock.dds", 1))))
		return E_FAIL;
	
	if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_Component_Texture_Skybox",
		CTexture::Create(m_pDevice, m_pContext->cmdList, L"Resources/Textures/Skybox_Cube.dds", 1, TEX_CUBE))))
		return E_FAIL;

	Add_Camera();

	/*Ready_TestLoader();

	CLoader_Map* pMapLoader = CLoader_Map::Create(m_pDevice, m_pContext);
	
	if (FAILED(pMapLoader->Load_MapData("Resources/Map/MapData.json", LEVEL_LOADING)))
	{
		MSG_BOX("Failed to load map data");
	}*/


	
	if (FAILED(m_pGameInstance->Add_Prototype(1, TEXT("Prototype_Component_Chick_3rd"),
		CModel::Create(m_pDevice, m_pContext, CModel::TYPE_NONANIM, L"Resources/chick/Prototype_Component_chick.txt", _fmatrix()))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_Prototype(1, TEXT("Prototype_GameObject_Chick_3rd"),
		CChick_3rd::Create(m_pContext))))
		return E_FAIL;

	CChick_3rd::Player_3rd_DESC desc;
	desc.strModelTag = L"Prototype_Component_Chick_3rd";
	desc.iModelLevelIndex = 1;
	desc.vScale = _float3(0.1f, 0.1f, 0.1f);
	m_pGameInstance->Add_GameObject_ToLayer(1, TEXT("Prototype_GameObject_Chick_3rd"),
		1, TEXT("Layer_Player_chick"), &desc);


	//Safe_Release(pMapLoader);

	// m_pLoader = CLoader::Create(m_pDevice, m_pContext, m_eNextLevelID)

	return S_OK;
}

void CLevel_Loading::Update(_float fTimeDelta)
{
	__super::Update(fTimeDelta);
}

HRESULT CLevel_Loading::Render()
{

#ifdef _DEBUG
	// m_pLoader->Show_Debug();
#endif

	return S_OK;
}

void CLevel_Loading::Add_Camera()
{
	CCamera_FPV::FPV_CAMERA_DESC tDesc;
	tDesc.vEye = _float3{ 0.f, 0.f, -5.f };
	tDesc.vAt = _float3{ 0.f, 0.f, 0.f };
	tDesc.fFovy = XMConvertToRadians(60.f);
	tDesc.fAspect = 1280.f / 720.f;
	tDesc.fNear = 0.1f;
	tDesc.fFar = 10000.f;
	tDesc.fCamMouseSensor = 1.f;
	tDesc.fCamSpeedPerSec = 2.f;
	tDesc.fRotationPerSec = 1.f;
	tDesc.fSpeedPerSec = 100.f;

	CCamera_FPV* pCamera = CCamera_FPV::Create(m_pContext);
	pCamera->Initialize(&tDesc);

	m_pCamera[CAMERA_FPV] = pCamera;
}

HRESULT CLevel_Loading::Ready_TestLoader()
{
	// 큐브 하나 그려보기
	CCube* pCube = CCube::Create(m_pContext);

	
	if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_GameObject_Cube", pCube)))
		return E_FAIL;
	if (FAILED(m_pGameInstance->Add_GameObject_ToLayer(LEVEL_LOADING, L"Prototype_GameObject_Cube", LEVEL_LOADING, L"Layer_Test", nullptr))) {
		MSG_BOX("Failed to Add GameObject To Layer : Cube");
		return E_FAIL;
	}
	
	
	if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_GameObject_Skybox", CSkybox::Create(m_pContext))))
		return E_FAIL;

	
	if (FAILED(m_pGameInstance->Add_GameObject_ToLayer(LEVEL_LOADING, L"Prototype_GameObject_Skybox", LEVEL_LOADING, L"Layer_Skybox", nullptr))) {
		MSG_BOX("Failed to Add GameObject To Layer : Skybox");
		return E_FAIL;
	}
	
	
	
	return S_OK;
}

CLevel_Loading* CLevel_Loading::Create(ID3D12Device* pDevice, EngineContext* pContext, LEVELID eNextLevelID)
{
	CLevel_Loading* pInstance = new CLevel_Loading(pDevice, pContext);

	if (FAILED(pInstance->Initialize(eNextLevelID)))
	{
		MSG_BOX("Failed to Created : CLevel_Loading");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CLevel_Loading::Free()
{
	__super::Free();

}
