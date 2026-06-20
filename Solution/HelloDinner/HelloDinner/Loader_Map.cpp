#include "Loader_Map.h"
#include "GameInstance.h"
#include "Model.h"
#include "Map.h"
#include "Texture.h"

#include <fstream>
#include "json.hpp"
using json = nlohmann::json;

CLoader_Map::CLoader_Map(EngineContext* pContext)
    : m_pContext {pContext}
    , m_pGameInstance {CGameInstance::GetInstance()}
{
    Safe_AddRef(m_pGameInstance);
}

void CLoader_Map::FlushCommandList()
{
    m_pGameInstance->CloseCmdList();
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

    // [Palette 대응] 과거에는 머티리얼마다 albedo 가 1:1 로 존재했지만,
    //   현재는 다수 머티리얼이 하나의 Palette(아틀라스) 텍스처를 공유한다.
    //   => 파일명 기준 중복 제거를 하면 "Palette 를 두 번째 이후로 참조한"
    //      머티리얼들이 빈 경로가 되어 검정으로 렌더링된다.
    //   매핑 테이블(materialName -> 파일 경로)은 모든 머티리얼에 대해 채우고,
    //   실제 텍스처 중복 로딩 방지는 CTexture 캐시 단계에서 처리한다.
    for (auto& mat : matJson["materials"])
    {
        string matName = mat["materialName"].get<string>();
        string albedoFile = mat.contains("albedoTexture") ? mat["albedoTexture"].get<string>() : "";
        string normalFile = mat.contains("normalTexture") ? mat["normalTexture"].get<string>() : "";

        // 같은 materialName 이 중복되면(예: "Kitchen" 2개) 동일 경로로 덮어쓰므로 무해.
        MATERIAL_INFO& info = m_MaterialInfos[matName];

        if (!albedoFile.empty())
            info.strAlbedoFile = m_strTextureDir + _wstring(albedoFile.begin(), albedoFile.end());

        if (!normalFile.empty())
            info.strNormalFile = m_strTextureDir + _wstring(normalFile.begin(), normalFile.end());
    }
    return S_OK;
}

