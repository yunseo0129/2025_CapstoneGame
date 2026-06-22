#include "Controller.h"
#include "Player_1rd.h"
#include "Player_Pig.h"
#include "GameInstance.h"
#include "NetworkClient.h"
#include "Defines.h"
#include "UI_Crosshair.h"

IMPLEMENT_SINGLETON(CController)

CController::CController() : m_pGameInstance{ CGameInstance::GetInstance() }
{
}

void CController::Update_Controller(_float fTimeDelta)
{
    Update_Input();
    Predict_Local(fTimeDelta);          // 1) 본인 캐릭터에 즉시 예측 이동 적용
    Send_InputPacket(fTimeDelta);       // 2) 키 입력 → 서버로 패킷 전송
    Apply_ServerEvents(fTimeDelta);     // 3) 서버 결과로 보정
    Input_UI(fTimeDelta);
}

void CController::Set_Player(CPlayer_1rd* _pPlayer)
{
    if (m_pPlayer != nullptr)
        Safe_Release(m_pPlayer);
	m_pPlayer = _pPlayer;
	Safe_AddRef(m_pPlayer);
}

void CController::Set_Crosshair(CUI_Crosshair* _pCrosshair)
{
    // 크로스헤어는 Object_Manager 가 소유/해제하므로 여기선 AddRef 하지 않는다.
    m_pCrosshair = _pCrosshair;
}

void CController::Input_Player(_float fTimeDelta)
{
    // 상점 등 메뉴가 열려 있으면 시점/이동/사격 입력을 전부 무시한다.
    if (m_bBlockInput)
        return;

	if (m_pPlayer != nullptr && !m_pPlayer->IsDead() && !m_pPlayer->Get_Die())
	{
        _long      MouseMove = {};
        if (MouseMove = m_pGameInstance->Get_DIMouseMove(Engine::DIMS_X))
            m_pPlayer->TurnYaw(MouseMove * fTimeDelta * m_fMouseSensitive);
        if (MouseMove = m_pGameInstance->Get_DIMouseMove(Engine::DIMS_Y))
            m_pPlayer->TurnPitch(MouseMove * fTimeDelta * m_fMouseSensitive);

        _float fLook = 0;
        _float fRight = 0;

        if (m_isKeyboardInput[KEYS_W]) fLook = 1.f;
        if (m_isKeyboardInput[KEYS_S]) fLook = -1.f;
        if (m_isKeyboardInput[KEYS_A]) fRight = -1.f;
        if (m_isKeyboardInput[KEYS_D]) fRight = 1.f;


        m_pPlayer->Move(fLook, fRight, fTimeDelta);

        if (m_isKeyboardInput[KEYS_SPACE])
        {
            m_pPlayer->Jump(fTimeDelta);
        }
        if (m_isKeyboardInput[KEYS_CTRL])
        {
            m_pPlayer->Crouch(fTimeDelta);
        }
        if (m_isKeyboardInput[KEYS_SHIFT])
        {
            m_pPlayer->Run(fTimeDelta);
        }
        if (m_isKeyboardInput[KEYS_R])
        {
            m_pPlayer->Reload(fTimeDelta);
        }
        if (m_isMouseInput[MOUSE_LB] && !m_isPreMouseInput[MOUSE_LB])
        {
            if (m_pPlayer->Shoot(fTimeDelta))
            {
                if (m_pCrosshair != nullptr)
                    m_pCrosshair->On_Fire();
            }
        }
	}
}

