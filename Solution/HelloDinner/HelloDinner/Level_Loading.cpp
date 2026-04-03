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
#include "Player_Pig.h"
#include "NetworkClient.h"
#include "NetPlayer.h"
#include "Collider.h"
#include "Player_1rd.h"
#include "Ketchup_Gun.h"
#include "Transform.h"

#include "Obj_CollisionTest.h"

#include "Light.h"
#include "Shadow.h"

CLevel_Loading::CLevel_Loading(EngineContext* pContext)
    : CLevel{ pContext }
{

}

HRESULT CLevel_Loading::Initialize(LEVELID eNextLevelID)
{
    m_eNextLevelID = eNextLevelID;

    Add_Camera();

    if (FAILED(Ready_Light()))
        return E_FAIL;

    if (FAILED(Ready_Component_Prototype()))
        return E_FAIL;

    if (FAILED(Ready_GameObject_Prototype()))
        return E_FAIL;

    if (FAILED(Ready_Layer()))
        return E_FAIL;

    // Collider prototype 때문에 Map 후순위로 변경
    // MapData.json → 모델 생성 + 텍스처 바인딩 + 맵 배치
    CLoader_Map* pMapLoader = CLoader_Map::Create(m_pContext);
    if (FAILED(pMapLoader->Load_MaterialData("Resources/NonAnim/Map/MaterialData.json")))
    {
        MSG_BOX("Failed to load material data");
    }
    if (FAILED(pMapLoader->Load_MapData("Resources/NonAnim/Map/MapData.json", LEVEL_LOADING)))
    {
        MSG_BOX("Failed to load map data");
    }
    Safe_Release(pMapLoader);

    return S_OK;
}

void CLevel_Loading::Update(_float fTimeDelta)
{
    __super::Update(fTimeDelta);

    ProcessNetworkEvents();
}

