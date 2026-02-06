#include "GameInstance.h"

#include "Prototype_Manager.h"

IMPLEMENT_SINGLETON(CGameInstance)

CGameInstance::CGameInstance()
{

}


HRESULT CGameInstance::Initialize_Engine(const ENGINE_DESC& EngineDesc, EngineContext* _pcontext)
{
	m_pPrototype_Manager = CPrototype_Manager::Create(EngineDesc.iNumLevels);
	if (nullptr == m_pPrototype_Manager)
		return E_FAIL;

	m_pGraphic_Device = CGraphic_Device::Create(EngineDesc.hWnd, _pcontext);
	if (nullptr == m_pGraphic_Device)
		return E_FAIL;
	return S_OK;
}

void CGameInstance::Update_Engine(_float fTimeDelta)
{

}

HRESULT CGameInstance::Add_Prototype(_uint iLevelIndex, const _wstring& strPrototypeTag, CBase* pPrototype)
{
	if (nullptr == m_pPrototype_Manager)
		return E_FAIL;

	return m_pPrototype_Manager->Add_Prototype(iLevelIndex, strPrototypeTag, pPrototype);
}

CBase* CGameInstance::Clone_Prototype(Engine::PROTOTYPE eType, _uint iLevelIndex, const _wstring& strPrototypeTag, void* pArg)
{
	if (nullptr == m_pPrototype_Manager)
		return nullptr;

	return m_pPrototype_Manager->Clone_Prototype(eType, iLevelIndex, strPrototypeTag, pArg);
}