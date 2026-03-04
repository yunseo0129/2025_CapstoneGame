#include "Camera_FPV.h"
#include "GameInstance.h"

CCamera_FPV::CCamera_FPV(EngineContext* pContext) : CCamera(pContext)
{
}

CCamera_FPV::CCamera_FPV(const CCamera_FPV& Prototype) : CCamera(Prototype)
{
}

CCamera_FPV::~CCamera_FPV()
{
}

HRESULT CCamera_FPV::Initialize(void* pArg)
{
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    FPV_CAMERA_DESC* pDesc = static_cast<FPV_CAMERA_DESC*>(pArg);

    m_fCamMouseSensor = pDesc->fCamMouseSensor;
	m_fCamSpeedPerSec = pDesc->fCamSpeedPerSec;

    return S_OK;
}

void CCamera_FPV::Priority_Update(_float fTimeDelta)
{

}

void CCamera_FPV::Update(_float fTimeDelta)
{
    if (m_bOnOff == true)
    {
		// 마우스 이동량에 따른 카메라 회전
        _long		MouseMove = {};
        if (MouseMove = m_pGameInstance->Get_DIMouseMove(Engine::DIMS_X))
        {
            m_pTransformCom->Turn(XMVectorSet(0.f, 1.f, 0.f, 0.f), MouseMove * fTimeDelta * m_fCamMouseSensor * 2.2f);
        }

        if (MouseMove = m_pGameInstance->Get_DIMouseMove(Engine::DIMS_Y))
        {
            m_pTransformCom->Turn(m_pTransformCom->Get_State(CTransform::STATE_RIGHT), MouseMove * fTimeDelta * m_fCamMouseSensor * 2.2f);
        }

		// 키보드 입력에 따른 카메라 이동
        if (m_pGameInstance->Key_Pressing(DIK_W) && m_pGameInstance->Key_Pressing(DIK_S))
        {
            if (m_pGameInstance->Key_Pressing(DIK_A) && m_pGameInstance->Key_Pressing(DIK_D))
            {

            }
            else if (m_pGameInstance->Key_Pressing(DIK_A))
            {
                m_pTransformCom->Go_Left(fTimeDelta * m_fCamSpeedPerSec);
            }
            else if (m_pGameInstance->Key_Pressing(DIK_D))
            {
                m_pTransformCom->Go_Right(fTimeDelta * m_fCamSpeedPerSec);
			}
        }
        else if (m_pGameInstance->Key_Pressing(DIK_W))
        {
            if (m_pGameInstance->Key_Pressing(DIK_A) && m_pGameInstance->Key_Pressing(DIK_D))
            {
                m_pTransformCom->Go_Straight(fTimeDelta * m_fCamSpeedPerSec);
            }
            else if (m_pGameInstance->Key_Pressing(DIK_A))
            {
				m_pTransformCom->Go_Straight(fTimeDelta * m_fCamSpeedPerSec * 0.7071f);
                m_pTransformCom->Go_Left(fTimeDelta * m_fCamSpeedPerSec * 0.7071f);
            }
            else if (m_pGameInstance->Key_Pressing(DIK_D))
            {
                m_pTransformCom->Go_Straight(fTimeDelta * m_fCamSpeedPerSec * 0.7071f);
                m_pTransformCom->Go_Right(fTimeDelta * m_fCamSpeedPerSec * 0.7071f);
            }
            else
            {
                m_pTransformCom->Go_Straight(fTimeDelta * m_fCamSpeedPerSec);
            }
        }
        else if (m_pGameInstance->Key_Pressing(DIK_S))
        {
            if (m_pGameInstance->Key_Pressing(DIK_A) && m_pGameInstance->Key_Pressing(DIK_D))
            {
                m_pTransformCom->Go_Backward(fTimeDelta * m_fCamSpeedPerSec);
            }
            else if (m_pGameInstance->Key_Pressing(DIK_A))
            {
                m_pTransformCom->Go_Backward(fTimeDelta * m_fCamSpeedPerSec * 0.7071f);
                m_pTransformCom->Go_Left(fTimeDelta * m_fCamSpeedPerSec * 0.7071f);
            }
            else if (m_pGameInstance->Key_Pressing(DIK_D))
            {
                m_pTransformCom->Go_Backward(fTimeDelta * m_fCamSpeedPerSec * 0.7071f);
                m_pTransformCom->Go_Right(fTimeDelta * m_fCamSpeedPerSec * 0.7071f);
            }
            else
            {
                m_pTransformCom->Go_Backward(fTimeDelta * m_fCamSpeedPerSec);
            }
		}
        else if (m_pGameInstance->Key_Pressing(DIK_A) && m_pGameInstance->Key_Pressing(DIK_D))
        {
            
        }
        else if (m_pGameInstance->Key_Pressing(DIK_A))
        {
            m_pTransformCom->Go_Left(fTimeDelta * m_fCamSpeedPerSec);
		}
        else if (m_pGameInstance->Key_Pressing(DIK_D))
        {
            m_pTransformCom->Go_Right(fTimeDelta * m_fCamSpeedPerSec);
		}

        if (m_pGameInstance->Key_Pressing(DIK_SPACE))
        {
            m_pTransformCom->Go_Up(fTimeDelta * m_fCamSpeedPerSec);
        }
        else if (m_pGameInstance->Key_Pressing(DIK_LCONTROL))
        {
            m_pTransformCom->Go_Up(-fTimeDelta * m_fCamSpeedPerSec);
        }
    }

    // 파이프라인 복구 후 아래 코드 주석 해제
    // __super::Compute_PipeLineMatrices();
}

void CCamera_FPV::Late_Update(_float fTimeDelta)
{

}

CCamera_FPV* CCamera_FPV::Create(EngineContext* pContext)
{
    CCamera_FPV* pInstance = new CCamera_FPV(pContext);

    if (FAILED(pInstance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : CCamera_FPV");
        Safe_Release(pInstance);
    }

    return pInstance;
}

CGameObject* CCamera_FPV::Clone(void* pArg)
{
    CCamera_FPV* pInstance = new CCamera_FPV(*this);

    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : CCamera_FPV");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CCamera_FPV::Free()
{
    __super::Free();
}
