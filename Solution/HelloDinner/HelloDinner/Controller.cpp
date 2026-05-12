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