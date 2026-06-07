#include "ContainerObj.h"

CContainerObj::CContainerObj(EngineContext* pContext)
	: CGameObject{ pContext }
{
}

CContainerObj::CContainerObj(const CContainerObj& Prototype)
	: CGameObject{ Prototype.m_pContext }
{

}

HRESULT CContainerObj::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CContainerObj::Initialize(void* pArg)
{
	CONTAINEROBJ_DESC* pDesc = static_cast<CONTAINEROBJ_DESC*>(pArg);

	m_PartObjects.resize(pDesc->iNumPartObj);

	if (FAILED(__super::Initialize(pDesc)))
		return E_FAIL;

	return S_OK;
}

void CContainerObj::Priority_Update(_float fTimeDelta)
{
	for (auto& pPartObj : m_PartObjects)
	{
		if (nullptr != pPartObj && pPartObj->GetOnOff())
			pPartObj->Priority_Update(fTimeDelta);
	}

}

void CContainerObj::Update(_float fTimeDelta)
{
	for (auto& pPartObj : m_PartObjects)
	{
		if (nullptr != pPartObj && pPartObj->GetOnOff())
			pPartObj->Update(fTimeDelta);
	}
}

void CContainerObj::Late_Update(_float fTimeDelta)
{
	for (auto& pPartObj : m_PartObjects)
	{
		if (nullptr != pPartObj && pPartObj->GetOnOff())
			pPartObj->Late_Update(fTimeDelta);
	}
}

void CContainerObj::Render(ID3D12GraphicsCommandList* _commandList)
{

}

void CContainerObj::ShadowRender(ID3D12GraphicsCommandList* _commandList)
{

}

void CContainerObj::Free()
{
	__super::Free();

	for (auto& pPartObj : m_PartObjects)
		Safe_Release(pPartObj);

	m_PartObjects.clear();
}
