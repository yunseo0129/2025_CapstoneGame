#include "stdafx.h"
#include "Level_GamePlay.h"

#include "GameInstance.h"
#include "NetworkClient.h"
#include "Controller.h"

#include "Camera_FPV.h"
#include "Light.h"
#include "Shadow.h"

#include "Skybox.h"
#include "Player_1rd.h"
#include "Player_Pig.h"

// 테스트용
#include "Collider.h"
#include "Map.h"
//

#include "Game_Manager.h"
#include "UI_Panel.h"

CLevel_GamePlay::CLevel_GamePlay(EngineContext* pContext)
    : CLevel {pContext}
{
}

HRESULT CLevel_GamePlay::Initialize()
{
    Add_Camera();

    if (FAILED(Ready_Light()))
        return E_FAIL;

    if (FAILED(Ready_Layer()))
        return E_FAIL;

    if (FAILED(Ready_UI()))
        return E_FAIL;

    // 시작 시점에 사용할 카메라
    Set_CurrentCamera(CAMERA_FPV);

    m_pGameInstance->Build_StaticBVH();

    m_pGameManager = CGame_Manager::Create();   // 1라운드 SCOREBOARD부터 시작

    return S_OK;
}

void CLevel_GamePlay::Update(_float fTimeDelta)
{
    __super::Update(fTimeDelta);
    //Process_NetworkEvents();

    // 네트워크 이벤트 처리: 플레이어 추가/제거/이동
    NetworkClient* pNetwork = NetworkClient::GetInstance();
    if (!pNetwork->IsConnected())
        return;

    std::vector<NetworkClient::NetEvent> events;
    pNetwork->PopAllEvents(events);

    if (m_pGameManager) m_pGameManager->Update(fTimeDelta);

    for (auto& evt : events) {
        switch (evt.type) {
        case NetworkClient::NetEventType::PLAYER_ADD: {
            // TODO: evt.id 플레이어 생성 + evt.cameraPos 위치에 배치 + evt.name 이름 설정
            break;
        }
        case NetworkClient::NetEventType::PLAYER_REMOVE: {
            // TODO: evt.id 플레이어 제거
            break;
        }
        case NetworkClient::NetEventType::PLAYER_MOVE: {
            // TODO: 해당 id 플레이어의 Transform 갱신
            break;
        }
        }
    }
}

HRESULT CLevel_GamePlay::Render()
{
    return S_OK;
}

