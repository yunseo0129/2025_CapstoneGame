#include "Loader_Map.h"
#include "GameInstance.h"
#include "Model.h"
#include "Map.h"
#include "Texture.h"

#include <unordered_set>
#include <fstream>
#include "json.hpp"
using json = nlohmann::json;

CLoader_Map::CLoader_Map(EngineContext* pContext)
	: m_pContext{ pContext }
	, m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);
}

void CLoader_Map::FlushCommandList()
{
	// 현재 커맨드 리스트를 실행 → GPU 동기화 → 리셋
	m_pGameInstance->CloseCmdList();
	// GPU 완료 후에야 대기 중인 리소스를 안전하게 삭제
	ReleasePendingResources();
	m_pGameInstance->ResetCmdList();
	m_iLoadCounter = 0;
}

void CLoader_Map::ReleasePendingResources()
{
	for (auto& pResource : m_PendingReleases)
		Safe_Release(pResource);

	m_PendingReleases.clear();
}

HRESULT CLoader_Map::Load_MaterialData(const string& strJsonPath)
{
	ifstream file(strJsonPath);
	if (!file.is_open())
	{
		MSG_BOX("Failed to open MaterialData.json");
		return E_FAIL;
	}

	json matJson;
	file >> matJson;
	file.close();

	// 중복 방지용 set (같은 PNG를 여러 머티리얼이 공유할 수 있음)
	unordered_set<string> registeredTextures;

	for (auto& mat : matJson["materials"])
	{
		string matName = mat["materialName"].get<string>();
		string albedoFile = mat["albedoTexture"].get<string>();
		string normalFile = mat["normalTexture"].get<string>();

		// ---- Albedo 텍스처 등록 ----
		if (!albedoFile.empty()) // 파일명이 비어있지 않으면
		{
			if (registeredTextures.find(albedoFile) == registeredTextures.end()) // 중복이 아니면
			{
				_wstring strPath = L"Resources/NonAnim/Map/dds/" + _wstring(albedoFile.begin(), albedoFile.end());	//Ex) "albedo.dds" → L"Resources/Map/dds/albedo.dds"
				registeredTextures.insert(albedoFile);
				m_MaterialInfos[matName].strAlbedoFile = strPath; // 머티리얼 이름 → 알베도 파일명 저장
			}
		}

		// ---- Normal 텍스처 등록 ----
		if (!normalFile.empty()) // 파일명이 비어있지 않으면
		{
			if (registeredTextures.find(normalFile) == registeredTextures.end()) // 중복이 아니면
			{
				_wstring strPath = L"Resources/NonAnim/Map/dds/" + _wstring(normalFile.begin(), normalFile.end());
				registeredTextures.insert(normalFile);
				m_MaterialInfos[matName].strNormalFile = strPath; // 머티리얼 이름 → 노말 파일명 저장
			}
		}
	}
	return S_OK;
}

