#include "stdafx.h"
#include "Loader.h"
#include "GameInstance.h"


#include "Loader_Map.h"
#include "Texture.h"
#include "Model.h"
#include "Collider.h"
#include "Skybox.h"
#include "VIBuffer_Cube.h"
#include "VIBuffer_Skybox.h"
#include "Player_1rd.h"
#include "Obj_CollisionTest.h"
#include "Ketchup_Gun.h"
#include "Player_Pig.h"

CLoader::CLoader(EngineContext* pContext)
    : m_pContext {pContext}
    , m_pGameInstance {CGameInstance::GetInstance()}
{
    Safe_AddRef(m_pGameInstance);
}

HRESULT CLoader::Initialize(LEVELID eNextLevelID)
{
    m_eNextLevelID = eNextLevelID;

    return Loading();
}

HRESULT CLoader::Loading()
{
    HRESULT hr = E_FAIL;

    switch (m_eNextLevelID)
    {
    case LEVEL_LOGO:
        hr = Loading_Level_Logo();
        break;

    case LEVEL_GAMEPLAY:
        hr = Loading_Level_GamePlay();
        break;

    default:
        // 알 수 없는 레벨 ID
        return E_FAIL;
    }

    if (FAILED(hr))
        return E_FAIL;

    m_isFinished.store(true);

    return S_OK;
}

void CLoader::Set_LoadingText(const _tchar* pText)
{
    lstrcpy(m_szLoadingText, pText);
}

void CLoader::Get_LoadingText(_tchar* pOutBuffer, size_t bufferSize)
{
    if (nullptr == pOutBuffer || 0 == bufferSize)
        return;

    lstrcpyn(pOutBuffer, m_szLoadingText, static_cast<int>(bufferSize));
}

#ifdef _DEBUG
void CLoader::Show_Debug()
{
    _tchar szText[MAX_PATH] = {};
    Get_LoadingText(szText, MAX_PATH);
    SetWindowText(g_hWnd, szText);
}
#endif

// ----------------------------------------------------------------------
//  로딩 함수들
//  이땐 GPU는 동작하지 않고 Windoes API와 CPU 작업만 함
// ----------------------------------------------------------------------

HRESULT CLoader::Loading_Level_Logo()
{
    Set_LoadingText(TEXT("로고 텍스처를 로딩중입니다."));

    Set_LoadingText(TEXT("UI를 로딩중입니다."));

    Set_LoadingText(TEXT("로딩이 완료되었습니다."));

    return S_OK;
}

