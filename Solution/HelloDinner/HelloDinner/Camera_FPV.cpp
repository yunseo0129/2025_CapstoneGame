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
	return E_NOTIMPL;
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

CGameObject* CCamera_FPV::Clone(void* pArg)
{
	return nullptr;
}

void CCamera_FPV::Free()
{
}
