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
#include "Pig_3rd.h"
#include "NetworkClient.h"
#include "Player_1rd.h"

CLevel_Loading::CLevel_Loading(EngineContext* pContext)
	: CLevel{pContext }
{
	
}

HRESULT CLevel_Loading::Initialize(LEVELID eNextLevelID)
{
	m_eNextLevelID = eNextLevelID;

	Add_Camera();

/*
	if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_Component_VIBuffer_VtxCube",
		CVIBuffer_Cube::Create(m_pContext))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_Component_VIBuffer_Skybox",
		CVIBuffer_Skybox::Create(m_pContext))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_Component_Texture_Cube",
		CTexture::Create(m_pContext, L"Resources/Textures/Rock.dds", 1))))
		return E_FAIL;
	
	if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_Component_Texture_Skybox",
		CTexture::Create(m_pContext, L"Resources/Textures/Skybox_Cube.dds", 1, TEX_CUBE))))
		return E_FAIL;

	Ready_TestLoader();
	
	*/


	// 1. MaterialData.json → 텍스처 Prototype 먼저 등록
	CLoader_Map* pMapLoader = CLoader_Map::Create(m_pContext);
	if (FAILED(pMapLoader->Load_MaterialData("Resources/Map/MaterialData.json", LEVEL_LOADING)))
	{
		MSG_BOX("Failed to load material data");
	}

	// 2. MapData.json → 모델 생성 + 텍스처 바인딩 + 맵 배치
	if (FAILED(pMapLoader->Load_MapData("Resources/Map/MapData.json", LEVEL_LOADING)))
	{
		MSG_BOX("Failed to load map data");
	}
	Safe_Release(pMapLoader);
	


	
	/*if (FAILED(m_pGameInstance->Add_Prototype(1, TEXT("Prototype_Component_Chick_3rd"),
		CModel::Create(m_pDevice, m_pContext, CModel::TYPE_NONANIM, L"Resources/NonAnim/chick/Prototype_Component_chicken.txt", _fmatrix()))))
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
	*/

	// 돼지 모델 로드
	_matrix PreTransformMatrix = XMMatrixIdentity() * XMMatrixScaling(0.01f, 0.01f, 0.01f);
	if (FAILED(m_pGameInstance->Add_Prototype(1, TEXT("Prototype_Component_Pig_3rd"),
		CModel::Create(m_pContext, CModel::TYPE_ANIM, L"Resources/Anim/Pig/Prototype_Component_Pig.txt", PreTransformMatrix))))
		return E_FAIL;

	// 애님모델 돼지 CPig_3rd
	/*{
		if (FAILED(m_pGameInstance->Add_Prototype(1, TEXT("Prototype_GameObject_Pig_3rd"),
			CPig_3rd::Create(m_pContext))))
			return E_FAIL;
		CPig_3rd::Player_3rd_DESC cdesc;
		cdesc.strModelTag = L"Prototype_Component_Pig_3rd";
		cdesc.iModelLevelIndex = 1;
		m_pGameInstance->Add_GameObject_ToLayer(1, TEXT("Prototype_GameObject_Pig_3rd"),
			1, TEXT("Layer_Player_3rd"), &cdesc);
	}*/

	// 애님모델 돼지 Player_1rd로 생성
	{
		if (FAILED(m_pGameInstance->Add_Prototype(1, TEXT("Prototype_GameObject_Player_1rd"),
			CPlayer_1rd::Create(m_pContext))))
			return E_FAIL;
		CPlayer_1rd::Player_1RD_DESC cdesc;
		cdesc.strModelTag = L"Prototype_Component_Pig_3rd";
		cdesc.iModelLevelIndex = 1;
		m_pGameInstance->Add_GameObject_ToLayer(1, TEXT("Prototype_GameObject_Player_1rd"),
			1, TEXT("Layer_Player_1rd"), &cdesc);
	}


	// m_pLoader = CLoader::Create(m_pDevice, m_pContext, m_eNextLevelID)

	return S_OK;
}

void CLevel_Loading::Update(_float fTimeDelta)
{
	__super::Update(fTimeDelta);

	// 네트워크 이벤트 처리: 플레이어 추가/제거/이동
	NetworkClient* pNetwork = NetworkClient::GetInstance();
	if (!pNetwork->IsConnected())
		return;

	std::vector<NetworkClient::NetEvent> events;
	pNetwork->PopAllEvents(events);

	for (auto& evt : events) {
		switch (evt.type) {
		case NetworkClient::NetEventType::PLAYER_ADD: {
			// 새 플레이어 접속 → CPig_3rd 생성
			//CPlayer_1rd::Player_1RD_DESC desc;
			//desc.strModelTag = L"Prototype_Component_Pig_3rd";
			//desc.iModelLevelIndex = 1;

			//// 레이어 이름에 id를 붙여 개별 관리
			//_wstring strLayerTag = L"Layer_Player_Pig_" + std::to_wstring(evt.id);

			//m_pGameInstance->Add_GameObject_ToLayer(1, TEXT("Prototype_GameObject_Pig_3rd"),
			//	1, strLayerTag, &desc);
			break;
		}
		case NetworkClient::NetEventType::PLAYER_REMOVE: {
			// 해당 id의 레이어에서 오브젝트를 찾아 Dead 처리
			_wstring strLayerTag = L"Layer_Player_Pig_" + std::to_wstring(evt.id);

			CGameObject* pObj = m_pGameInstance->Get_GameObject_To_Layer(1, strLayerTag, 0);
			if (pObj != nullptr)
				pObj->SetDead();
			break;
		}
		case NetworkClient::NetEventType::PLAYER_MOVE: {
			// TODO: 해당 id 플레이어의 Transform 갱신
			break;
		}
		}
	}
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
	tDesc.vAt = _float3{ 0.f, 0.f, -1.f };
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
	__super::Free();

}
