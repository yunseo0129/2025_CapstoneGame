#include "Controller.h"
#include "Player_1rd.h"
#include "GameInstance.h"
#include "NetworkClient.h"

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

void CController::Update_Input()
{
    for (int i = 0; i < KEYS_END; ++i)
        m_isKeyboardInput[i] = false;

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

unsigned char CController::Build_KeyBitFlags() const
{
    unsigned char flags = 0;
    if (m_isKeyboardInput[KEYS_W])     flags |= KEY_W;
    if (m_isKeyboardInput[KEYS_S])     flags |= KEY_S;
    if (m_isKeyboardInput[KEYS_A])     flags |= KEY_A;
    if (m_isKeyboardInput[KEYS_D])     flags |= KEY_D;
    if (m_isKeyboardInput[KEYS_SPACE]) flags |= KEY_SPACE;
    if (m_isKeyboardInput[KEYS_CTRL])  flags |= KEY_CTRL;
    return flags;
}

void CController::Predict_Local(_float fTimeDelta)
{
    auto* pNet = NetworkClient::GetInstance();
    if (!pNet->IsInGame()) return;
    if (m_pPlayer == nullptr || m_pPlayer->IsDead()) return;

    m_pPlayer->PredictMove(Build_KeyBitFlags(), m_fMouseYawThisFrame, fTimeDelta);
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

    unsigned char keyFlags = Build_KeyBitFlags();
    pNet->Send_Move(keyFlags, m_fAccumMouseYaw,
        m_pPlayer->Get_PredictedMatrixPtr());

    m_fAccumMouseYaw = 0.f;  // 전송 후 리셋
}

void CController::Apply_ServerEvents(_float fTimeDelta)
{
    if (m_pPlayer == nullptr || m_pPlayer->IsDead()) return;

    auto* pNet = NetworkClient::GetInstance();
    if (!pNet->IsInGame()) return;

    std::vector<NetPlayer::Event> events;
    pNet->PopAllPlayerEvents(events);

    int myId = pNet->GetMyId();

    for (auto& evt : events)
    {
        if (evt.type != NetPlayer::EventType::MOVED) continue;

        if (evt.id == myId)
        {
            m_pPlayer->Apply_ServerCorrection(evt.worldMatrix, fTimeDelta);
        }
        // 다른 플레이어의 MOVED는 후속 작업에서 처리
    }
}

void CController::Input_UI(_float fTimeDelta)
{
}

CController* CController::Create()
{
    return new CController();
}

void CController::Free()
{
    if (m_pPlayer != nullptr)
        Safe_Release(m_pPlayer);
}