HRESULT CLoader_Map::Load_MapData(const string& strJsonPath, _uint iLevelIndex)
{
	// JSON 파일 열기
	ifstream file(strJsonPath);
	if (!file.is_open())
	{
		MSG_BOX("Failed to open MapData.json");
		return E_FAIL;
	}

	json mapJson;
	file >> mapJson;
	file.close();

	// CMap 게임오브젝트 프로토타입 등록 (한 번만)
	if (FAILED(m_pGameInstance->Add_Prototype(iLevelIndex,
		L"Prototype_GameObject_Map", CMap::Create(m_pContext))))
	{
		// 이미 등록되어 있을 수 있으므로 실패해도 계속 진행
	}

	// mapData 배열 순회
	for (auto& entry : mapJson["mapData"])
	{
		string fbxName = entry["fbxName"].get<string>();
		_wstring strModelTag = Get_ModelTag(fbxName);

		// materialNames 배열 읽기 (유니티에서 뽑은 MatIdx별 텍스처 파일명)
		vector<string> materialNames;
		if (entry.contains("materialNames"))
		{
			for (auto& texName : entry["materialNames"])
			{
				materialNames.push_back(texName.get<string>());
			}
		}

		// 해당 fbxName의 CModel 프로토타입이 아직 없으면 등록
		if (nullptr == m_pGameInstance->Clone_Prototype(
			Engine::PROTOTYPE::PROTO_COMPONENT, iLevelIndex, strModelTag, nullptr))
		{
			// Clone 실패 = 프로토타입이 없다는 뜻 → 새로 등록
			_wstring strBinaryPath = Get_BinaryPath(fbxName);

			CModel* pModel = CModel::Create(m_pContext,
				CModel::TYPE_NONANIM, strBinaryPath.c_str(), XMMatrixIdentity(), CModel::MATLOAD_DDS_FILE);

			if (nullptr == pModel)
			{
				wstring wmsg = L"Failed to create CModel for: " + _wstring(fbxName.begin(), fbxName.end());
				MessageBox(NULL, wmsg.c_str(), L"System Message", MB_OK);
				continue;
			}

			for (int i = 0; i < materialNames.size(); ++i)
			{
				const auto& matName = materialNames[i];
				if (m_MaterialInfos.find(matName) != m_MaterialInfos.end())
				{
					const auto& matInfo = m_MaterialInfos[matName];
					if (!matInfo.strAlbedoFile.empty())
					{
						// 다른 모델이 TextureType_DIFFUSE로 읽어왔을 경우 맵 모델도 수정 필요
						pModel->Ready_MapMaterial(matInfo.strAlbedoFile.c_str(), i, TextureType_DIFFUSE);
					}
					if (!matInfo.strNormalFile.empty())
					{
						pModel->Ready_MapMaterial(matInfo.strNormalFile.c_str(),i, TextureType_NORMALS);
					}
				}
			}

			if (FAILED(m_pGameInstance->Add_Prototype(iLevelIndex, strModelTag, pModel)))
			{
				// 모델도 즉시 삭제하지 않고 보류
				m_PendingReleases.push_back(pModel);
				continue;
			}

			// 메쉬 버퍼 업로드 후 배치 플러시 (수정 필요)
			if (++m_iLoadCounter >= FLUSH_INTERVAL)
				FlushCommandList();
		}

		// 각 인스턴스마다 CMap Clone 생성
		for (auto& inst : entry["instances"])
		{
			CMap::MAP_DESC desc{};
			desc.strModelTag = strModelTag;
			desc.iModelLevelIndex = iLevelIndex;

			// 이동
			desc.vPosition.x = inst["position"]["x"].get<float>()*100.f - 6500.f;
			desc.vPosition.y = inst["position"]["y"].get<float>()*100.f - 150.f;
			desc.vPosition.z = inst["position"]["z"].get<float>()*100.f - 10500.f;

			// 회전
			desc.vRotation.x = XMConvertToRadians(inst["rotation"]["x"].get<float>());
			desc.vRotation.y = XMConvertToRadians(inst["rotation"]["y"].get<float>() + 180);
			desc.vRotation.z = XMConvertToRadians(inst["rotation"]["z"].get<float>());

			// 스케일
			desc.vScale.x = inst["scale"]["x"].get<float>()*100.f;
			desc.vScale.y = inst["scale"]["y"].get<float>()*100.f;
			desc.vScale.z = inst["scale"]["z"].get<float>()*100.f;

			// Collider 정보
			auto& colliderNode = inst["collider"];
			std::string strColliderType = colliderNode["colliderType"].get<std::string>();

			// 1. 충돌체 타입
			if (strColliderType == "AABB")
				desc.eColliderType = CCollider::TYPE_AABB;
			else if (strColliderType == "OBB")
				desc.eColliderType = CCollider::TYPE_OBB;
			else if (strColliderType == "SPHERE")
				desc.eColliderType = CCollider::TYPE_SPHERE;
			else
				desc.eColliderType = CCollider::TYPE_END; // "NONE" 이거나 알 수 없는 타입

			// 2. 중심 좌표 (position이랑 정확하게 맞춰야됨)
			desc.vCenterCollider.x = colliderNode["center"]["x"].get<float>() * 100.f - 6500.f;
			desc.vCenterCollider.y = colliderNode["center"]["y"].get<float>() * 100.f - 150.f;
			desc.vCenterCollider.z = colliderNode["center"]["z"].get<float>() * 100.f - 10500.f;

			// 3. 사이즈
			desc.vExtentsCollider.x = colliderNode["extents"]["x"].get<float>() * 100.f;
			desc.vExtentsCollider.y = colliderNode["extents"]["y"].get<float>() * 100.f;
			desc.vExtentsCollider.z = colliderNode["extents"]["z"].get<float>() * 100.f;

			// 4. 회전 (메쉬랑 맞춰야됨)
			desc.vRotationCollider.x = XMConvertToRadians(colliderNode["rotation"]["x"].get<float>());
			desc.vRotationCollider.y = XMConvertToRadians(colliderNode["rotation"]["y"].get<float>() + 180.f);
			desc.vRotationCollider.z = XMConvertToRadians(colliderNode["rotation"]["z"].get<float>());

			// 5. 반지름 (구형 충돌체용)
			desc.fRadius = colliderNode["radius"].get<float>() * 100.f;

			if (FAILED(m_pGameInstance->Add_GameObject_ToLayer(
				iLevelIndex, L"Prototype_GameObject_Map",
				iLevelIndex, L"Layer_Map", &desc)))
			{
				MSG_BOX("Failed to add map instance to layer");
			}
		}
	}

	// 남은 커맨드 플러시 (수정필요)
	if (m_iLoadCounter > 0)
		FlushCommandList();
	//

	return S_OK;
}

