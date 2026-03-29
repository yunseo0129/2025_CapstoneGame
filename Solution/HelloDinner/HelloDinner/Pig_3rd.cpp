#include "Pig_3rd.h"
#include "Transform.h"
#include "GameInstance.h"
#include "Model.h"

CPig_3rd::CPig_3rd(EngineContext* pContext)
	: CPlayer_3rd(pContext)
{
}

CPig_3rd::CPig_3rd(const CPig_3rd& Prototype)
	: CPlayer_3rd(Prototype.m_pContext)
{
}

HRESULT CPig_3rd::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CPig_3rd::Initialize(void* pArg)
{
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	m_iState = STATE_IDLE;
	m_pModelCom->SetUp_Animation(0, true);

	return S_OK;
}

void CPig_3rd::Priority_Update(_float fTimeDelta)
{
	__super::Priority_Update(fTimeDelta);
}

void CPig_3rd::Update(_float fTimeDelta)
{
	m_pModelCom->Play_Animation(fTimeDelta);
	
	__super::Update(fTimeDelta);
}

void CPig_3rd::Late_Update(_float fTimeDelta)
{
	__super::Late_Update(fTimeDelta);
}

void CPig_3rd::Render(ID3D12GraphicsCommandList* _commandList)
{
	__super::Render(_commandList);
}

HRESULT CPig_3rd::Ready_Components()
{
	if (FAILED(__super::Ready_Components()))
		return E_FAIL;

	return S_OK;
}

CPig_3rd* CPig_3rd::Create(EngineContext* pContext)
{
	CPig_3rd* pInstance = new CPig_3rd(pContext);
	if (FAILED(pInstance->Initialize_Prototype()))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Create : CPig_3rd");
	}
	return pInstance;
}

CGameObject* CPig_3rd::Clone(void* pArg)
{
	CPig_3rd* pInstance = new CPig_3rd(*this);
	if (FAILED(pInstance->Initialize(pArg)))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Clone : CPig_3rd");
	}
	return pInstance;
}

void CPig_3rd::Free()
{
	__super::Free();
}