HRESULT CLoader_Map::Load_MapData(const string& strJsonPath, _uint iLevelIndex)
{
    ifstream file(strJsonPath);
    if (!file.is_open())
    {
        MSG_BOX("Failed to open MapData.json");
        return E_FAIL;
    }

    json mapJson;
    file >> mapJson;
    file.close();

    if (FAILED(m_pGameInstance->Add_Prototype(iLevelIndex,
        L"Prototype_GameObject_Map", CMap::Create(m_pContext))))
    {
        // 이미 등록되어 있을 수 있으므로 실패해도 계속 진행
    }

    // [Fracture] 부서지는 벽 인스턴스마다 부여할 안정적 ID(로드 순번 = 모든 클라 동일)
    _uint iFractureWallId = 0;

    for (auto& entry : mapJson["mapData"])
    {
        string fbxName = entry["fbxName"].get<string>();

        // [Anim 분기] JSON 의 isAnim 플래그로 ANIM/NONANIM 결정.
        //   (Unity 익스포터: SkinnedMeshRenderer=isAnim:true, MeshFilter=isAnim:false)
        //   더 이상 WallFence->Fractured_Wall 치환하지 않는다(Fractured_Wall 을 직접 배치).
        bool bAnim = entry.contains("isAnim") ? entry["isAnim"].get<bool>() : false;

        // [Fracture] 파괴 가능 벽 판별(현재는 모델 이름 기반). anim 이라고 모두 파괴 가능한 건
        //   아니므로 분리 판별. 파괴 모델이 늘면 이 조건만 확장(이름 집합/별도 플래그)하면 된다.
        string baseName = fbxName.substr(0, fbxName.find_last_of('.'));
        bool bBreakable = (_stricmp(baseName.c_str(), "Fractured_Wall") == 0);

        char szDbgLoad[256];
        sprintf_s(szDbgLoad, "[LoadMap] %s : %s, breakable=%d\n",
            fbxName.c_str(), (bAnim ? "ANIM" : "NONANIM"), bBreakable ? 1 : 0);
        OutputDebugStringA(szDbgLoad);

        _wstring strModelTag = Get_ModelTag(fbxName);

        // [Material 로딩] 머티리얼 이름 목록(검증은 안 하되 텍스처 로딩에는 필요)
        vector<string> materialNames;
        if (entry.contains("materialNames"))
            for (auto& texName : entry["materialNames"])
                materialNames.push_back(texName.get<string>());

        if (nullptr == m_pGameInstance->Clone_Prototype(
            Engine::PROTOTYPE::PROTO_COMPONENT, iLevelIndex, strModelTag, nullptr))
        {
            _wstring strBinaryPath = Get_BinaryPath(fbxName, bAnim);

            CModel::TYPE eType = bAnim ? CModel::TYPE_ANIM : CModel::TYPE_NONANIM;
            CModel* pModel = CModel::Create(m_pContext, eType, strBinaryPath.c_str(),
                XMMatrixIdentity(), CModel::MATLOAD_DDS_FILE);

            if (nullptr == pModel)
            {
                wstring wmsg = L"Failed to create CModel for: " + _wstring(fbxName.begin(), fbxName.end());
                MessageBox(NULL, wmsg.c_str(), L"System Message", MB_OK);
                continue;
            }

            // [Material 로딩] 검사(개수 검증)는 제거하되 텍스처 로딩은 유지한다.
            //   이 루프가 없으면 디퓨즈가 안 올라와 맵 전체가 완전 검정이 된다.
            //   (캐릭터는 MATLOAD_FROM_BINARY 라 영향 없음 / 맵만 외부 DDS 로딩 경로)
            const int iModelMatCount = (int)pModel->Get_NumMaterials();
            const int iMatCount = ((int)materialNames.size() < iModelMatCount)
                ? (int)materialNames.size() : iModelMatCount;

            int iLoadedTex = 0;
            for (int i = 0; i < iMatCount; ++i)
            {
                auto it = m_MaterialInfos.find(materialNames[i]);
                if (it == m_MaterialInfos.end())
                    continue;
                const auto& matInfo = it->second;
                if (!matInfo.strAlbedoFile.empty())
                {
                    pModel->Ready_MapMaterial(matInfo.strAlbedoFile.c_str(), i, TextureType_DIFFUSE);
                    ++iLoadedTex;
                }
                if (!matInfo.strNormalFile.empty())
                    pModel->Ready_MapMaterial(matInfo.strNormalFile.c_str(), i, TextureType_NORMALS);
            }
            // [진단] diffuseLoaded=0 이면 검정 원인(머티리얼 매칭 실패 / MaterialData 미로딩 등)
            {
                char szLog[256];
                sprintf_s(szLog, "[LoadMap] %s : matInfos=%zu, jsonMats=%zu, diffuseLoaded=%d\n",
                    fbxName.c_str(), m_MaterialInfos.size(), materialNames.size(), iLoadedTex);
                OutputDebugStringA(szLog);
            }

            if (FAILED(m_pGameInstance->Add_Prototype(iLevelIndex, strModelTag, pModel)))
            {
                m_PendingReleases.push_back(pModel);
                continue;
            }

            if (++m_iLoadCounter >= FLUSH_INTERVAL)
                FlushCommandList();
        }

        // 각 인스턴스마다 CMap Clone 생성
        for (auto& inst : entry["instances"])
        {
            CMap::MAP_DESC desc {};
            desc.strModelTag = strModelTag;
            desc.iModelLevelIndex = iLevelIndex;

            // [Fracture] 파괴 가능 벽 인스턴스만 파괴 가능 표시 + 안정적 ID 부여
            if (bBreakable)
            {
                desc.bBreakable = true;
                desc.iWallId = iFractureWallId++;
            }

            desc.vPosition.x = inst["position"]["x"].get<float>();
            desc.vPosition.y = inst["position"]["y"].get<float>() - 1.5f;
            desc.vPosition.z = inst["position"]["z"].get<float>();

            desc.vRotation.x = XMConvertToRadians(inst["rotation"]["x"].get<float>());
            desc.vRotation.y = XMConvertToRadians(inst["rotation"]["y"].get<float>() + 180);
            desc.vRotation.z = XMConvertToRadians(inst["rotation"]["z"].get<float>());

            desc.vScale.x = inst["scale"]["x"].get<float>();
            desc.vScale.y = inst["scale"]["y"].get<float>();
            desc.vScale.z = inst["scale"]["z"].get<float>();

            auto& colliderNode = inst["collider"];
            std::string strColliderType = colliderNode["colliderType"].get<std::string>();

            if (strColliderType == "AABB")
                desc.eColliderType = CCollider::TYPE_AABB;
            else if (strColliderType == "OBB")
                desc.eColliderType = CCollider::TYPE_OBB;
            else if (strColliderType == "SPHERE")
                desc.eColliderType = CCollider::TYPE_SPHERE;
            else
                desc.eColliderType = CCollider::TYPE_END;

            desc.vCenterCollider.x = colliderNode["center"]["x"].get<float>();
            desc.vCenterCollider.y = colliderNode["center"]["y"].get<float>() - 1.5f;
            desc.vCenterCollider.z = colliderNode["center"]["z"].get<float>();

            desc.vExtentsCollider.x = colliderNode["extents"]["x"].get<float>();
            desc.vExtentsCollider.y = colliderNode["extents"]["y"].get<float>();
            desc.vExtentsCollider.z = colliderNode["extents"]["z"].get<float>();

            desc.vRotationCollider.x = XMConvertToRadians(colliderNode["rotation"]["x"].get<float>());
            desc.vRotationCollider.y = XMConvertToRadians(colliderNode["rotation"]["y"].get<float>() + 180.f);
            desc.vRotationCollider.z = XMConvertToRadians(colliderNode["rotation"]["z"].get<float>());

            desc.fRadius = colliderNode["radius"].get<float>() * 100.f;

            if (FAILED(m_pGameInstance->Add_GameObject_ToLayer(
                iLevelIndex, L"Prototype_GameObject_Map",
                iLevelIndex, L"Layer_Map", &desc)))
            {
                MSG_BOX("Failed to add map instance to layer");
            }
        }
    }

    if (m_iLoadCounter > 0)
        FlushCommandList();

    return S_OK;
}