void CController::Update_Input()
{
    for (int i = 0; i < MOUSE_END; ++i)
        m_isPreMouseInput[i] = m_isMouseInput[i];

    for (int i = 0; i < KEYS_END; ++i)
        m_isKeyboardInput[i] = false;

    for (int i = 0; i < MOUSE_END; ++i)
        m_isMouseInput[i] = false;

    if (m_pGameInstance->Key_Pressing(DIK_A))
        m_isKeyboardInput[KEYS_A] = true;
    if (m_pGameInstance->Key_Pressing(DIK_S))
        m_isKeyboardInput[KEYS_S] = true;
    if (m_pGameInstance->Key_Pressing(DIK_D))
        m_isKeyboardInput[KEYS_D] = true;
    if (m_pGameInstance->Key_Pressing(DIK_W))
        m_isKeyboardInput[KEYS_W] = true;
    if (m_pGameInstance->Key_Pressing(DIK_SPACE))
        m_isKeyboardInput[KEYS_SPACE] = true;
    if (m_pGameInstance->Key_Pressing(DIK_LCONTROL))
        m_isKeyboardInput[KEYS_CTRL] = true;
    if (m_pGameInstance->Key_Pressing(DIK_LSHIFT))
        m_isKeyboardInput[KEYS_SHIFT] = true;
    if (m_pGameInstance->Key_Pressing(DIK_R))
        m_isKeyboardInput[KEYS_R] = true;
    if (m_pGameInstance->Get_DIMouseState(Engine::DIM_LB))
        m_isMouseInput[MOUSE_LB] = true;

    // 마우스 Yaw/Pitch (매 프레임 수집)
    _long mouseX = m_pGameInstance->Get_DIMouseMove(Engine::DIMS_X);
    m_fMouseYawThisFrame = 0.f;
    if (mouseX != 0)
    {
        m_fMouseYawThisFrame = mouseX * m_fMouseSensitive;
        m_fAccumMouseYaw += m_fMouseYawThisFrame;
    }

    _long mouseY = m_pGameInstance->Get_DIMouseMove(Engine::DIMS_Y);
    m_fMousePitchThisFrame = 0.f;
    if (mouseY != 0)
        m_fMousePitchThisFrame = mouseY * m_fMouseSensitive;
}

unsigned short CController::Build_KeyBitFlags() const
{
    unsigned short flags = 0;
    if (m_isKeyboardInput[KEYS_W])     flags |= KEY_W;
    if (m_isKeyboardInput[KEYS_S])     flags |= KEY_S;
    if (m_isKeyboardInput[KEYS_A])     flags |= KEY_A;
    if (m_isKeyboardInput[KEYS_D])     flags |= KEY_D;
    if (m_isKeyboardInput[KEYS_SPACE]) flags |= KEY_SPACE;
    if (m_isKeyboardInput[KEYS_CTRL])  flags |= KEY_CTRL;
    if (m_isKeyboardInput[KEYS_SHIFT]) flags |= KEY_SHIFT;
    if (m_isKeyboardInput[KEYS_R])     flags |= KEY_R;
    if (m_isMouseInput[MOUSE_LB])      flags |= KEY_MOUSE_LB;
    return flags;
}

void CController::Predict_Local(_float fTimeDelta)
{
    auto* pNet = NetworkClient::GetInstance();
    if (!pNet->IsInGame()) return;
    if (m_pPlayer == nullptr || m_pPlayer->IsDead()) return;

    unsigned short keyFlags = Build_KeyBitFlags();

    float fLook  = 0.f;
    float fRight = 0.f;
    if (keyFlags & KEY_W) fLook  += 1.f;
    if (keyFlags & KEY_S) fLook  -= 1.f;
    if (keyFlags & KEY_A) fRight -= 1.f;
    if (keyFlags & KEY_D) fRight += 1.f;
    m_pPlayer->Move(fLook, fRight, 0.f);

    if (keyFlags & KEY_SPACE) m_pPlayer->Jump(fTimeDelta);
    if (keyFlags & KEY_CTRL)  m_pPlayer->Crouch(fTimeDelta);
    if (keyFlags & KEY_SHIFT) m_pPlayer->Run(fTimeDelta);

    if (m_fMouseYawThisFrame != 0.f)
        m_pPlayer->TurnYaw(m_fMouseYawThisFrame * fTimeDelta * 2.2f);

    m_pPlayer->TurnPitch(m_fMousePitchThisFrame * fTimeDelta * 2.2f);
}

void CController::Send_InputPacket(_float fTimeDelta)
{
    auto* pNet = NetworkClient::GetInstance();
    if (!pNet->IsInGame()) return;

    // 전송 주기 제한 (초당 20회)
    m_fSendTimer += fTimeDelta;
    if (m_fSendTimer < m_fSendInterval) return;
    m_fSendTimer = 0.f;

    if (m_pPlayer == nullptr || m_pPlayer->IsDead()) return;

    unsigned short keyFlags = Build_KeyBitFlags();
    pNet->Send_Move(keyFlags, m_fAccumMouseYaw,
        m_pPlayer->Get_PredictedMatrixPtr());

    m_fAccumMouseYaw = 0.f;  // 전송 후 리셋
}

