#include "Skybox.h"
#include "Transform.h"
#include "GameInstance.h"
#include "Texture.h"
#include "VIBuffer.h"

CSkybox::CSkybox(EngineContext* pContext)
	: CGameObject(pContext)
{

}

CSkybox::CSkybox(const CSkybox& Prototype)
	: CGameObject(Prototype)
{

}

HRESULT CSkybox::Initialize_Prototype()
{
	Ready_Components();
	return S_OK;
}

HRESULT CSkybox::Initialize(void* pArg)
{
	Ready_Components();
	return S_OK;
}

void CSkybox::Priority_Update(_float fTimeDelta)
{
}

void CSkybox::Update(_float fTimeDelta)
{
}

void CSkybox::Late_Update(_float fTimeDelta)
{
	m_pGameInstance->Add_RenderObject(CRenderer::RG_NONBLEND, this);
}

void CSkybox::Render(ID3D12GraphicsCommandList* _commandList)
{
	// Transform 컴포넌트의 월드 행렬을 RootConstantBuffer에 넘겨준다.
	XMFLOAT4X4 WorldMatrix;
	XMStoreFloat4x4(&WorldMatrix, m_pTransformCom->Get_WorldMatrix());
	_commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &WorldMatrix, 0);

	// PSO 바인딩
	m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::DEFAULT);

	if (FAILED(m_pTextureCom->Bind_ShaderResource(_commandList, RootParameterIndex::TEXTURE))) {
		MSG_BOX("Failed to Bind Texture Resource in CSkybox");
		return;
	}

	// 정점 버퍼 바인딩 및 렌더링
	m_pVIBufferCom->Render(_commandList);
}

HRESULT CSkybox::Ready_Components()
{

	m_pTransformCom = CTransform::Create(m_pContext);

	if (FAILED(Add_Component(LEVEL_LOADING, L"Prototype_Component_VIBuffer_Skybox",
		TEXT("Com_VIBuffer"), reinterpret_cast<CComponent**>(&m_pVIBufferCom)))) {
		return E_FAIL;
	}

	if (m_pVIBufferCom == nullptr) {
		MSG_BOX("Failed to Add Component : VIBuffer");
		return E_FAIL;
	}

	if (FAILED(Add_Component(LEVEL_LOADING, L"Prototype_Component_Texture_Skybox",
		TEXT("Com_Texture"), reinterpret_cast<CComponent**>(&m_pTextureCom))))
		return E_FAIL;

	if (m_pTextureCom == nullptr) {
		MSG_BOX("Failed to Add Component : Texture");
		return E_FAIL;
	}

	return S_OK;
}

HRESULT CSkybox::Bind_ShaderResources()
{

	return S_OK;
}

CSkybox* CSkybox::Create(EngineContext* pContext)
{
	CSkybox* pInstance = new CSkybox(pContext);
	if (FAILED(pInstance->Initialize_Prototype()))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Create : CSkybox");
	}
	return pInstance;
}

CGameObject* CSkybox::Clone(void* pArg)
{
	CSkybox* pInstance = new CSkybox(*this);
	if (FAILED(pInstance->Initialize(pArg)))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Clone : CSkybox");
	}
	return pInstance;
}

void CSkybox::Free()
{
	__super::Free();
}