HRESULT CLoader_Map::Check_Fbx_Existence(const string& strJsonPath)
{
    ifstream file(strJsonPath);
    if (!file.is_open())
    {
        MSG_BOX("Failed to open MapData.json");
        return E_FAIL;
    }

    json mapJson;
    file >> mapJson;
    file.close();

    for (auto& entry : mapJson["mapData"])
    {
        string fbxName = entry["fbxName"].get<string>();
        bool bAnim = entry.contains("isAnim") ? entry["isAnim"].get<bool>() : false;
        _wstring strModelTag = Get_ModelTag(fbxName);
        _wstring strBinaryPath = Get_BinaryPath(fbxName, bAnim);

        ifstream fbxFile(strBinaryPath.c_str(), ios::binary);
        if (!fbxFile.is_open())
        {
            char szLog[512];
            sprintf_s(szLog, "[FBXFile] Name: %s not exist\n", fbxName.c_str());
            OutputDebugStringA(szLog);
        }
        fbxFile.close();
    }

    return E_NOTIMPL;
}

_wstring CLoader_Map::Get_BinaryPath(const string& strFbxName, bool /*bAnim*/)
{
    string name = strFbxName.substr(0, strFbxName.find_last_of('.'));
    // ANIM/NONANIM 모두 동일 폴더(사용자 확인). isAnim 은 로드 타입에만 영향, 경로엔 무영향.
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
    ReleasePendingResources();
    Safe_Release(m_pGameInstance);
    __super::Free();
}