void CLevel_GamePlay::Add_Camera()
{
    ShowCursor(false);

    CCamera_FPV::FPV_CAMERA_DESC tDesc;
    tDesc.vEye = _float3 {0.f, 1.38f, -100.f};
    tDesc.vAt = _float3 {0.f, 1.38f, 0.f};
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

HRESULT CLevel_GamePlay::Ready_Light()
{
    CLight::LIGHT_DESC LightDesc {};
    ZeroMemory(&LightDesc, sizeof LightDesc);

    LightDesc.eType = CLight::LIGHT_DESC::TYPE_DIRECTIONAL;
    LightDesc.vDirection = _float4(1.f, -1.f, 1.f, 0.f);
    LightDesc.vDiffuse = _float4(1.0f, 1.0f, 0.95f, 1.f);
    LightDesc.vAmbient = _float4(0.15f, 0.15f, 0.2f, 1.f);
    LightDesc.vSpecular = _float4(1.f, 1.f, 1.f, 1.f);

    CLight* pLight = CLight::Create(m_pContext, LightDesc);
    if (nullptr == pLight)
        return E_FAIL;

    CShadow* pShadow = CShadow::Create(m_pContext, 8192, 8192);
    if (nullptr == pShadow)
        return E_FAIL;

    pLight->Set_Shadow(pShadow);
    m_pLights.push_back(pLight);

    return S_OK;
}

HRESULT CLevel_GamePlay::Ready_Layer()
{
    // Skybox
    CSkybox::Skybox_DESC SkyboxDesc;
    SkyboxDesc.strTextureTag = L"Prototype_Component_Texture_Skybox";
    SkyboxDesc.strVIbufferTag = L"Prototype_Component_VIBuffer_Skybox";
    SkyboxDesc.iModelLevelIndex = LEVEL_GAMEPLAY;
    if (FAILED(m_pGameInstance->Add_GameObject_ToLayer(LEVEL_GAMEPLAY, L"Prototype_GameObject_Skybox",
        LEVEL_GAMEPLAY, L"Layer_Skybox", &SkyboxDesc)))
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
        cdesc.iModelLevelIndex = LEVEL_GAMEPLAY;
        cdesc.vPos = _float3(0.f, 0.f, 0.f);
        cdesc.pCamera = static_cast<CCamera_FPV*>(m_pCamera[CAMERA_FPV]);
        CGameObject* pPlayer = m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(LEVEL_GAMEPLAY, TEXT("Prototype_GameObject_Player_1rd"),
            LEVEL_GAMEPLAY, TEXT("Layer_Player"), &cdesc);

        m_pGameInstance->Get_Controller()->Set_Player(static_cast<CPlayer_1rd*>(pPlayer));
    }

    // Pig_3rd
    {
        CPlayer_Pig::PLAYER_PIG_DESC eState;
        eState.fSpeedPerSec = 1.f;
        eState.vRotation = _float3(0.f, XM_PI, 0.f);
        eState.vPos = _float3(0.f, 0.f, 0.f);
        eState.strModelTag = L"Prototype_Component_Pig_3rd";
        eState.iModelLevelIndex = LEVEL_GAMEPLAY;
        m_pGameInstance->Add_GameObject_ToLayer(LEVEL_GAMEPLAY, TEXT("Prototype_GameObject_Player_Pig"),
            LEVEL_GAMEPLAY, TEXT("Layer_Other_Player"), &eState);
    }
    // Pig_3rd
    {
        CPlayer_Pig::PLAYER_PIG_DESC eState;
        eState.fSpeedPerSec = 1.f;
        eState.vRotation = _float3(0.f, XM_PI, 0.f);
        eState.vPos = _float3(1.f, 0.f, 0.f);
        eState.strModelTag = L"Prototype_Component_Pig_3rd";
        eState.iModelLevelIndex = LEVEL_GAMEPLAY;
        m_pGameInstance->Add_GameObject_ToLayer(LEVEL_GAMEPLAY, TEXT("Prototype_GameObject_Player_Pig"),
            LEVEL_GAMEPLAY, TEXT("Layer_Other_Player"), &eState);
    }
    // Pig_3rd
    {
        CPlayer_Pig::PLAYER_PIG_DESC eState;
        eState.fSpeedPerSec = 1.f;
        eState.vRotation = _float3(0.f, XM_PI, 0.f);
        eState.vPos = _float3(2.f, 0.f, 0.f);
        eState.strModelTag = L"Prototype_Component_Pig_3rd";
        eState.iModelLevelIndex = LEVEL_GAMEPLAY;
        m_pGameInstance->Add_GameObject_ToLayer(LEVEL_GAMEPLAY, TEXT("Prototype_GameObject_Player_Pig"),
            LEVEL_GAMEPLAY, TEXT("Layer_Other_Player"), &eState);
    }

    return S_OK;
} 

static inline _float4 UICol(_int r, _int g, _int b, _float a)
{
    return _float4(r / 255.f, g / 255.f, b / 255.f, a);
}

