#include "GameInstance.h"

#include "Graphic_Device.h"
#include "Renderer.h"
#include "Input_Device.h"
#include "Timer_Manager.h"
#include "Level_Manager.h"
#include "Object_Manager.h"
#include "Prototype_Manager.h"
#include "Load_Manager.h"
#include "Shader_Manager.h"

#include "Camera.h"

IMPLEMENT_SINGLETON(CGameInstance)

CGameInstance::CGameInstance()
{

}

// ------------------------------------------------------------------------
// GameInstance
// ------------------------------------------------------------------------
HRESULT CGameInstance::Initialize_Engine(const ENGINE_DESC& EngineDesc, EngineContext* _pcontext)
{
	m_pGraphic_Device = CGraphic_Device::Create(EngineDesc.hWnd, _pcontext);
	if (nullptr == m_pGraphic_Device)
		return E_FAIL;

	m_pInput_Device = CInput_Device::Create(EngineDesc.hInstance, EngineDesc.hWnd);
	if (nullptr == m_pInput_Device)
		return E_FAIL;

	m_pTimer_Manager = CTimer_Manager::Create();
	if (nullptr == m_pTimer_Manager)
		return E_FAIL;

	m_pLevel_Manager = CLevel_Manager::Create();
	if (nullptr == m_pLevel_Manager)
		return E_FAIL;

	m_pPrototype_Manager = CPrototype_Manager::Create(EngineDesc.iNumLevels);
	if (nullptr == m_pPrototype_Manager)
		return E_FAIL;

	m_pObject_Manager = CObject_Manager::Create(EngineDesc.iNumLevels);
	if (nullptr == m_pObject_Manager)
		return E_FAIL;


	m_pShader_Manager = CShader_Manager::Create(_pcontext->device);

	m_pRenderer = CRenderer::Create(_pcontext->device, _pcontext->cmdList);
	if (nullptr == m_pRenderer)
		return E_FAIL;

	m_pLoad_Manager = CLoad_Manager::Create();
	if (nullptr == m_pLoad_Manager)
		return E_FAIL;


	// 테스트용
	// Camera.h 변경해야할 것들
	// 1. abstract 설정
	// 2. Clone = 0;
	// 3. 생성자, 소멸자 protected로 변경
	m_pCamera = new CCamera(_pcontext);

	CCamera::CAMERA_DESC CameraDesc;
	CameraDesc.vEye = { 0.f, 5.f, 10.f };
	CameraDesc.vAt = { 0.f, 0.f, 0.f };
	CameraDesc.fFovy = XMConvertToRadians(45.f);
	CameraDesc.fAspect = static_cast<_float>(EngineDesc.iViewportWidth) / EngineDesc.iViewportHeight;
	CameraDesc.fNear = 0.1f;
	CameraDesc.fFar = 100.f;

	if (FAILED(m_pCamera->Initialize(&CameraDesc)))
		return E_FAIL;

	return S_OK;
}

void CGameInstance::Update_Engine(_float fTimeDelta)
{
	m_pCamera->Update(fTimeDelta);
	m_pObject_Manager->Late_Update(fTimeDelta);
}

HRESULT CGameInstance::Render_Begin(const _float4& vClearColor)
{
	m_pGraphic_Device->BeforeRender(vClearColor);
	m_pCommandList = m_pGraphic_Device->GetCommandList ();

	
	Set_RootSignature(m_pCommandList.Get());

	
	return S_OK;
}

HRESULT CGameInstance::Draw()
{
	m_pCamera->Bind_CameraBuffer ( m_pCommandList.Get () , RootParameterIndex::Camera );
	m_pRenderer->Draw_RenderObject ( m_pCommandList.Get () );
	return S_OK;
}

HRESULT CGameInstance::Render_End()
{
	m_pGraphic_Device->AfterRender();

	return S_OK;
}

void CGameInstance::Clear(_int iLevelID)
{
	/* 특정 레벨용 객체들을 지운다. */
	m_pObject_Manager->Clear(iLevelID);

	m_pPrototype_Manager->Clear(iLevelID);
}

// ------------------------------------------------------------------------
// Input_Device
// ------------------------------------------------------------------------

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


void CGameInstance::ResetCmdList ()
{
	if ( nullptr == m_pGraphic_Device )
		return;

	m_pGraphic_Device->ResetCmdList ();
}

void CGameInstance::CloseCmdList ()
{
	if ( nullptr == m_pGraphic_Device )
		return;

	m_pGraphic_Device->CloseCmdList ();
}

// ------------------------------------------------------------------------
// Timer_Manager
// ------------------------------------------------------------------------

_float CGameInstance::Get_TimeDelta(const _wstring& strTimerTag)
{
	if (nullptr == m_pTimer_Manager)
		return 0.f;

	return m_pTimer_Manager->Get_TimeDelta(strTimerTag);
}

HRESULT CGameInstance::Add_Timer(const _wstring& strTimerTag)
{
	if (nullptr == m_pTimer_Manager)
		return E_FAIL;

	return m_pTimer_Manager->Add_Timer(strTimerTag);
}

void CGameInstance::Update_TimeDelta(const _wstring& strTimerTag)
{
	if (nullptr == m_pTimer_Manager)
		return;

	return m_pTimer_Manager->Update_TimeDelta(strTimerTag);
}


// ------------------------------------------------------------------------
// Load_Device
// ------------------------------------------------------------------------