HRESULT CLoader::Loading_Level_GamePlay()
{
    Set_LoadingText(TEXT("텍스처를 로딩중입니다."));
    // 텍스쳐 로드
    {      
        // Prototype_Component_Texture_Cube
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, L"Prototype_Component_Texture_Cube",
            CTexture::Create(m_pContext, L"Resources/Textures/Rock.dds", 1))))
            return E_FAIL;
        // Prototype_Component_Texture_Skybox
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, L"Prototype_Component_Texture_Skybox",
            CTexture::Create(m_pContext, L"Resources/Textures/SkyBox_Cube.dds", 1, TEX_CUBE))))
            return E_FAIL;
    }

    Set_LoadingText(TEXT("폰트를 로딩중입니다."));

    Set_LoadingText(TEXT("콜라이더를 로딩중입니다."));
    // CCollider
    {
        // Prototype_Component_AABB
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, TEXT("Prototype_Component_AABB"),
            CCollider::Create(m_pContext, CCollider::TYPE_AABB))))
            return E_FAIL;
        // Prototype_Component_OBB
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, TEXT("Prototype_Component_OBB"),
            CCollider::Create(m_pContext, CCollider::TYPE_OBB))))
            return E_FAIL;
        // Prototype_Component_Sphere
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, TEXT("Prototype_Component_Sphere"),
            CCollider::Create(m_pContext, CCollider::TYPE_SPHERE))))
            return E_FAIL;
    }

    Set_LoadingText(TEXT("월드를 로딩중입니다."));
    CLoader_Map* pMapLoader = CLoader_Map::Create(m_pContext);
    pMapLoader->Load_MaterialData("Resources/NonAnim/Map/MaterialData.json");
    pMapLoader->Load_MapData("Resources/NonAnim/Map/MapData.json", LEVEL_GAMEPLAY);
    Safe_Release(pMapLoader);

    Set_LoadingText(TEXT("모델을 로딩중입니다."));
    // 모델 로드
    // 기본 버퍼
    {
        // Prototype_Component_VIBuffer_VtxCube
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, L"Prototype_Component_VIBuffer_VtxCube",
            CVIBuffer_Cube::Create(m_pContext))))
            return E_FAIL;
        // Prototype_Component_VIBuffer_Skybox
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, L"Prototype_Component_VIBuffer_Skybox",
            CVIBuffer_Skybox::Create(m_pContext))))
            return E_FAIL;
    }

    // Prototype_Component_Pig_3rd
    {
        _matrix PreTransformMatrix = XMMatrixIdentity() * XMMatrixScaling(1.f, 1.f, 1.f);
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, TEXT("Prototype_Component_Pig_3rd"),
            CModel::Create(m_pContext, CModel::TYPE_ANIM, L"Resources/Anim/Pig/Prototype_Component_Pig.txt", PreTransformMatrix))))
            return E_FAIL;
    }

    // Prototype_Component_ketchupGun
    {
        _matrix PreTransformMatrix = XMMatrixIdentity() * XMMatrixScaling(1.f, 1.f, 1.f);
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, TEXT("Prototype_Component_ketchupGun"),
            CModel::Create(m_pContext, CModel::TYPE_NONANIM, L"Resources/NonAnim/Gun/Prototype_Component_ketchupGun.txt", PreTransformMatrix))))
            return E_FAIL;
    }

    // Prototype_Component_pig_first_person
    {
        _matrix PreTransformMatrix = XMMatrixIdentity() * XMMatrixScaling(1.f, 1.f, 1.f);
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, TEXT("Prototype_Component_Player_Pig_fps"),
            CModel::Create(m_pContext, CModel::TYPE_ANIM, L"Resources/Anim/1st_Player/Prototype_Component_pig_first_person.txt", PreTransformMatrix))))
            return E_FAIL;
    }

    Set_LoadingText(TEXT("객체원형을 로딩중입니다."));
    // 객체 원형 로드
    // CCollider
    {
        // AABB
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, TEXT("Prototype_TestObject_AABB"),
            CObj_CollisionTest::Create(m_pContext))))
            return E_FAIL;
        // Sphere
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, TEXT("Prototype_TestObject_Sphere"),
            CObj_CollisionTest::Create(m_pContext))))
            return E_FAIL;
        // OBB
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, TEXT("Prototype_TestObject_OBB"),
            CObj_CollisionTest::Create(m_pContext))))
            return E_FAIL;
    }

    // Prototype_GameObject_Skybox
    if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, L"Prototype_GameObject_Skybox",
        CSkybox::Create(m_pContext))))
        return E_FAIL;

    // Prototype_GameObject_Ketchup_Gun
    if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, TEXT("Prototype_GameObject_Ketchup_Gun"),
        CKetchup_Gun::Create(m_pContext))))
        return E_FAIL;

    // Prototype_GameObject_Player_1rd
    if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, TEXT("Prototype_GameObject_Player_1rd"),
        CPlayer_1rd::Create(m_pContext))))
        return E_FAIL;

    // Prototype_GameObject_Pig_3rd
    if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_GAMEPLAY, TEXT("Prototype_GameObject_Player_Pig"),
        CPlayer_Pig::Create(m_pContext))))
        return E_FAIL;

    Set_LoadingText(TEXT("로딩이 완료되었습니다."));

    return S_OK;
}

HRESULT CLoader::Loading_Level_MapLoading()
{
    return E_NOTIMPL;
}

CLoader* CLoader::Create(EngineContext* pContext, LEVELID eNextLevelID)
{
    CLoader* pInstance = new CLoader(pContext);

    if (FAILED(pInstance->Initialize(eNextLevelID)))
    {
        MSG_BOX("Failed to Created : CLoader");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CLoader::Free()
{
    
    Safe_Release(m_pGameInstance);

    __super::Free();
}
