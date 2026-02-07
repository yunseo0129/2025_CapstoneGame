#include "stdafx.h"
#include "MainApp.h"
#include "GameInstance.h"
#include <WindowsX.h>

CMainApp::CMainApp()
	: m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);

}


HRESULT CMainApp::Initialize()
{
	ENGINE_DESC EngineDesc;

	EngineDesc.hInstance = g_hInst;
	EngineDesc.hWnd = g_hWnd;
	EngineDesc.isWindowed = true;
	EngineDesc.iNumLevels = 3;
	EngineDesc.iViewportWidth = Client::g_iWinSizeX;
	EngineDesc.iViewportHeight = Client::g_iWinSizeY;

	EngineContext EngineContext;
	EngineContext.cmdList = nullptr;
	EngineContext.cmdQueue = nullptr;
	EngineContext.device = nullptr;
	EngineContext.rtvHeap = nullptr;
	EngineContext.dsvHeap = nullptr;
	EngineContext.srvHeap = nullptr;

	if (FAILED(m_pGameInstance->Initialize_Engine(EngineDesc, &EngineContext)))
		return E_FAIL;

	return S_OK;
}


void CMainApp::Update(_float fTimeDelta)
{
	if (nullptr != m_pGameInstance)
		m_pGameInstance->Update_Engine(fTimeDelta);
}

HRESULT CMainApp::Render()
{
	if (nullptr == m_pGameInstance)
		return E_FAIL;

	if (FAILED(m_pGameInstance->Render_Begin()))
		return E_FAIL;

	m_pGameInstance->Draw();

	if (FAILED(m_pGameInstance->Render_End()))
		return E_FAIL;

	return S_OK;
}


void CMainApp::FrameAdvance()
{
	Render();
}


//void CMainApp::CreateRootSignature()
//{
//	CD3DX12_DESCRIPTOR_RANGE descriptorRange[2];
//	descriptorRange[DescriptorRange::Texture].Init(
//		D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
//	descriptorRange[DescriptorRange::TextureCube].Init(
//		D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
//
//	CD3DX12_ROOT_PARAMETER rootParameter[4];
//	rootParameter[RootParameter::GameObject].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_ALL);
//	rootParameter[RootParameter::Camera].InitAsConstants(32, 1, 0, D3D12_SHADER_VISIBILITY_ALL);
//	rootParameter[RootParameter::Texture].InitAsDescriptorTable(1,
//		&descriptorRange[DescriptorRange::Texture], D3D12_SHADER_VISIBILITY_PIXEL);
//	rootParameter[RootParameter::TextureCube].InitAsDescriptorTable(1,
//		&descriptorRange[DescriptorRange::TextureCube], D3D12_SHADER_VISIBILITY_PIXEL);
//
//	CD3DX12_STATIC_SAMPLER_DESC samplerDesc;
//	samplerDesc.Init(
//		0,
//		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
//		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
//		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
//		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
//		0.0f,
//		1,
//		D3D12_COMPARISON_FUNC_ALWAYS,
//		D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
//		0.0f,
//		D3D12_FLOAT32_MAX,
//		D3D12_SHADER_VISIBILITY_PIXEL,
//		0
//	);
//
//	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
//	rootSignatureDesc.Init(_countof(rootParameter), rootParameter, 1, &samplerDesc,
//		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
//
//	ComPtr<ID3DBlob> signature, error;
//	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc,
//		D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
//	ThrowIfFailed(m_pD3dDevice->CreateRootSignature(0, signature->GetBufferPointer(),
//		signature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature)));
//}

CMainApp* CMainApp::Create()
{
	CMainApp* pInstance = new CMainApp();

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : CMainApp");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CMainApp::Free()
{
	/* 부모 멤버를 정리한다. */
	__super::Free();
	Safe_Release(m_pGameInstance);

	CGameInstance::Release_Engine();
}

