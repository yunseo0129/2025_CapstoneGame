#include "GameInstance.h"

#include "Prototype_Manager.h"

IMPLEMENT_SINGLETON(CGameInstance)

CGameInstance::CGameInstance()
{

}


HRESULT CGameInstance::Initialize_Engine(const ENGINE_DESC& EngineDesc, EngineContext* _pcontext)
{
	m_pGraphic_Device = CGraphic_Device::Create(EngineDesc.hWnd, _pcontext);
	if (nullptr == m_pGraphic_Device)
		return E_FAIL;

	m_pPrototype_Manager = CPrototype_Manager::Create(EngineDesc.iNumLevels);
	if (nullptr == m_pPrototype_Manager)
		return E_FAIL;

	return S_OK;
}

void CGameInstance::Update_Engine(_float fTimeDelta)
{

}

HRESULT CGameInstance::Render_Begin(const _float4& vClearColor)
{
	m_pGraphic_Device->BeforeRender(vClearColor);

	return S_OK;
}

HRESULT CGameInstance::Draw()
{

	return S_OK;
}

HRESULT CGameInstance::Render_End()
{
	m_pGraphic_Device->AfterRender();

	return S_OK;
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

void CGameInstance::Set_Transform(CPipeLine::D3DTRANSFORMSTATE eState, _fmatrix TransformMatrix)
{
	if (nullptr == m_pPipeLine)
		return;

	return m_pPipeLine->Set_Transform(eState, TransformMatrix);
}

_matrix CGameInstance::Get_ViewProjMatrix()
{
	return m_pPipeLine->Get_ViewProjMatrix();
}

_matrix CGameInstance::Get_TransformMatrix(CPipeLine::D3DTRANSFORMSTATE eState)
{
	if (nullptr == m_pPipeLine)
		return XMMatrixIdentity();

	return m_pPipeLine->Get_TransformMatrix(eState);
}

_float4x4 CGameInstance::Get_TransformFloat4x4(CPipeLine::D3DTRANSFORMSTATE eState)
{
	if (nullptr == m_pPipeLine)
		return _float4x4();

	return m_pPipeLine->Get_TransformFloat4x4(eState);
}

const _float4* CGameInstance::Get_CamPosition() const
{
	if (nullptr == m_pPipeLine)
		return nullptr;

	return m_pPipeLine->Get_CamPosition();
}

_byte CGameInstance::Get_DIKeyState(_ubyte byKeyID)
{
	return m_pInput_Device->Get_DIKeyState(byKeyID);
}

_byte CGameInstance::Get_DIMouseState(Engine::MOUSEKEYSTATE eMouse)
{
	return m_pInput_Device->Get_DIMouseState(eMouse);
}

_long CGameInstance::Get_DIMouseMove(Engine::MOUSEMOVESTATE eMouseState)
{
	return m_pInput_Device->Get_DIMouseMove(eMouseState);
}

bool CGameInstance::Key_Pressing(int _iKey)
{
	return m_pInput_Device->Key_Pressing(_iKey);
}

bool CGameInstance::Key_Down(int _iKey)
{
	return m_pInput_Device->Key_Down(_iKey);
}

bool CGameInstance::Key_Up(int _iKey)
{
	return m_pInput_Device->Key_Up(_iKey);
}

bool CGameInstance::Mouse_Pressing(int _iKey)
{
	return m_pInput_Device->Mouse_Pressing(_iKey);
}

bool CGameInstance::Mouse_Down(int _iKey)
{
	return m_pInput_Device->Mouse_Down(_iKey);
}

bool CGameInstance::Mouse_Up(int _iKey)
{
	return m_pInput_Device->Mouse_Up(_iKey);
}

void CGameInstance::Release_Engine()
{
	CGameInstance::GetInstance()->Free();

	CGameInstance::GetInstance()->DestroyInstance();
}

void CGameInstance::Free()
{
	__super::Free();

	Safe_Release(m_pPrototype_Manager);
	Safe_Release(m_pGraphic_Device);
}