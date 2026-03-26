#include "Loader_Map.h"
#include "GameInstance.h"
#include "Model.h"
#include "Map.h"
#include "Texture.h"

#include <unordered_set>
#include <fstream>
#include "json.hpp"
using json = nlohmann::json;

CLoader_Map::CLoader_Map(ID3D12Device* pDevice, EngineContext* pContext)
	: m_pDevice{ pDevice }
	, m_pContext{ pContext }
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


HRESULT CLoader_Map::Load_MaterialData(const string& strJsonPath, _uint iLevelIndex)
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
		if (!albedoFile.empty())
		{
			_wstring strAlbedoTag = Get_TextureTag(albedoFile);
			m_mapMaterialToAlbedoTag[matName] = strAlbedoTag;

			if (registeredTextures.find(albedoFile) == registeredTextures.end())
			{
				_wstring strPath = L"Resources/Map/dds/" + _wstring(albedoFile.begin(), albedoFile.end());

				CTexture* pTexture = CTexture::Create(m_pDevice, m_pContext->cmdList, strPath.c_str());
				if (pTexture != nullptr)
				{
					if (FAILED(m_pGameInstance->Add_Prototype(iLevelIndex, strAlbedoTag, pTexture)))
					{
						// 즉시 삭제하지 않고, 플러시 후까지 보류
						m_PendingReleases.push_back(pTexture);
						
						// Safe_Release(pTexture);
					}
					else
					{
						registeredTextures.insert(albedoFile);
					}
				}
				// 배치 플러시(수정 필요)
				if (++m_iLoadCounter >= FLUSH_INTERVAL)
					FlushCommandList();
			}
		}

		// ---- Normal 텍스처 등록 ----
		if (!normalFile.empty())
		{
			_wstring strNormalTag = Get_TextureTag(normalFile);
			m_mapMaterialToNormalTag[matName] = strNormalTag;

			if (registeredTextures.find(normalFile) == registeredTextures.end())
			{
				_wstring strPath = L"Resources/Map/dds/" + _wstring(normalFile.begin(), normalFile.end());

				CTexture* pTexture = CTexture::Create(m_pDevice, m_pContext->cmdList, strPath.c_str());
				if (pTexture != nullptr)
				{
					if (FAILED(m_pGameInstance->Add_Prototype(iLevelIndex, strNormalTag, pTexture)))
					{
						// 즉시 삭제하지 않고, 플러시 후까지 보류
						m_PendingReleases.push_back(pTexture);

						// Safe_Release(pTexture);
					}
					else
					{
						registeredTextures.insert(normalFile);
					}
				}
				// 배치 플러시(수정 필요)
				if (++m_iLoadCounter >= FLUSH_INTERVAL)
					FlushCommandList();
			}
		}
	}
	// 남은 커맨드 플러시(수정 필요)
	if (m_iLoadCounter > 0)
		FlushCommandList();
	//

	char szLog[512];
	sprintf_s(szLog, "[MaterialData] Registered %zu textures, %zu materials\n",
		registeredTextures.size(), m_mapMaterialToAlbedoTag.size());
	OutputDebugStringA(szLog);

	return S_OK;
}