void CLevel_Loading::ProcessNetworkEvents()
{
    NetworkClient* pNetwork = NetworkClient::GetInstance();
    if (!pNetwork->IsConnected())
        return;

    int iMyId = pNetwork->GetMyId();

    // 1) 플레이어 이벤트 처리
    std::vector<NetPlayer::Event> playerEvents;
    pNetwork->PopAllPlayerEvents(playerEvents);

    for (auto& evt : playerEvents) {
        switch (evt.type) {
        case NetPlayer::EventType::ADDED: {
            // 내 플레이어는 이미 Ready_Layer에서 생성됨 → 위치만 갱신
            if (evt.id == iMyId) {
                if (m_pMyPlayer) {
                    CTransform* pTransform = static_cast<CTransform*>(
                        m_pMyPlayer->Find_Component(TEXT("Com_Transform")));
                    if (pTransform) {
                        // 서버가 보낸 월드행렬을 그대로 적용
                        XMMATRIX matWorld = XMMATRIX(evt.worldMatrix);
                        XMFLOAT4 vRow;
                        XMStoreFloat4(&vRow, matWorld.r[0]);
                        pTransform->Set_State(CTransform::STATE_RIGHT, XMLoadFloat4(&vRow));
                        XMStoreFloat4(&vRow, matWorld.r[1]);
                        pTransform->Set_State(CTransform::STATE_UP, XMLoadFloat4(&vRow));
                        XMStoreFloat4(&vRow, matWorld.r[2]);
                        pTransform->Set_State(CTransform::STATE_LOOK, XMLoadFloat4(&vRow));
                        XMStoreFloat4(&vRow, matWorld.r[3]);
                        pTransform->Set_State(CTransform::STATE_POSITION, XMLoadFloat4(&vRow));
                    }
                }
                break;
            }

            // 다른 플레이어 → CPlayer_Pig 생성
            if (m_mapNetPlayers.find(evt.id) != m_mapNetPlayers.end())
                break;  // 이미 존재하면 무시

            CPlayer_Pig::PLAYER_PIG_DESC desc;
            desc.fSpeedPerSec = 1.f;
            desc.fRotationPerSec = 1.f;
            desc.vRotation = _float3(0.f, XM_PI, 0.f);
            desc.vPos = _float3(evt.worldMatrix[12], evt.worldMatrix[13], evt.worldMatrix[14]);
            desc.strModelTag = L"Prototype_Component_Pig_3rd";
            desc.iModelLevelIndex = LEVEL_LOADING;

            CGameObject* pNewPlayer = m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
                LEVEL_LOADING, TEXT("Prototype_GameObject_Player_Pig"),
                LEVEL_LOADING, TEXT("Layer_Other_Player"), &desc);

            if (pNewPlayer) {
                m_mapNetPlayers[evt.id] = pNewPlayer;
            }
            break;
        }
        case NetPlayer::EventType::REMOVED: {
            // 내 플레이어 제거는 무시
            if (evt.id == iMyId)
                break;

            auto it = m_mapNetPlayers.find(evt.id);
            if (it != m_mapNetPlayers.end()) {
                // 오브젝트 비활성화 (엔진의 오브젝트 관리 정책에 따라 처리)
                it->second->SetDead();
                m_mapNetPlayers.erase(it);
            }
            break;
        }
        case NetPlayer::EventType::MOVED: {
            // 내 플레이어는 로컬 예측으로 이미 움직이고 있으므로 서버 보정만 적용
            if (evt.id == iMyId) {
                // TODO: 서버 보정(reconciliation) 필요 시 여기에 구현
                break;
            }

            auto it = m_mapNetPlayers.find(evt.id);
            if (it == m_mapNetPlayers.end())
                break;

            CGameObject* pPlayer = it->second;
            CTransform* pTransform = static_cast<CTransform*>(
                pPlayer->Find_Component(TEXT("Com_Transform")));
            if (!pTransform)
                break;

            // 서버가 보낸 권위적 월드행렬을 그대로 적용
            XMMATRIX matWorld = XMMATRIX(evt.worldMatrix);
            XMFLOAT4 vRow;
            XMStoreFloat4(&vRow, matWorld.r[0]);
            pTransform->Set_State(CTransform::STATE_RIGHT, XMLoadFloat4(&vRow));
            XMStoreFloat4(&vRow, matWorld.r[1]);
            pTransform->Set_State(CTransform::STATE_UP, XMLoadFloat4(&vRow));
            XMStoreFloat4(&vRow, matWorld.r[2]);
            pTransform->Set_State(CTransform::STATE_LOOK, XMLoadFloat4(&vRow));
            XMStoreFloat4(&vRow, matWorld.r[3]);
            pTransform->Set_State(CTransform::STATE_POSITION, XMLoadFloat4(&vRow));
            break;
        }
        }
    }

    // 2) 매칭 이벤트 처리
    std::vector<NetworkClient::NetEvent> matchEvents;
    pNetwork->PopAllMatchEvents(matchEvents);

    for (auto& evt : matchEvents) {
        switch (evt.type) {
        case NetworkClient::NetEventType::MATCH_WAIT:
            // TODO: 대기열 UI 갱신 (evt.queueSize)
            break;
        case NetworkClient::NetEventType::MATCH_SUCCESS:
            // TODO: 방 입장 처리 (evt.roomId, evt.playerIds)
            break;
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
    ShowCursor(false);
    CCamera_FPV::FPV_CAMERA_DESC tDesc;
    tDesc.vEye = _float3{ 0.f, 1.38f, -5.f };
    tDesc.vAt = _float3{ 0.f, 1.38f, 0.f };
    tDesc.fFovy = XMConvertToRadians(60.f);
    tDesc.fAspect = 1280.f / 720.f;
    tDesc.fNear = 0.1f;
    tDesc.fFar = 100.f;
    tDesc.fCamMouseSensor = 1.f;
    tDesc.fCamSpeedPerSec = 1.f;
    tDesc.fRotationPerSec = 1.f;
    tDesc.fSpeedPerSec = 1.f;

    CCamera_FPV* pCamera = CCamera_FPV::Create(m_pContext);
    pCamera->Initialize(&tDesc);

    m_pCamera[CAMERA_FPV] = pCamera;
}

HRESULT CLevel_Loading::Ready_Component_Prototype()
{
    // 충돌체 로드
    // CCollider
    {
        // Prototype_Component_AABB
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, TEXT("Prototype_Component_AABB"),
            CCollider::Create(m_pContext, CCollider::TYPE_AABB))))
            return E_FAIL;
        // Prototype_Component_OBB
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, TEXT("Prototype_Component_OBB"),
            CCollider::Create(m_pContext, CCollider::TYPE_OBB))))
            return E_FAIL;
        // Prototype_Component_Sphere
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, TEXT("Prototype_Component_Sphere"),
            CCollider::Create(m_pContext, CCollider::TYPE_SPHERE))))
            return E_FAIL;
    }

    // 텍스쳐 로드
    // Prototype_Component_Texture_Cube
    if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_Component_Texture_Cube",
        CTexture::Create(m_pContext, L"Resources/Textures/Rock.dds", 1))))
        return E_FAIL;
    // Prototype_Component_Texture_Skybox
    if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_Component_Texture_Skybox",
        CTexture::Create(m_pContext, L"Resources/Textures/SkyBox_Cube.dds", 1, TEX_CUBE))))
        return E_FAIL;


    // 모델 로드
    // 기본 버퍼
    {
        // Prototype_Component_VIBuffer_VtxCube
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_Component_VIBuffer_VtxCube",
            CVIBuffer_Cube::Create(m_pContext))))
            return E_FAIL;
        // Prototype_Component_VIBuffer_Skybox
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_Component_VIBuffer_Skybox",
            CVIBuffer_Skybox::Create(m_pContext))))
            return E_FAIL;
    }

    // Prototype_Component_Pig_3rd
    {
        _matrix PreTransformMatrix = XMMatrixIdentity() * XMMatrixScaling(1.f, 1.f, 1.f);
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, TEXT("Prototype_Component_Pig_3rd"),
            CModel::Create(m_pContext, CModel::TYPE_ANIM, L"Resources/Anim/Pig/Prototype_Component_Pig.txt", PreTransformMatrix))))
            return E_FAIL;
    }

    // Prototype_Component_ketchupGun
    {
        _matrix PreTransformMatrix = XMMatrixIdentity() * XMMatrixScaling(1.f, 1.f, 1.f);
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, TEXT("Prototype_Component_ketchupGun"),
            CModel::Create(m_pContext, CModel::TYPE_NONANIM, L"Resources/NonAnim/Gun/Prototype_Component_ketchupGun.txt", PreTransformMatrix))))
            return E_FAIL;
    }

    // Prototype_Component_pig_first_person
    {
        _matrix PreTransformMatrix = XMMatrixIdentity() * XMMatrixScaling(1.f, 1.f, 1.f)  * XMMatrixTranslation(0.1f, 1.39f, -0.2f);
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, TEXT("Prototype_Component_Player_Pig_fps"),
            CModel::Create(m_pContext, CModel::TYPE_ANIM, L"Resources/Anim/1st_Player/Prototype_Component_pig_first_person.txt", PreTransformMatrix))))
            return E_FAIL;
    }

    return S_OK;
}

