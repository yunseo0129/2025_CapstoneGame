#include "PartObj.h"

CPartObj::CPartObj(EngineContext* pContext)
	: CGameObject{ pContext }
{

}

CPartObj::CPartObj(const CPartObj& Prototype)
	: CGameObject{ Prototype.m_pContext }
{

}

HRESULT CPartObj::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CPartObj::Initialize(void* pArg)
{
	PARTOBJ_DESC* pDesc = static_cast<PARTOBJ_DESC*>(pArg);

	m_pParentMatrix = pDesc->pParentMatrix;

	if (FAILED(__super::Initialize(pDesc)))
		return E_FAIL;

	return S_OK;
}

void CPartObj::Priority_Update(_float fTimeDelta)
{
}

void CPartObj::Update(_float fTimeDelta)
{
}

void CPartObj::Late_Update(_float fTimeDelta)
{
}

void CPartObj::Render(ID3D12GraphicsCommandList* _commandList)
{

}

void CPartObj::Free()
{
	__super::Free();


}
