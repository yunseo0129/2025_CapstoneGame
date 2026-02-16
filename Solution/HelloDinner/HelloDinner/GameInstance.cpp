#include "GameInstance.h"

#include "Renderer.h"
#include "PipeLine.h"
#include "Input_Device.h"
#include "Timer_Manager.h"
#include "Level_Manager.h"
#include "Graphic_Device.h"
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

	m_pPipeLine = CPipeLine::Create();
	if (nullptr == m_pPipeLine)
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
	CameraDesc.vEye = { 0.f, 0.f, -5.f };
	CameraDesc.vAt = { 0.f, 0.f, 0.f };
	CameraDesc.fFovy = XMConvertToRadians(45.f);
	CameraDesc.fAspect = static_cast<_float>(EngineDesc.iViewportWidth) / EngineDesc.iViewportHeight;
	CameraDesc.fNear = 0.1f;
	CameraDesc.fFar = 100.f;

	if (FAILED(m_pCamera->Initialize(&CameraDesc)))
		return E_FAIL;

	UINT ncbElementBytes = ((sizeof(CB_VS_CAMERA) + 255) & ~255) / 256;

	D3D12_HEAP_PROPERTIES d3dHeapPropertiesDesc;
	::ZeroMemory(&d3dHeapPropertiesDesc, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapPropertiesDesc.Type = D3D12_HEAP_TYPE_UPLOAD;
	d3dHeapPropertiesDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapPropertiesDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapPropertiesDesc.CreationNodeMask = 1;
	d3dHeapPropertiesDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DIMENSION d3dResourceDimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	D3D12_RESOURCE_DESC d3dResourceDesc;
	::ZeroMemory(&d3dResourceDesc, sizeof(D3D12_RESOURCE_DESC));
	d3dResourceDesc.Dimension = d3dResourceDimension; //D3D12_RESOURCE_DIMENSION_BUFFER, D3D12_RESOURCE_DIMENSION_TEXTURE1D, D3D12_RESOURCE_DIMENSION_TEXTURE2D
	d3dResourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	d3dResourceDesc.Width = ncbElementBytes;
	d3dResourceDesc.Height = 1;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels =  1;
	d3dResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	d3dResourceDesc.SampleDesc.Count = 1;
	d3dResourceDesc.SampleDesc.Quality = 0;
	d3dResourceDesc.Layout = (d3dResourceDimension == D3D12_RESOURCE_DIMENSION_BUFFER) ? D3D12_TEXTURE_LAYOUT_ROW_MAJOR : D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_RESOURCE_STATES d3dResourceStates = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	d3dResourceStates |= D3D12_RESOURCE_STATE_GENERIC_READ;
	HRESULT hResult = _pcontext->device->CreateCommittedResource(&d3dHeapPropertiesDesc, D3D12_HEAP_FLAG_NONE, &d3dResourceDesc, d3dResourceStates, NULL, __uuidof(ID3D12Resource), (void**)&m_pCameraBuffer);

	m_pCameraBuffer.Get()->Map(0, NULL, (void**)&m_pcbMappedCamera);

	m_pCommandList = _pcontext->cmdList;

	return S_OK;
}

void CGameInstance::Update_Engine(_float fTimeDelta)
{
	m_pCamera->Update(fTimeDelta);

	XMFLOAT4X4 xmf4x4View;
	XMStoreFloat4x4(&xmf4x4View, m_pPipeLine->Get_TransformMatrix(CPipeLine::D3DTS_VIEW));
	memcpy(&m_pcbMappedCamera->m_xmf4x4View, &xmf4x4View, sizeof(_float4x4));

	XMFLOAT4X4 xmf4x4Proj;
	XMStoreFloat4x4(&xmf4x4Proj, m_pPipeLine->Get_TransformMatrix(CPipeLine::D3DTS_PROJ));
	memcpy(&m_pcbMappedCamera->m_xmf4x4Proj, &xmf4x4Proj, sizeof(_float4x4));

	memcpy(&m_pcbMappedCamera->m_xmf3Position, m_pPipeLine->Get_CamPosition(), sizeof(_float3));

	m_pObject_Manager->Late_Update(fTimeDelta);
}

HRESULT CGameInstance::Render_Begin(const _float4& vClearColor)
{
	m_pGraphic_Device->BeforeRender(vClearColor);

	m_pCommandList = m_pGraphic_Device->GetCommandList();

	Set_RootSignature(m_pCommandList.Get());

	D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = m_pCameraBuffer->GetGPUVirtualAddress();
	m_pCommandList->SetGraphicsRootConstantBufferView(RootParameterIndex::Camera, d3dGpuVirtualAddress);


	return S_OK;
}

HRESULT CGameInstance::Draw()
{


	m_pRenderer->Draw_RenderObject(m_pCommandList.Get());

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
// PipeLine
// ------------------------------------------------------------------------

void CGameInstance::Set_Transform(CPipeLine::D3DTRANSFORMSTATE eState, _fmatrix TransformMatrix)
{
	if (nullptr == m_pPipeLine)
		return;

	return m_pPipeLine->Set_Transform(eState, TransformMatrix);
}

void CGameInstance::Set_CamPosition(_float4 vCamPosition)
{
	if (nullptr == m_pPipeLine)
		return;
	return m_pPipeLine->Set_CamPosition(vCamPosition);
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

	Safe_Release(m_pPrototype_Manager);
	Safe_Release(m_pGraphic_Device);
}