HRESULT CLoader_Map::Load_MapData(const string& strJsonPath, _uint iLevelIndex)
{
	// 1. JSON 파일 열기
	ifstream file(strJsonPath);
	if (!file.is_open())
	{
		MSG_BOX("Failed to open MapData.json");
		return E_FAIL;
	}

	json mapJson;
	file >> mapJson;
	file.close();

	// 2. CMap 게임오브젝트 프로토타입 등록 (한 번만)
	if (FAILED(m_pGameInstance->Add_Prototype(iLevelIndex,
		L"Prototype_GameObject_Map", CMap::Create(m_pContext))))
	{
		// 이미 등록되어 있을 수 있으므로 실패해도 계속 진행
	}

	// 3. mapData 배열 순회
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

		// 3-1. 해당 fbxName의 CModel 프로토타입이 아직 없으면 등록
		if (nullptr == m_pGameInstance->Clone_Prototype(
			Engine::PROTOTYPE::PROTO_COMPONENT, iLevelIndex, strModelTag, nullptr))
		{
			// Clone 실패 = 프로토타입이 없다는 뜻 → 새로 등록
			_wstring strBinaryPath = Get_BinaryPath(fbxName);

			CModel* pModel = CModel::Create(m_pDevice, m_pContext,
				CModel::TYPE_NONANIM, strBinaryPath.c_str(), XMMatrixIdentity(), CModel::MATLOAD_SKIP_TEXTURE);

			if (nullptr == pModel)
			{
				wstring wmsg = L"Failed to create CModel for: " + _wstring(fbxName.begin(), fbxName.end());
				MessageBox(NULL, wmsg.c_str(), L"System Message", MB_OK);
				continue;
			}

			// textureNames 배열을 이용하여 각 MatIdx에 텍스처 설정
			char szDebug[512];
			sprintf_s(szDebug, "[LoadMap] Model has %u materials, textureNames has %zu entries\n",
				pModel->Get_NumMaterials(), materialNames.size());
			OutputDebugStringA(szDebug);

			for (_uint matIdx = 0; matIdx < (_uint)materialNames.size(); matIdx++)
			{
				const string& matName = materialNames[matIdx];

				// ---- Albedo 텍스처 바인딩 ----
				auto itAlbedo = m_mapMaterialToAlbedoTag.find(matName);
				if (itAlbedo != m_mapMaterialToAlbedoTag.end())
				{
					const _wstring& strAlbedoTag = itAlbedo->second;

					CTexture* pAlbedo = static_cast<CTexture*>(
						m_pGameInstance->Clone_Prototype(
							Engine::PROTOTYPE::PROTO_COMPONENT, iLevelIndex, strAlbedoTag, nullptr));

					sprintf_s(szDebug, "[LoadMap] matIdx=%u, matName=%s, albedoTag=%ls, pAlbedo=0x%p\n",
						matIdx, matName.c_str(), strAlbedoTag.c_str(), pAlbedo);
					OutputDebugStringA(szDebug);

					if (pAlbedo != nullptr)
					{
						HRESULT hr = pModel->Set_MaterialTexture(matIdx, (TextureType)TextureType_DIFFUSE, pAlbedo);
						sprintf_s(szDebug, "[LoadMap] Set_MaterialTexture(Albedo) result: 0x%08X\n", hr);
						OutputDebugStringA(szDebug);
					}
				}

				// ---- Normal 텍스처 바인딩 ----
				auto itNormal = m_mapMaterialToNormalTag.find(matName);
				if (itNormal != m_mapMaterialToNormalTag.end())
				{
					const _wstring& strNormalTag = itNormal->second;

					CTexture* pNormal = static_cast<CTexture*>(
						m_pGameInstance->Clone_Prototype(
							Engine::PROTOTYPE::PROTO_COMPONENT, iLevelIndex, strNormalTag, nullptr));

					sprintf_s(szDebug, "[LoadMap] matIdx=%u, matName=%s, normalTag=%ls, pNormal=0x%p\n",
						matIdx, matName.c_str(), strNormalTag.c_str(), pNormal);
					OutputDebugStringA(szDebug);

					if (pNormal != nullptr)
					{
						HRESULT hr = pModel->Set_MaterialTexture(matIdx, (TextureType)TextureType_NORMALS, pNormal);
						sprintf_s(szDebug, "[LoadMap] Set_MaterialTexture(Normal) result: 0x%08X\n", hr);
						OutputDebugStringA(szDebug);
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

		// 3-2. 각 인스턴스마다 CMap Clone 생성
		for (auto& inst : entry["instances"])
		{
			CMap::MAP_DESC desc{};
			desc.strModelTag = strModelTag;
			desc.iModelLevelIndex = iLevelIndex;

			desc.vPosition.x = inst["position"]["x"].get<float>() * 100.0f;
			desc.vPosition.y = inst["position"]["y"].get<float>() * 100.0f;
			desc.vPosition.z = inst["position"]["z"].get<float>() * 100.0f;
			
			
			desc.vRotation.x = XMConvertToRadians(inst["rotation"]["x"].get<float>());
			desc.vRotation.y = XMConvertToRadians(inst["rotation"]["y"].get<float>());
			desc.vRotation.z = XMConvertToRadians(inst["rotation"]["z"].get<float>());
			

			desc.vScale.x = inst["scale"]["x"].get<float>();
			desc.vScale.y = inst["scale"]["y"].get<float>();
			desc.vScale.z = inst["scale"]["z"].get<float>();

			if (FAILED(m_pGameInstance->Add_GameObject_ToLayer(
				iLevelIndex, L"Prototype_GameObject_Map",
				iLevelIndex, L"Layer_Test", &desc)))
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

_wstring CLoader_Map::Get_BinaryPath(const string& strFbxName)
{
	// FBX 파일명에서 확장자 제거 후 바이너리 경로 생성
	string name = strFbxName.substr(0, strFbxName.find_last_of('.'));
	string path = "Resources/Map/fbx/Prototype_Component_" + name + ".txt";

	return _wstring(path.begin(), path.end());
}

_wstring CLoader_Map::Get_ModelTag(const string& strFbxName)
{
	string name = strFbxName.substr(0, strFbxName.find_last_of('.'));
	string tag = "Prototype_Component_Model_" + name;

	return _wstring(tag.begin(), tag.end());
}

_wstring CLoader_Map::Get_TextureTag(const string& strPngFileName)
{
	string name = strPngFileName.substr(0, strPngFileName.find_last_of('.'));
	string tag = "Prototype_Component_Texture_" + name;
	return _wstring(tag.begin(), tag.end());
}

CLoader_Map* CLoader_Map::Create(ID3D12Device* pDevice, EngineContext* pContext)
{
	CLoader_Map* pInstance = new CLoader_Map(pDevice, pContext);
	return pInstance;
}

void CLoader_Map::Free()
{
	__super::Free();

	// 혹시 남아있을 수 있는 보류 리소스 정리
	ReleasePendingResources();

	Safe_Release(m_pGameInstance);
}