HRESULT CGameInstance::Open_File(const wchar_t* filename)
{
	return m_pLoad_Manager->Open_File(filename);
}

void CGameInstance::Close_File()
{
	m_pLoad_Manager->Close_File();
}

void CGameInstance::Read_File(_char& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(size_t& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_int& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_uint& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_float& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_float4& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_float4x4& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(VTXMESH& read)
{
	m_pLoad_Manager->Read_File(read);
}

/*
void CGameInstance::Read_File(VTXANIMMESH& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(KEYFRAME& read)
{
	m_pLoad_Manager->Read_File(read);
}
*/

void CGameInstance::Read_File(char(&read)[MAX_PATH])
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_tchar(&read)[MAX_PATH])
{
	m_pLoad_Manager->Read_File(read);
}

// ------------------------------------------------------------------------
// Level_Manager
// ------------------------------------------------------------------------

HRESULT CGameInstance::Open_Level(_int iLevelIndex, CLevel* pNewLevel)
{
	if (nullptr == m_pLevel_Manager)
		return E_FAIL;

	return m_pLevel_Manager->Open_Level(iLevelIndex, pNewLevel);
}

// ------------------------------------------------------------------------
// Prototype_Manager
// ------------------------------------------------------------------------

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

void CGameInstance::ReleaseUploadBuffers ( _uint iLevelIndex )
{
	if ( nullptr == m_pPrototype_Manager )
		return;

	m_pPrototype_Manager->ReleaseUploadBuffers ( iLevelIndex );
}

// ------------------------------------------------------------------------
// Object_Manager
// ------------------------------------------------------------------------

HRESULT CGameInstance::Add_GameObject_ToLayer(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag, _uint iLevelIndex, const _wstring& strLayerTag, void* pArg)
{
	if (nullptr == m_pObject_Manager)
		return E_FAIL;

	return m_pObject_Manager->Add_GameObject_ToLayer(iPrototypeLevelIndex, strPrototypeTag, iLevelIndex, strLayerTag, pArg);
}

CGameObject* CGameInstance::Add_GameObject_ToLayer_Return_Obj(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag, _uint iLevelIndex, const _wstring& strLayerTag, void* pArg)
{
	if (nullptr == m_pObject_Manager)
		return nullptr;

	return m_pObject_Manager->Add_GameObject_ToLayer_Return_Obj(iPrototypeLevelIndex, strPrototypeTag, iLevelIndex, strLayerTag, pArg);
}

list<class CGameObject*> CGameInstance::Get_List(_uint iLevelIndex, const _wstring& strLayerTag)
{
	return m_pObject_Manager->Get_List(iLevelIndex, strLayerTag);
}

CGameObject* CGameInstance::Get_GameObject_To_Layer(_uint iLevelIndex, const _wstring& strLayerTag, _uint Index)
{
	return m_pObject_Manager->Get_GameObject_To_Layer(iLevelIndex, strLayerTag, Index);
}


// ------------------------------------------------------------------------
// Shader_Manager
// ------------------------------------------------------------------------

void CGameInstance::Set_PipelineState(ID3D12GraphicsCommandList* pCmdList, const PSO_TYPE& _eType)
{
	if (nullptr == m_pShader_Manager)
		return;
	m_pShader_Manager->Set_PipelineState(pCmdList, _eType);
}

void CGameInstance::Set_RootSignature(ID3D12GraphicsCommandList* pCmdList)
{
	if (nullptr == m_pShader_Manager) {
		MSG_BOX("CGameInstance::Set_RootSignature() : Shader_Manager is nullptr");
		return;
	}
	m_pShader_Manager->Set_RootSignature(pCmdList);
}

// ------------------------------------------------------------------------
// Renderer
// ------------------------------------------------------------------------
HRESULT CGameInstance::Add_RenderObject(CRenderer::RENDERGROUP eRenderGroup, CGameObject* pRenderObject)
{
	if (nullptr == m_pRenderer)
		return E_FAIL;

	return m_pRenderer->Add_RenderObject(eRenderGroup, pRenderObject);
}

void CGameInstance::Release_Engine()
{
	CGameInstance::GetInstance()->Free();

	CGameInstance::GetInstance()->DestroyInstance();
}

void CGameInstance::Free()
{
	__super::Free();


	// 2. ComPtr로 보유한 커맨드 리스트 참조 해제
	m_pCommandList.Reset ();

	// 3. 렌더러 해제 (Device, CmdList를 Safe_AddRef로 보유 중)
	Safe_Release ( m_pRenderer );

	// 4. 게임 오브젝트 해제 (컴포넌트 → 텍스처/버퍼의 ComPtr 해제)
	Safe_Release ( m_pObject_Manager );

	// 5. 레벨 해제
	Safe_Release ( m_pLevel_Manager );

	// 6. 프로토타입 해제 (텍스처/버퍼 원본 리소스 해제)
	Safe_Release ( m_pPrototype_Manager );

	// 7. 카메라 해제 (CameraBuffer ComPtr 해제)
	Safe_Delete ( m_pCamera );

	// 8. 셰이더 매니저 해제 (PSO, RootSignature 해제)
	Safe_Release ( m_pShader_Manager );

	// 9. 나머지 매니저 해제
	Safe_Release ( m_pLoad_Manager );
	Safe_Release ( m_pTimer_Manager );
	Safe_Release ( m_pInput_Device );

	// 10. Device를 가장 마지막에 해제
	Safe_Release ( m_pGraphic_Device );
}