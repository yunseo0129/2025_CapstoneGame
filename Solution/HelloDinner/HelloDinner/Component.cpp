#include "Component.h"
#include "GameInstance.h"

CComponent::CComponent(EngineContext* _context)
	: m_pContext{ _context }
	, m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pContext->device);
	Safe_AddRef(m_pContext->cmdList);
	Safe_AddRef(m_pContext->cmdQueue);
	Safe_AddRef(m_pContext->dsvHeap);
	Safe_AddRef(m_pContext->rtvHeap);
	Safe_AddRef(m_pContext->srvHeap);
	Safe_AddRef(m_pGameInstance);
}

CComponent::CComponent(const CComponent& Prototype)
	: m_pContext{ Prototype.m_pContext }
	, m_pGameInstance{ Prototype.m_pGameInstance }
{
	Safe_AddRef(m_pContext->cmdList);
	Safe_AddRef(m_pContext->device);
	Safe_AddRef(m_pContext->dsvHeap);
	Safe_AddRef(m_pContext->cmdQueue);
	Safe_AddRef(m_pContext->rtvHeap);
	Safe_AddRef(m_pContext->srvHeap);
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
	Safe_Release(m_pContext->cmdList);
	Safe_Release(m_pContext->device);
	Safe_Release(m_pContext->dsvHeap);
	Safe_Release(m_pContext->cmdQueue);
	Safe_Release(m_pContext->rtvHeap);
	Safe_Release(m_pContext->srvHeap);
	Safe_Release(m_pGameInstance);
	__super::Free();
}