HRESULT CLevel_Loading::Ready_GameObject_Prototype()
{
    // 객체 원형 로드
    // CCollider
    {
        // AABB
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, TEXT("Prototype_TestObject_AABB"),
            CObj_CollisionTest::Create(m_pContext))))
            return E_FAIL;
        // Sphere
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, TEXT("Prototype_TestObject_Sphere"),
            CObj_CollisionTest::Create(m_pContext))))
            return E_FAIL;
        // OBB
        if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, TEXT("Prototype_TestObject_OBB"),
            CObj_CollisionTest::Create(m_pContext))))
            return E_FAIL;
    }

    // Prototype_GameObject_Skybox
    if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, L"Prototype_GameObject_Skybox",
        CSkybox::Create(m_pContext))))
        return E_FAIL;

    // Prototype_GameObject_Ketchup_Gun
    if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, TEXT("Prototype_GameObject_Ketchup_Gun"),
        CKetchup_Gun::Create(m_pContext))))
        return E_FAIL;

    // Prototype_GameObject_Player_1rd
    if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, TEXT("Prototype_GameObject_Player_1rd"),
        CPlayer_1rd::Create(m_pContext))))
        return E_FAIL;

    // Prototype_GameObject_Pig_3rd
    if (FAILED(m_pGameInstance->Add_Prototype(LEVEL_LOADING, TEXT("Prototype_GameObject_Player_Pig"),
        CPlayer_Pig::Create(m_pContext))))
        return E_FAIL;

    return S_OK;
}

