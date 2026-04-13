#include "Controller.h"
#include "Player_1rd.h"
#include "GameInstance.h"

IMPLEMENT_SINGLETON(CController)

CController::CController() : m_pGameInstance{ CGameInstance::GetInstance() }
{

}

void CController::Update_Controller(_float fTimeDelta)
{
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
        {
            m_pPlayer->Turn(MouseMove * fTimeDelta * 2.2f);
        }

        if (m_pGameInstance->Key_Pressing(DIK_W) && m_pGameInstance->Key_Pressing(DIK_S))
        {
            if (m_pGameInstance->Key_Pressing(DIK_A) && m_pGameInstance->Key_Pressing(DIK_D))
            {

            }
            else if (m_pGameInstance->Key_Pressing(DIK_A))
            {
                m_pPlayer->Move(DIR_LEFT, fTimeDelta);
            }
            else if (m_pGameInstance->Key_Pressing(DIK_D))
            {
                m_pPlayer->Move(DIR_RIGHT, fTimeDelta);
            }
        }
        else if (m_pGameInstance->Key_Pressing(DIK_W))
        {
            if (m_pGameInstance->Key_Pressing(DIK_A) && m_pGameInstance->Key_Pressing(DIK_D))
            {
                m_pPlayer->Move(DIR_FOWARD, fTimeDelta);
            }
            else if (m_pGameInstance->Key_Pressing(DIK_A))
            {
                m_pPlayer->Move(DIR_FOWARD, fTimeDelta * 0.7071f);
                m_pPlayer->Move(DIR_LEFT, fTimeDelta * 0.7071f);
            }
            else if (m_pGameInstance->Key_Pressing(DIK_D))
            {
                m_pPlayer->Move(DIR_FOWARD, fTimeDelta * 0.7071f);
                m_pPlayer->Move(DIR_RIGHT, fTimeDelta * 0.7071f);
            }
            else
            {
                m_pPlayer->Move(DIR_FOWARD, fTimeDelta);
            }
        }
        else if (m_pGameInstance->Key_Pressing(DIK_S))
        {
            if (m_pGameInstance->Key_Pressing(DIK_A) && m_pGameInstance->Key_Pressing(DIK_D))
            {
                m_pPlayer->Move(DIR_BACK, fTimeDelta);
            }
            else if (m_pGameInstance->Key_Pressing(DIK_A))
            {
                m_pPlayer->Move(DIR_BACK, fTimeDelta * 0.7071f);
                m_pPlayer->Move(DIR_LEFT, fTimeDelta * 0.7071f);
            }
            else if (m_pGameInstance->Key_Pressing(DIK_D))
            {
                m_pPlayer->Move(DIR_BACK, fTimeDelta * 0.7071f);
                m_pPlayer->Move(DIR_RIGHT, fTimeDelta * 0.7071f);
            }
            else
            {
                m_pPlayer->Move(DIR_BACK, fTimeDelta * 0.7071f);
            }
        }
        else if (m_pGameInstance->Key_Pressing(DIK_A) && m_pGameInstance->Key_Pressing(DIK_D))
        {

        }
        else if (m_pGameInstance->Key_Pressing(DIK_A))
        {
            m_pPlayer->Move(DIR_LEFT, fTimeDelta);
        }
        else if (m_pGameInstance->Key_Pressing(DIK_D))
        {
            m_pPlayer->Move(DIR_RIGHT, fTimeDelta);
        }
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