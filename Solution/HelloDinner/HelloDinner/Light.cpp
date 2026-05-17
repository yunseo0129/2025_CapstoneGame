#include "Light.h"
#include "Camera.h"
#include "Shadow.h"
#include "GameInstance.h"

CLight::CLight(EngineContext* pContext)
	: m_pContext{ pContext }
	, m_pGameInstance{ CGameInstance::GetInstance() }
{
}

HRESULT CLight::Initialize(const LIGHT_DESC& LightDesc)
{
	m_LightDesc = LightDesc;

	Create_LightBuffer();
	return S_OK;
}

void CLight::Update(const _float fTimeDelta)
{
	//Light 업데이트 
}

void CLight::Render_Begin(CCamera* _camera)
{
	// Light에서 그림자 맵에 씬을 그리는 함수 호출
	if (m_pShadow)
		m_pShadow->Update(_camera, this);
}

void CLight::Render(ID3D12GraphicsCommandList* pCmdList)
{
	if (m_pShadow)
		m_pShadow->DrawSceneToShadowMap(pCmdList);
}

void CLight::Bind_LightBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex)
{
	_int iFrameIndex = m_pGameInstance->GetCurrentFrameIndex();

	CB_LIGHT cbLight = {};
	cbLight.vDirection = m_LightDesc.vDirection;
	cbLight.vPosition = m_LightDesc.vPosition;
	cbLight.vDiffuse = m_LightDesc.vDiffuse;
	cbLight.vAmbient = m_LightDesc.vAmbient;
	cbLight.vSpecular = m_LightDesc.vSpecular;
	cbLight.fRange = m_LightDesc.fRange;
	if (m_pShadow)
	{
		cbLight.matLightTransform = m_pShadow->Get_LightTransform();
	}
	else
	{
		cbLight.matLightTransform = XMFLOAT4X4();
	}
	memcpy(m_pCbMappedLight[iFrameIndex], &cbLight, sizeof(CB_LIGHT));

	pCmdList->SetGraphicsRootConstantBufferView(_eIndex, m_pLightbuffer[iFrameIndex]->GetGPUVirtualAddress());

	m_pShadow->Bind_ShadowMap(pCmdList, RootParameterIndex::ShadowMap);
}

HRESULT CLight::Create_LightBuffer()
{
	_uint ncbElementBytes = ((sizeof(CB_LIGHT) + 255) & ~255);

	D3D12_HEAP_PROPERTIES d3dHeapPropertiesDesc;
	::ZeroMemory(&d3dHeapPropertiesDesc, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapPropertiesDesc.Type = D3D12_HEAP_TYPE_UPLOAD;
	d3dHeapPropertiesDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapPropertiesDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapPropertiesDesc.CreationNodeMask = 1;
	d3dHeapPropertiesDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC d3dResourceDesc;
	::ZeroMemory(&d3dResourceDesc, sizeof(D3D12_RESOURCE_DESC));
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	d3dResourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	d3dResourceDesc.Width = ncbElementBytes;
	d3dResourceDesc.Height = 1;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	d3dResourceDesc.SampleDesc.Count = 1;
	d3dResourceDesc.SampleDesc.Quality = 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	for (_int i = 0; i < FRAME_COUNT; ++i)
	{
		HRESULT hResult = m_pContext->device->CreateCommittedResource(
			&d3dHeapPropertiesDesc, D3D12_HEAP_FLAG_NONE,
			&d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
			NULL, __uuidof(ID3D12Resource), (void**)&m_pLightbuffer[i]);

		if (FAILED(hResult))
			return E_FAIL;

		m_pLightbuffer[i]->Map(0, NULL, (void**)&m_pCbMappedLight[i]);
	}

	return S_OK;
}

CLight* CLight::Create(EngineContext* pContext, const LIGHT_DESC& LightDesc)
{
	CLight* pInstance = new CLight(pContext);

	if (FAILED(pInstance->Initialize(LightDesc)))
	{
		MSG_BOX("Failed to Created : CLight");
		Safe_Release(pInstance);
	}

	return pInstance;
}


void CLight::Free()
{
	__super::Free();

}