HRESULT CLevel_GamePlay::Ready_UI()
{
    const _uint PROTO = LEVEL_STATIC;        // UI 프로토타입이 등록된 레벨
    const _uint LV = LEVEL_GAMEPLAY;      // 생성될 레벨
    const _wstring SB = L"Layer_UI_Scoreboard";
    const _wstring SH = L"Layer_UI_Shop";

    // -----------------------------------------------------------------
    //  1) 스코어보드 (Layer_UI_Scoreboard)
    // -----------------------------------------------------------------
    // 좌표는 1280x720 기준. (설계 시안과 동일 비율)
    // depth: 값이 클수록 뒤. 배경 0.8, 행 0.5

    // 상단 점수판
    {
        CUI_Panel::UI_PANEL_DESC d;
        d.fX = 440.f; d.fY = 40.f; d.fSizeX = 400.f; d.fSizeY = 70.f;
        d.fDepth = 0.7f;
        d.vColor = UICol(46, 50, 60, 0.92f);
        m_pGameInstance->Add_GameObject_ToLayer(PROTO, L"Prototype_GameObject_UI_Panel", LV, SB, &d);
    }

    // 팀 A 패널 (좌측, 파랑 계열)
    {
        CUI_Panel::UI_PANEL_DESC d;
        d.fX = 120.f; d.fY = 150.f; d.fSizeX = 480.f; d.fSizeY = 420.f;
        d.fDepth = 0.8f;
        d.vColor = UICol(28, 44, 72, 0.9f);
        m_pGameInstance->Add_GameObject_ToLayer(PROTO, L"Prototype_GameObject_UI_Panel", LV, SB, &d);
    }
    // 팀 A 행 3개
    for (_int i = 0; i < 3; ++i)
    {
        CUI_Panel::UI_PANEL_DESC d;
        d.fX = 144.f; d.fY = 220.f + i * 64.f; d.fSizeX = 432.f; d.fSizeY = 52.f;
        d.fDepth = 0.5f;
        d.vColor = UICol(38, 60, 96, 0.95f);
        m_pGameInstance->Add_GameObject_ToLayer(PROTO, L"Prototype_GameObject_UI_Panel", LV, SB, &d);
    }

    // 팀 B 패널 (우측, 빨강 계열)
    {
        CUI_Panel::UI_PANEL_DESC d;
        d.fX = 680.f; d.fY = 150.f; d.fSizeX = 480.f; d.fSizeY = 420.f;
        d.fDepth = 0.8f;
        d.vColor = UICol(72, 32, 32, 0.9f);
        m_pGameInstance->Add_GameObject_ToLayer(PROTO, L"Prototype_GameObject_UI_Panel", LV, SB, &d);
    }
    // 팀 B 행 3개
    for (_int i = 0; i < 3; ++i)
    {
        CUI_Panel::UI_PANEL_DESC d;
        d.fX = 704.f; d.fY = 220.f + i * 64.f; d.fSizeX = 432.f; d.fSizeY = 52.f;
        d.fDepth = 0.5f;
        d.vColor = UICol(96, 48, 48, 0.95f);
        m_pGameInstance->Add_GameObject_ToLayer(PROTO, L"Prototype_GameObject_UI_Panel", LV, SB, &d);
    }

    // -----------------------------------------------------------------
    //  2) 상점 (Layer_UI_Shop)
    // -----------------------------------------------------------------
    // 전체 반투명 오버레이 (게임이 뒤로 비침)
    {
        CUI_Panel::UI_PANEL_DESC d;
        d.fX = 0.f; d.fY = 0.f; d.fSizeX = 1280.f; d.fSizeY = 720.f;
        d.fDepth = 0.95f;                          // 가장 뒤
        d.vColor = UICol(8, 10, 16, 0.55f);        // 어둡게 반투명
        m_pGameInstance->Add_GameObject_ToLayer(PROTO, L"Prototype_GameObject_UI_Panel", LV, SH, &d);
    }
    // 상점 헤더
    {
        CUI_Panel::UI_PANEL_DESC d;
        d.fX = 160.f; d.fY = 120.f; d.fSizeX = 960.f; d.fSizeY = 64.f;
        d.fDepth = 0.6f;
        d.vColor = UICol(120, 90, 30, 0.95f);
        m_pGameInstance->Add_GameObject_ToLayer(PROTO, L"Prototype_GameObject_UI_Panel", LV, SH, &d);
    }
    // 상품 슬롯 그리드 3 x 2
    for (_int row = 0; row < 2; ++row)
    {
        for (_int col = 0; col < 3; ++col)
        {
            CUI_Panel::UI_PANEL_DESC d;
            d.fX = 160.f + col * 332.f;
            d.fY = 232.f + row * 216.f;
            d.fSizeX = 300.f; d.fSizeY = 184.f;
            d.fDepth = 0.5f;
            d.vColor = UICol(30, 120, 100, 0.95f);
            m_pGameInstance->Add_GameObject_ToLayer(PROTO, L"Prototype_GameObject_UI_Panel", LV, SH, &d);
        }
    }

    // 생성 직후 상점은 꺼둠 (스코어보드부터 시작)
    {
        list<CGameObject*> shopObjs = m_pGameInstance->Get_List(LV, SH);
        for (auto* p : shopObjs)
            if (p) p->SetOnOff(false);
    }

    return S_OK;
}

void CLevel_GamePlay::Process_NetworkEvents()
{
	// Update()에서 매 프레임마다 네트워크 이벤트를 처리하는 함수 여기로 옮기기
}

CLevel_GamePlay* CLevel_GamePlay::Create(EngineContext* pContext)
{
    CLevel_GamePlay* pInstance = new CLevel_GamePlay(pContext);

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : CLevel_GamePlay");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CLevel_GamePlay::Free()
{
    for (auto& pLight : m_pLights)
        Safe_Release(pLight);
    m_pLights.clear();
    Safe_Release(m_pGameManager);
    __super::Free();
}