HRESULT CLevel_Loading::Ready_Light()
{
    CLight::LIGHT_DESC         LightDesc{};
    ZeroMemory(&LightDesc, sizeof LightDesc);

    LightDesc.eType = CLight::LIGHT_DESC::TYPE_DIRECTIONAL;
    LightDesc.vDirection = _float4(1.f, -1.f, 1.f, 0.f);
    LightDesc.vDiffuse = _float4(1.0f, 1.0f, 0.95f, 1.f);
    LightDesc.vAmbient = _float4(0.15f, 0.15f, 0.2f, 1.f);
    LightDesc.vSpecular = _float4(1.f, 1.f, 1.f, 1.f);

    CLight* pLight = CLight::Create(m_pContext, LightDesc);
    if (nullptr == pLight)
        return E_FAIL;

    // Shadow
    CShadow* pShadow = CShadow::Create(m_pContext, 8192, 8192);
    if (nullptr == pShadow)
        return E_FAIL;

    pLight->Set_Shadow(pShadow);

    m_pLights.push_back(pLight);

    return S_OK;
}

HRESULT CLevel_Loading::Ready_Layer()
{
    // Collider Test
    //{
    //   // AABB
    //   {
    //      CObj_CollisionTest::Obj_CollisionTest_DESC cdesc;
    //      cdesc.vPosition = _float3(0.f, 0.f, 0.f);
    //      cdesc.vRotation = _float3(0.f, 0.f, 0.f);
    //      cdesc.vScale = _float3(1.f, 1.f, 1.f);
    //      cdesc.eColliderType = CCollider::TYPE_AABB;
    //      m_pGameInstance->Add_GameObject_ToLayer(LEVEL_LOADING, TEXT("Prototype_TestObject_AABB"),
    //         LEVEL_LOADING, TEXT("Layer_CollisionTest"), &cdesc);
    //   }
    //   // Sphere
    //   {
    //      CObj_CollisionTest::Obj_CollisionTest_DESC cdesc;
    //      cdesc.vPosition = _float3(200.f, 0.f, 0.f);
    //      cdesc.vRotation = _float3(0.f, 0.f, 0.f);
    //      cdesc.vScale = _float3(1.f, 1.f, 1.f);
    //      cdesc.eColliderType = CCollider::TYPE_SPHERE;
    //      m_pGameInstance->Add_GameObject_ToLayer(LEVEL_LOADING, TEXT("Prototype_TestObject_Sphere"),
    //         LEVEL_LOADING, TEXT("Layer_CollisionTest"), &cdesc);
    //   }
    //   // OBB
    //   {
    //      CObj_CollisionTest::Obj_CollisionTest_DESC cdesc;
    //      cdesc.vPosition = _float3(400.f, 0.f, 0.f);
    //      cdesc.vRotation = _float3(0.f, 45.f, 45.f);
    //      cdesc.vScale = _float3(1.f, 1.f, 1.f);
    //      cdesc.eColliderType = CCollider::TYPE_OBB;
    //      m_pGameInstance->Add_GameObject_ToLayer(LEVEL_LOADING, TEXT("Prototype_TestObject_OBB"),
    //         LEVEL_LOADING, TEXT("Layer_CollisionTest"), &cdesc);
    //   }
    //}

    // Skybox
    CSkybox::Skybox_DESC SkyboxDesc;
    SkyboxDesc.strTextureTag = L"Prototype_Component_Texture_Skybox";
    SkyboxDesc.strVIbufferTag = L"Prototype_Component_VIBuffer_Skybox";
    SkyboxDesc.iModelLevelIndex = LEVEL_LOADING;
    if (FAILED(m_pGameInstance->Add_GameObject_ToLayer(LEVEL_LOADING, L"Prototype_GameObject_Skybox",
        LEVEL_LOADING, L"Layer_Skybox", &SkyboxDesc)))
    {
        MSG_BOX("Failed to Add GameObject To Layer : Skybox");
        return E_FAIL;
    }

    // Player_1rd
    {
        CPlayer_1rd::Player_1RD_DESC cdesc;
        cdesc.strModelTag = L"Prototype_Component_Pig_3rd";
        cdesc.fSpeedPerSec = 1.f;
        cdesc.vRotation = _float3(0.f, 0.f, 0.f);
        cdesc.iModelLevelIndex = LEVEL_LOADING;
        cdesc.vPos = _float3(0.f, 0.f, -5.f);
        cdesc.pCamera = static_cast<CCamera_FPV*>(m_pCamera[CAMERA_FPV]);
        m_pGameInstance->Add_GameObject_ToLayer(LEVEL_LOADING, TEXT("Prototype_GameObject_Player_1rd"),
            LEVEL_LOADING, TEXT("Layer_Player"), &cdesc);

        // 플레이어를 참조하기 위한 포인터 저장
        CGameObject* pObj = m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
            LEVEL_LOADING, TEXT("Prototype_GameObject_Player_1rd"),
            LEVEL_LOADING, TEXT("Layer_Player"), &cdesc);

        m_pMyPlayer = static_cast<CPlayer_1rd*>(pObj);
    }

    // Pig_3rd
    {
        CPlayer_Pig::PLAYER_PIG_DESC eState;
        eState.fSpeedPerSec = 1.f;
        eState.vRotation = _float3(0.f, XM_PI, 0.f);
        eState.vPos = _float3(0.f, 0.f, 0.f);
        eState.strModelTag = L"Prototype_Component_Pig_3rd";
        eState.iModelLevelIndex = LEVEL_LOADING;
        m_pGameInstance->Add_GameObject_ToLayer(LEVEL_LOADING, TEXT("Prototype_GameObject_Player_Pig"),
            LEVEL_LOADING, TEXT("Layer_Other_Player"), &eState);
    }
    // Pig_3rd
    {
        CPlayer_Pig::PLAYER_PIG_DESC eState;
        eState.fSpeedPerSec = 1.f;
        eState.vRotation = _float3(0.f, XM_PI, 0.f);
        eState.vPos = _float3(1.f, 0.f, 0.f);
        eState.strModelTag = L"Prototype_Component_Pig_3rd";
        eState.iModelLevelIndex = LEVEL_LOADING;
        m_pGameInstance->Add_GameObject_ToLayer(LEVEL_LOADING, TEXT("Prototype_GameObject_Player_Pig"),
            LEVEL_LOADING, TEXT("Layer_Other_Player"), &eState);
    }
    // Pig_3rd
    {
        CPlayer_Pig::PLAYER_PIG_DESC eState;
        eState.fSpeedPerSec = 1.f;
        eState.vRotation = _float3(0.f, XM_PI, 0.f);
        eState.vPos = _float3(2.f, 0.f, 0.f);
        eState.strModelTag = L"Prototype_Component_Pig_3rd";
        eState.iModelLevelIndex = LEVEL_LOADING;
        m_pGameInstance->Add_GameObject_ToLayer(LEVEL_LOADING, TEXT("Prototype_GameObject_Player_Pig"),
            LEVEL_LOADING, TEXT("Layer_Other_Player"), &eState);
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
    m_mapNetPlayers.clear();

    __super::Free();
    for (auto& pLight : m_pLights)
        Safe_Release(pLight);

    m_pLights.clear();

}