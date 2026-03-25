#include "Loader_Map.h"
#include "GameInstance.h"
#include "Model.h"
#include "Map.h"

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

		// 3-1. 해당 fbxName의 CModel 프로토타입이 아직 없으면 등록
		if (nullptr == m_pGameInstance->Clone_Prototype(
			Engine::PROTOTYPE::PROTO_COMPONENT, iLevelIndex, strModelTag, nullptr))
		{
			// Clone 실패 = 프로토타입이 없다는 뜻 → 새로 등록
			_wstring strBinaryPath = Get_BinaryPath(fbxName);

			CModel* pModel = CModel::Create(m_pDevice, m_pContext,
				CModel::TYPE_NONANIM, strBinaryPath.c_str(), XMMatrixIdentity());

			if (nullptr == pModel)
			{
				wstring wmsg = L"Failed to create CModel for: " + _wstring(fbxName.begin(), fbxName.end());
				MessageBox(NULL, wmsg.c_str(), L"System Message", MB_OK);
				continue;
			}

			if (FAILED(m_pGameInstance->Add_Prototype(iLevelIndex, strModelTag, pModel)))
			{
				Safe_Release(pModel);
				continue;
			}
		}

		// 3-2. 각 인스턴스마다 CMap Clone 생성
		for (auto& inst : entry["instances"])
		{
			CMap::MAP_DESC desc{};
			desc.strModelTag = strModelTag;
			desc.iModelLevelIndex = iLevelIndex;

			desc.vPosition.x = inst["position"]["x"].get<float>();
			desc.vPosition.y = inst["position"]["y"].get<float>();
			desc.vPosition.z = inst["position"]["z"].get<float>();

			desc.vRotation.x = inst["rotation"]["x"].get<float>();
			desc.vRotation.y = inst["rotation"]["y"].get<float>();
			desc.vRotation.z = inst["rotation"]["z"].get<float>();

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

CLoader_Map* CLoader_Map::Create(ID3D12Device* pDevice, EngineContext* pContext)
{
	CLoader_Map* pInstance = new CLoader_Map(pDevice, pContext);
	return pInstance;
}

void CLoader_Map::Free()
{
	__super::Free();
	Safe_Release(m_pGameInstance);
}