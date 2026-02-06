#include "Component.h"
#include "GameInstance.h"

CComponent::CComponent(EngineContext* _context)
	: m_pContext{ _context }
	, m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);
}

CComponent::CComponent(const CComponent& Prototype)
	: m_pContext{ Prototype.m_pContext }
	, m_pGameInstance{ Prototype.m_pGameInstance }
{
	Safe_AddRef(m_pGameInstance);
} 

HRESULT CComponent::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CComponent::Initialize(void* pArg)
{
	return S_OK;
}

void CComponent::Free()
{
	__super::Free();

	Safe_Release(m_pGameInstance);
}
