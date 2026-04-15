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
   
}

void CCamera_FPV::Late_Update(_float fTimeDelta)
{

}

void CCamera_FPV::Set_WorldMatrix(_float4x4 _mat)
{
    m_pTransformCom->Set_WorldMatrix(_mat);
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
