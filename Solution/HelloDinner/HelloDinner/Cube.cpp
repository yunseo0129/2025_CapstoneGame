#include "Cube.h"
#include "Transform.h"
#include "GameInstance.h"
#include "Texture.h"
#include "VIBuffer.h"

CCube::CCube(EngineContext* pContext)
	: CGameObject(pContext)
{
	Safe_AddRef(m_pTextureCom);
	Safe_AddRef(m_pVIBufferCom);
}

CCube::CCube(const CCube& Prototype)
	: CGameObject(Prototype.m_pContext)
	, m_pTextureCom(Prototype.m_pTextureCom)
	, m_pVIBufferCom(Prototype.m_pVIBufferCom)
{
	Safe_AddRef(m_pTextureCom);
	Safe_AddRef(m_pVIBufferCom);
}

HRESULT CCube::Initialize_Prototype()
{
	Ready_Components();
	return S_OK;
}

HRESULT CCube::Initialize(void* pArg)
{
	Ready_Components();
	return S_OK;
}

void CCube::Priority_Update(_float fTimeDelta)
{
}

void CCube::Update(_float fTimeDelta)
{
}

void CCube::Late_Update(_float fTimeDelta)
{
	m_pGameInstance->Add_RenderObject(CRenderer::RG_NONBLEND, this);
}

void CCube::Render(ID3D12GraphicsCommandList* _commandList)
{
	// Transform 컴포넌트의 월드 행렬을 RootConstantBuffer에 넘겨준다.
	XMFLOAT4X4 WorldMatrix;
	XMStoreFloat4x4(&WorldMatrix, m_pTransformCom->Get_WorldMatrix());
	_commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &WorldMatrix, 0);

	// PSO 바인딩
	m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::DEFAULT);

	if (FAILED(m_pTextureCom->Bind_ShaderResource(_commandList, RootParameterIndex::TEXTURE_Diffuse))) {
		MSG_BOX("Failed to Bind Texture Resource in CCube");
		return;
	}

	// 정점 버퍼 바인딩 및 렌더링
	m_pVIBufferCom->Render(_commandList);
}

HRESULT CCube::Ready_Components()
{

	m_pTransformCom = CTransform::Create(m_pContext);

	if (FAILED(Add_Component(LEVEL_LOADING, L"Prototype_Component_VIBuffer_VtxCube",
		TEXT("Com_VIBuffer"), reinterpret_cast<CComponent**>(&m_pVIBufferCom)))) {
		return E_FAIL;  
	}

	if (m_pVIBufferCom == nullptr) {
		MSG_BOX("Failed to Add Component : VIBuffer");
		return E_FAIL;
	}

	if (FAILED(Add_Component(LEVEL_LOADING, L"Prototype_Component_Texture_Cube",
		TEXT("Com_Texture"), reinterpret_cast<CComponent**>(&m_pTextureCom))))
		return E_FAIL;

	if (m_pTextureCom == nullptr) {
		MSG_BOX("Failed to Add Component : Texture");
		return E_FAIL;
	}

	return S_OK;
}

HRESULT CCube::Bind_ShaderResources()
{

	return S_OK;
}

CCube* CCube::Create(EngineContext* pContext)
{
	CCube* pInstance = new CCube(pContext);
	if (FAILED(pInstance->Initialize_Prototype()))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Create : CCube");
	}
	return pInstance;
}

CGameObject* CCube::Clone(void* pArg)
{
	CCube* pInstance = new CCube(*this);
	if (FAILED(pInstance->Initialize(pArg)))
	{
		Safe_Release(pInstance);
		MSG_BOX("Failed to Clone : CCube");
	}
	return pInstance;
}

void CCube::Free()
{
	Safe_Release(m_pVIBufferCom);
	Safe_Release(m_pTextureCom);
	__super::Free();
}
