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

    // [UV] 새 MaterialData 구조:
    //   - 머티리얼이 슬롯(서브메시) 단위로 분리되어 각자 고유 텍스처(textureKey)를 가짐.
    //   - 키는 더 이상 Unity 머티리얼 이름이 아니라 "textureKey"({fbx}_mat{slot}_Texture).
    //     MapData 의 materialNames[i] 가 이 키를 직접 가리킨다.
    //   - albedo 가 슬롯마다 고유하므로 파일명 중복 제거(dedup)를 하지 않는다.
    //     (과거 Palette 공유 시절의 registeredTextures 로직은 제거)
    //   - uvOffset/uvScale 을 함께 읽어 셰이더 UV 재매핑에 사용한다.
    for (auto& mat : matJson["materials"])
    {
        // textureKey 우선, 구버전 호환을 위해 없으면 materialName 으로 폴백.
        string key;
        if (mat.contains("textureKey"))      key = mat["textureKey"].get<string>();
        else if (mat.contains("materialName")) key = mat["materialName"].get<string>();
        else continue;

        string albedoFile = mat.contains("albedoTexture") ? mat["albedoTexture"].get<string>() : "";
        string normalFile = mat.contains("normalTexture") ? mat["normalTexture"].get<string>() : "";

        MATERIAL_INFO& info = m_MaterialInfos[key];

        // [팔레트 방식] 어느 공유 팔레트를 쓸지 고르기 위한 Unity 머티리얼 이름 저장.
        //   'Paintings' → Paintings 팔레트, 그 외 전부 → 공용 Palette.
        if (mat.contains("materialName"))
            info.strMaterialName = mat["materialName"].get<string>();

        if (!albedoFile.empty())
            info.strAlbedoFile = m_strTextureDir + _wstring(albedoFile.begin(), albedoFile.end());
        if (!normalFile.empty())
            info.strNormalFile = m_strTextureDir + _wstring(normalFile.begin(), normalFile.end());

        // UV 재매핑 파라미터(없으면 기본값 유지: 변환 없음).
        if (mat.contains("uvOffsetX")) info.vUVOffset.x = mat["uvOffsetX"].get<float>();
        if (mat.contains("uvOffsetY")) info.vUVOffset.y = mat["uvOffsetY"].get<float>();
        if (mat.contains("uvScaleX"))  info.vUVScale.x = mat["uvScaleX"].get<float>();
        if (mat.contains("uvScaleY"))  info.vUVScale.y = mat["uvScaleY"].get<float>();

        // [투명] 표면타입/알파 (없으면 기본 Opaque/1.0 → 기존 맵 JSON 그대로 동작).
        if (mat.contains("surfaceType"))
        {
            string st = mat["surfaceType"].get<string>();
            if (st == "Transparent")   info.iSurfaceType = 1;
            else if (st == "Cutout")   info.iSurfaceType = 2;
            else                       info.iSurfaceType = 0; // Opaque
        }
        if (mat.contains("alpha")) info.fAlpha = mat["alpha"].get<float>();
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
        bool bBreakable = (_stricmp(baseName.c_str(), "Brread") == 0);

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

            // [팔레트 방식] 모델의 "모든" 슬롯에 공유 팔레트를 바인딩한다.
            //   • 핵심: min(jsonMats, modelMats) 가 아니라 modelMatCount 전체를 순회.
            //     변환기(Assimp)가 만든 슬롯 수(예: Freezer=5)가 Unity 익스포트 슬롯 수
            //     (예: 3)보다 많아도, 빠짐없이 팔레트가 발려 검정 슬롯이 없어진다.
            //   • 메시 정점 UV 가 팔레트의 올바른 색을 직접 가리키므로, 슬롯 순서/개수가
            //     익스포트와 어긋나도 색이 정확하다(슬롯별 crop/UV 재매핑 불필요).
            //   • 'Paintings' 머티리얼 슬롯만 Paintings 팔레트, 그 외는 공용 Palette.
            //   • DIFFUSE 만 바인딩(이 에셋군은 normal/AO/metallic 맵이 없음).
            //   (캐릭터/총은 MATLOAD_FROM_BINARY 라 이 경로를 타지 않음 → 영향 0)
            const int iModelMatCount = (int)pModel->Get_NumMaterials();

            int iLoadedTex = 0;
            int iBlendSlots = 0;   // [투명] 진단용: Transparent 슬롯 수
            for (int i = 0; i < iModelMatCount; ++i)
            {
                // 이 슬롯의 JSON 머티리얼 정보(있으면 한 번만 룩업). 슬롯 i 정보가 없으면
                // (익스포트 슬롯 수 < 모델 슬롯 수) 기본값(Palette/Opaque/1.0) 사용.
                const MATERIAL_INFO* pInfo = nullptr;
                if (i < (int)materialNames.size())
                {
                    auto it = m_MaterialInfos.find(materialNames[i]);
                    if (it != m_MaterialInfos.end())
                        pInfo = &it->second;
                }

                // 팔레트 선택: 'Paintings' 만 별도, 그 외 공용 Palette.
                const _wstring* pPalette =
                    (pInfo && pInfo->strMaterialName == "Paintings")
                    ? &m_strPalettePaintings : &m_strPaletteDefault;

                if (SUCCEEDED(pModel->Ready_MapMaterial(pPalette->c_str(), i, TextureType_DIFFUSE)))
                    ++iLoadedTex;

                // [레거시] UV 재매핑 미사용 → 기본값(변환 없음) 명시.
                pModel->Set_MapMaterialUV(i, _float2(0.f, 0.f), _float2(1.f, 1.f));

                // [투명] 표면타입/알파 전달(슬롯 정보 없으면 Opaque/1.0).
                _int   iSurface = pInfo ? pInfo->iSurfaceType : 0;
                _float fAlpha = pInfo ? pInfo->fAlpha : 1.f;

                // [투명] Transparent 인데 알파가 사실상 1.0 이면(예: Kitchen_alpha) 블렌딩할
                //   게 없고(공유 팔레트 불투명), 블렌드 패스 Z-Write Off 로 정렬 아티팩트만
                //   생긴다 → 불투명으로 격하해 불투명 패스에서 솔리드로 그린다.
                if (iSurface == 1 && fAlpha >= 0.99f)
                    iSurface = 0;

                pModel->Set_MapMaterialBlend(i, iSurface, fAlpha);
                if (iSurface == 1) ++iBlendSlots;
            }
            // [진단] paletteBound 가 modelMats 와 같아야 정상(모든 슬롯에 팔레트 바인딩됨).
            {
                char szLog[256];
                sprintf_s(szLog, "[LoadMap] %s : modelMats=%d, jsonMats=%zu, paletteBound=%d, blendSlots=%d\n",
                    fbxName.c_str(), iModelMatCount, materialNames.size(), iLoadedTex, iBlendSlots);
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

            desc.vPosition.x = inst["position"]["x"].get<float>() * 50.f;
            desc.vPosition.y = inst["position"]["y"].get<float>() * 50.f;
            desc.vPosition.z = inst["position"]["z"].get<float>() * 50.f;

            desc.vRotation.x = XMConvertToRadians(inst["rotation"]["x"].get<float>());
            desc.vRotation.y = XMConvertToRadians(inst["rotation"]["y"].get<float>() + 180.f);
            desc.vRotation.z = XMConvertToRadians(inst["rotation"]["z"].get<float>());

            desc.vScale.x = inst["scale"]["x"].get<float>() / 2.0f;
            desc.vScale.y = inst["scale"]["y"].get<float>() / 2.0f;
            desc.vScale.z = inst["scale"]["z"].get<float>() / 2.0f;

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
            desc.vCenterCollider.x = colliderNode["center"]["x"].get<float>() * 50.f;
            desc.vCenterCollider.y = colliderNode["center"]["y"].get<float>() * 50.f;
            desc.vCenterCollider.z = colliderNode["center"]["z"].get<float>() * 50.f;


            desc.vExtentsCollider.x = colliderNode["extents"]["x"].get<float>() * 50.f;
            desc.vExtentsCollider.y = colliderNode["extents"]["y"].get<float>() * 50.f;
            desc.vExtentsCollider.z = colliderNode["extents"]["z"].get<float>() * 50.f;

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