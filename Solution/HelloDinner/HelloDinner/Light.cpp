#include "Light.h"

CLight::CLight(EngineContext* pContext)
	: m_pContext{ pContext }
{
	Safe_AddRef(m_pContext);
}

HRESULT CLight::Initialize(const LIGHT_DESC& LightDesc)
{
	m_LightDesc = LightDesc;

	return S_OK;
}

CLight* CLight::Create(EngineContext* pContext, const LIGHT_DESC& LightDesc)
{
	CLight* pInstance = new CLight(pContext);

	if (FAILED(pInstance->Initialize(LightDesc)))
	{
		MSG_BOX("Failed to Created : CLight");
		Safe_Release(pInstance);
	}

	return pInstance;
}


void CLight::Free()
{
	__super::Free();
	Safe_Release(m_pContext);
}