HRESULT CLoader_Map::Check_Fbx_Existence ( const string& strJsonPath )
{
	// 1. JSON 파일 열기
	ifstream file ( strJsonPath );
	if ( !file.is_open () )
	{
		MSG_BOX ( "Failed to open MapData.json" );
		return E_FAIL;
	}

	json mapJson;
	file >> mapJson;
	file.close ();


	// 3. mapData 배열 순회
	for ( auto& entry : mapJson["mapData"] )
	{
		string fbxName = entry["fbxName"].get<string> ();
		_wstring strModelTag = Get_ModelTag ( fbxName );

		_wstring strBinaryPath = Get_BinaryPath ( fbxName );

		ifstream fbxFile ( strBinaryPath.c_str () , ios::binary );
		if ( !fbxFile.is_open () )
		{
			char szLog[512];
			sprintf_s ( szLog ,"[FBXFile] Name: %s not exist\n" , fbxName.c_str () );
			OutputDebugStringA ( szLog );
		}
		fbxFile.close ();
	}

	return E_NOTIMPL;
}

_wstring CLoader_Map::Get_BinaryPath(const string& strFbxName)
{
	// FBX 파일명에서 확장자 제거 후 바이너리 경로 생성
	string name = strFbxName.substr(0, strFbxName.find_last_of('.'));
	string path = "Resources/NonAnim/Map/fbx/Prototype_Component_" + name + ".txt";

	return _wstring(path.begin(), path.end());
}

_wstring CLoader_Map::Get_ModelTag(const string& strFbxName)
{
	string name = strFbxName.substr(0, strFbxName.find_last_of('.'));
	string tag = "Prototype_Component_Model_" + name;

	return _wstring(tag.begin(), tag.end());
}

CLoader_Map* CLoader_Map::Create(EngineContext* pContext)
{
	CLoader_Map* pInstance = new CLoader_Map(pContext);
	return pInstance;
}

void CLoader_Map::Free()
{
	// 혹시 남아있을 수 있는 보류 리소스 정리
	ReleasePendingResources();
	Safe_Release(m_pGameInstance);
	__super::Free();

}