void CController::Apply_ServerEvents(_float fTimeDelta)
{
    auto* pNet = NetworkClient::GetInstance();
    if (!pNet->IsInGame()) return;

    // 플레이어 이동 이벤트 (기존)
    std::vector<NetPlayer::Event> events;
    pNet->PopAllPlayerEvents(events);

    int myId = pNet->GetMyId();

    for (auto& evt : events)
    {
        if (evt.id == myId)
        {
            if (evt.type == NetPlayer::EventType::MOVED && m_pPlayer && !m_pPlayer->IsDead() && !m_pPlayer->Get_Die())
            {
                m_pPlayer->Apply_ServerCorrection(evt.worldMatrix, fTimeDelta);

                // 서버가 에코한 keyInput 상승 에지 → 1인칭 애니메이션
                unsigned short risingEdge = (~m_prevServerKeyInput) & evt.keyInput;
                m_prevServerKeyInput = evt.keyInput;

                if (!m_bBlockInput)
                {
                    if (risingEdge & KEY_R)
                        m_pPlayer->Reload(fTimeDelta);

                    if (risingEdge & KEY_MOUSE_LB)
                    {
                        if (m_pPlayer->Shoot(fTimeDelta))
                            if (m_pCrosshair) m_pCrosshair->On_Fire();
                    }
                }
            }
            continue;
        }

        switch (evt.type)
        {
        case NetPlayer::EventType::ADDED:
            Spawn_OtherPlayer(evt.id, evt.worldMatrix);
            break;
        case NetPlayer::EventType::MOVED:
            Move_OtherPlayer(evt.id, evt.worldMatrix, evt.keyInput);
            break;
        case NetPlayer::EventType::REMOVED:
            Remove_OtherPlayer(evt.id);
            break;
        }
    }

}

void CController::Spawn_OtherPlayer(int id, const float* worldMatrix)
{
    if (m_otherPlayers.count(id)) return;

    CPlayer_Pig::PLAYER_PIG_DESC desc;
    desc.strModelTag     = L"Prototype_Component_Pig_3rd";
    desc.iModelLevelIndex = LEVEL_GAMEPLAY;
    desc.fSpeedPerSec    = 1.f;
    desc.fRotationPerSec = 1.f;
    desc.vRotation       = _float3(0.f, 0.f, 0.f);
    desc.vPos            = _float3(worldMatrix[12], worldMatrix[13], worldMatrix[14]);

    CGameObject* pObj = m_pGameInstance->Add_GameObject_ToLayer_Return_Obj(
        LEVEL_GAMEPLAY, TEXT("Prototype_GameObject_Player_Pig"),
        LEVEL_GAMEPLAY, TEXT("Layer_Other_Player"), &desc);

    if (!pObj) return;

    CPlayer_Pig* pPig = static_cast<CPlayer_Pig*>(pObj);
    pPig->Apply_NetworkMatrix(worldMatrix);
    m_otherPlayers[id] = pPig;
}

void CController::Remove_OtherPlayer(int id)
{
    auto it = m_otherPlayers.find(id);
    if (it == m_otherPlayers.end()) return;
    it->second->SetDead();
    m_otherPlayers.erase(it);
}

void CController::Move_OtherPlayer(int id, const float* worldMatrix, unsigned short keyInput)
{
    auto it = m_otherPlayers.find(id);
    if (it == m_otherPlayers.end()) return;
    it->second->Apply_NetworkMatrix(worldMatrix, keyInput);
}

void CController::Clear_OtherPlayers()
{
    for (auto& pair : m_otherPlayers)
        pair.second->SetDead();
    m_otherPlayers.clear();
}

void CController::Input_UI(_float fTimeDelta)
{
    // SHIFT/R/MOUSE_LB는 Update_Input에서 이미 읽음 (패킷 전송 전 필요)
    // UI 전용 로직이 있으면 여기에 추가
}

CController* CController::Create()
{
    return new CController();
}

void CController::Free()
{
    if (m_pPlayer != nullptr)
        Safe_Release(m_pPlayer);
    // m_otherPlayers의 CPlayer_Pig*는 Layer가 소유하므로 해제하지 않는다
    m_otherPlayers.clear();
}