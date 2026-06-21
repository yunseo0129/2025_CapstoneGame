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
#include "Player_Chick.h"

// 테스트용
#include "Collider.h"
#include "Map.h"
//

#include "Game_Manager.h"
#include "UI_Panel.h"
#include "MiniMap.h"
#include "MapSelect.h"
#include "MapRenderTarget.h"
#include "Bullets.h"

#include "UI_Crosshair.h"
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

    if (FAILED(Ready_MapRT()))
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

    // [캐릭터 선택 카메라 고정]
    //  플레이어가 Priority_Update에서 카메라를 자기 위치로 동기화한다(그게 먼저 돈다).
    //  레벨 Update는 그 뒤·Draw 직전이므로, 선택 단계엔 여기서 매 프레임 덮어써야
    //  카메라가 "선택 시점"에 고정된다. (준비완료로 phase가 바뀌면 자동으로 플레이어가 다시 가져감)
    if (m_pGameManager && m_pGameManager->Get_Phase() == GAME_PHASE::PHASE_CHARSELECT)
    {
        // 선택 시점 — 내 시작 지점(팀/번호) 기준으로 정면에서 바라본다.
        //  프리뷰 돼지(Game_Manager::Ready_CharSelect)도 같은 지점에 서 있다.
        _float3 spot = m_pGameManager->Get_MySpot();
        const XMVECTOR vSpot = XMVectorSet(spot.x, spot.y, spot.z, 0.f);

        const XMVECTOR eyeOff = XMVectorSet(0.f, 1.25f, -6.5f, 1.f);
        const XMVECTOR atOff = XMVectorSet(0.f, 0.95f, 0.f, 1.f);
        const XMVECTOR eye = XMVectorAdd(vSpot, eyeOff);
        const XMVECTOR at = XMVectorAdd(vSpot, atOff);
        const XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

        // 카메라 월드 = 뷰의 역행렬 (뷰 = inverse(world)이므로)
        XMMATRIX world = XMMatrixInverse(nullptr, XMMatrixLookAtLH(eye, at, up));
        XMFLOAT4X4 w;
        XMStoreFloat4x4(&w, world);
        static_cast<CCamera_FPV*>(m_pCamera[CAMERA_FPV])->Set_WorldMatrix(w);
    }

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
    tDesc.fFar = 400.f;
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
    LightDesc.vDirection = _float4(0.2f, -1.f, 0.2f, 0.f);
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

// =====================================================================
//  [맵 RTT] 탑다운 맵 렌더 타겟
// =====================================================================
HRESULT CLevel_GamePlay::Ready_MapRT()
{
    // 256x256 컬러 타겟. 클리어색 투명(맵이 없는 영역은 패널 배경이 비침).
    m_pMapRT = CMapRenderTarget::Create(m_pContext, 256, 256, _float4(0.f, 0.f, 0.f, 0.f));
    if (nullptr == m_pMapRT)
        return E_FAIL;

    // 초기 뷰: 원점 중심, 반경 5000유닛(≈50m, ×100 스케일), 북쪽 고정
    m_pMapRT->Set_View(_float3(0.f, 0.f, 0.f), 100.f, _float3(0.f, 0.f, 1.f));
    m_bMapRTActive = false;
    return S_OK;
}

void CLevel_GamePlay::Set_MapRTView(const _float3& vCenterXZ, _float fHalfExtent, const _float3& vUpDirXZ)
{
    if (m_pMapRT)
        m_pMapRT->Set_View(vCenterXZ, fHalfExtent, vUpDirXZ);
}

_bool CLevel_GamePlay::Begin_MapRTPass(ID3D12GraphicsCommandList* cmdList)
{
    if (!m_pMapRT || !m_bMapRTActive)
        return false;
    m_pMapRT->Begin_Pass(cmdList);
    return true;
}

void CLevel_GamePlay::Render_MapRT(ID3D12GraphicsCommandList* cmdList)
{
    if (!m_pMapRT) return;
    m_pMapRT->Render_MapLayer(cmdList, LEVEL_GAMEPLAY, L"Layer_Map");
}

void CLevel_GamePlay::End_MapRTPass(ID3D12GraphicsCommandList* cmdList)
{
    if (!m_pMapRT) return;
    m_pMapRT->End_Pass(cmdList);
    // 한 프레임용으로 자동 비활성화. 보이는 UI 가 매 프레임 다시 켠다(Update 에서).
    m_bMapRTActive = false;
}

