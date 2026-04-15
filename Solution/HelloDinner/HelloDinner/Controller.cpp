#include "Controller.h"
#include "Player_1rd.h"
#include "GameInstance.h"

IMPLEMENT_SINGLETON(CController)

CController::CController() : m_pGameInstance{ CGameInstance::GetInstance() }
{

}

void CController::Update_Controller(_float fTimeDelta)
{
    Update_Input();
	Input_Player(fTimeDelta);
	Input_UI(fTimeDelta);
}

void CController::Set_Player(CPlayer_1rd* _pPlayer)
{
    if (m_pPlayer != nullptr)
        Safe_Release(m_pPlayer);
	m_pPlayer = _pPlayer;
	Safe_AddRef(m_pPlayer);
}

void CController::Input_Player(_float fTimeDelta)
{
	if (m_pPlayer != nullptr && !m_pPlayer->IsDead())
	{
        _long      MouseMove = {};
        if (MouseMove = m_pGameInstance->Get_DIMouseMove(Engine::DIMS_X))
            m_pPlayer->TurnYaw(MouseMove * fTimeDelta * m_fMouseSensitive);
        if (MouseMove = m_pGameInstance->Get_DIMouseMove(Engine::DIMS_Y))
            m_pPlayer->TurnPitch(MouseMove * fTimeDelta * m_fMouseSensitive);

        XMFLOAT2 input = { 0.f, 0.f };

        if (m_isKeyboardInput[KEYS_W]) input.y += 1.f;
        if (m_isKeyboardInput[KEYS_S]) input.y -= 1.f;
        if (m_isKeyboardInput[KEYS_A]) input.x -= 1.f;
        if (m_isKeyboardInput[KEYS_D]) input.x += 1.f;

        float len = sqrtf(input.x * input.x + input.y * input.y);

        if (len > 0.0f)
        {
            input.x /= len;
            input.y /= len;

            if (input.y > 0)     m_pPlayer->Move(DIR_FOWARD, fTimeDelta * input.y);
            if (input.y < 0)     m_pPlayer->Move(DIR_BACK, fTimeDelta * -input.y);
            if (input.x < 0)     m_pPlayer->Move(DIR_LEFT, fTimeDelta * -input.x);
            if (input.x > 0)     m_pPlayer->Move(DIR_RIGHT, fTimeDelta * input.x);
        }

        if (m_isKeyboardInput[KEYS_SPACE])
        {
            m_pPlayer->Jump(fTimeDelta);
        }
        if (m_isKeyboardInput[KEYS_CTRL])
        {
            m_pPlayer->Crouch(fTimeDelta);
        }
	}
}

void CController::Input_UI(_float fTimeDelta)
{
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