_uint CLevel_GamePlay::Get_MapRT_SRVIndex() const
{
    return m_pMapRT ? m_pMapRT->Get_SRVIndex() : 0;
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

    // Bullets
    {
        CBullets::BULLET_DESC cdesc;
        CGameObject* pBullets = m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(LEVEL_GAMEPLAY, TEXT("Prototype_GameObject_Bullets"),
            LEVEL_GAMEPLAY, TEXT("Layer_Bullets"), &cdesc);

        m_pGameInstance->Set_Bullets(static_cast<CBullets*>(pBullets));
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

    // Chick_3rd
    {
        CPlayer_Chick::PLAYER_CHICK_DESC eState;
        eState.fSpeedPerSec = 1.f;
        eState.vRotation = _float3(0.f, XM_PI, 0.f);
        eState.vPos = _float3(-1.f, 0.f, 0.f);
        eState.strModelTag = L"Prototype_Component_Chick_3rd";
        eState.iModelLevelIndex = LEVEL_GAMEPLAY;
        m_pGameInstance->Add_GameObject_ToLayer(LEVEL_GAMEPLAY, TEXT("Prototype_GameObject_Player_Chick"),
            LEVEL_GAMEPLAY, TEXT("Layer_Other_Player"), &eState);
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
    // 상품 슬롯: 클릭 가능한 무기 슬롯은 CGame_Manager::Ready_ShopSlots() 에서
    //  Layer_UI_Shop 에 직접 생성한다. (슬롯0=케첩건/빨강, 슬롯1=마요네즈건/하양)
    //  여기서는 더 이상 더미 그리드를 만들지 않는다.

    // 생성 직후 상점은 꺼둠 (스코어보드부터 시작)
    {
        list<CGameObject*> shopObjs = m_pGameInstance->Get_List(LV, SH);
        for (auto* p : shopObjs)
            if (p) p->SetOnOff(false);
    }

    // MiniMap
    {
        CMiniMap::MINIMAP_DESC desc;
        desc.fX = 1060.f;  desc.fY = 20.f;
        desc.fSizeX = 200.f;   desc.fSizeY = 200.f;
        desc.fDepth = 0.3f;
        desc.vColor = _float4(0.f, 0.f, 0.f, 0.5f); // 단색 배경(반투명 검정)

        desc.fViewRange = 10.f;            // 내 주위 반경 10m (맵 크기와 무관하게 고정)
        desc.iMapLevelIndex = LEVEL_GAMEPLAY;
        desc.strMapLayerTag = L"Layer_Map";

        desc.fHeightMin = -5.f;                // 맵의 실제 최저~최고에 맞춰 조정
        desc.fHeightMax = 20.f;
        // vColorLow/High, fBlipScale, fMarkerSize 등은 기본값으로 충분

        if (FAILED(m_pGameInstance->Add_GameObject_ToLayer(
            LEVEL_STATIC, L"Prototype_GameObject_MiniMap",
            LEVEL_GAMEPLAY, L"Layer_UI_MiniMap", &desc)))
        {
            MSG_BOX("Failed to Add GameObject To Layer : MiniMap");
            return E_FAIL;
        }
    }

    // MapSelect (상점 대신 띄우는 맵 정보 + 스폰 선택 창)
    {
        CMapSelect::MAPSELECT_DESC desc;
        // 화면 중앙에 큼직하게 배치 (1280x720 기준)
        desc.fX = 340.f;   desc.fY = 90.f;
        desc.fSizeX = 600.f;   desc.fSizeY = 540.f;
        desc.fDepth = 0.4f;
        desc.vColor = _float4(0.05f, 0.06f, 0.10f, 0.85f); // 어두운 반투명 배경

        // 보여줄 맵 영역. 맵이 ×100 스케일이라 "반경 50m" = 5000 월드유닛.
        //  (오브젝트 1개가 ~50유닛이므로 50 으로는 한 오브젝트도 화면에 안 들어왔음)
        //  맵 전체가 안 보이면 이 값을 키우고, 너무 작게 보이면 줄인다. 
       // 중심은 런타임에 식탁(Table_1)으로 자동 설정되므로 이 값은 초기/폴백용.
        desc.vCenterWorld = _float2(0.f, 0.f);
        desc.fWorldRange = 70.f;   // 줌(반경). 식탁이 패널 폭의 ~60%로 보임.

        desc.iMapLevelIndex = LEVEL_GAMEPLAY;
        desc.strMapLayerTag = L"Layer_Map";
        desc.strTableModelTag = L"Prototype_Component_Model_Table_1"; // 식탁 모델 태그

        desc.fHeightMin = -5.f;                  // 맵 실제 최저~최고에 맞춰 조정
        desc.fHeightMax = 20.f;
        // vColorLow/High, fBlipScale, fMarkerSize 등은 기본값으로 충분

        if (FAILED(m_pGameInstance->Add_GameObject_ToLayer(
            LEVEL_STATIC, L"Prototype_GameObject_MapSelect",
            LEVEL_GAMEPLAY, L"Layer_UI_MapSelect", &desc)))
        {
            MSG_BOX("Failed to Add GameObject To Layer : MapSelect");
            return E_FAIL;
        }

        // 생성 직후엔 꺼둔다 (스코어보드부터 시작 → 맵선택 단계에서 켜짐)
        list<CGameObject*> msObjs = m_pGameInstance->Get_List(LEVEL_GAMEPLAY, L"Layer_UI_MapSelect");
        for (auto* p : msObjs)
            if (p) p->SetOnOff(false);
    }

    // Crosshair (에임)
    {
        CUI_Crosshair::CROSSHAIR_DESC desc;
        desc.fSizeX = 64.f;   desc.fSizeY = 64.f;   // 기본 크기 (fX/fY 는 내부에서 중앙 보정)
        desc.fDepth = 0.2f;                          // 미니맵(0.3)보다 앞
        desc.vColor = _float4(1.f, 1.f, 1.f, 1.f);

        desc.strTextureProtoTag = L"Prototype_Component_Texture_Crosshair";
        desc.iTextureLevelIndex = LEVEL_GAMEPLAY;
        desc.fExpandScale = 1.8f;    // 발사 시 1.8배까지 확장
        desc.fRecoverTime = 0.25f;   // 0.25초에 걸쳐 복귀

        CGameObject* pCrosshair = m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
            LEVEL_STATIC, L"Prototype_GameObject_UI_Crosshair",
            LEVEL_GAMEPLAY, L"Layer_UI_Crosshair", &desc);

        if (nullptr == pCrosshair)
        {
            MSG_BOX("Failed to Add GameObject To Layer : Crosshair");
            return E_FAIL;
        }

        // 컨트롤러에 연결 (발사 성공 시 On_Fire 호출용)
        m_pGameInstance->Get_Controller()->Set_Crosshair(
            static_cast<CUI_Crosshair*>(pCrosshair));
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
    Safe_Release(m_pMapRT);
    Safe_Release(m_pGameManager);
    __super::Free();
}