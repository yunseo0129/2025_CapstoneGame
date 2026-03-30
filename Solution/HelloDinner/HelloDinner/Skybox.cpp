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
	: CGameObject(Prototype.m_pContext)
	, m_pTextureCom(Prototype.m_pTextureCom)
	, m_pVIBufferCom(Prototype.m_pVIBufferCom)
{
	Safe_AddRef(m_pTextureCom);
	Safe_AddRef(m_pVIBufferCom);
}

HRESULT CSkybox::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CSkybox::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return E_FAIL;

	Skybox_DESC* pDesc = static_cast<Skybox_DESC*>(pArg);

	m_strVIBufferTag = pDesc->strVIbufferTag;
	m_strTextureTag = pDesc->strTextureTag;
	m_iModelLevelIndex = pDesc->iModelLevelIndex;

	// CGameObject::Initialize가 Transform 생성 및 속도 설정
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
		return E_FAIL;

	return S_OK;
}

void CSkybox::Priority_Update(_float fTimeDelta)
{
}

void CSkybox::Update(_float fTimeDelta)
{
	__super::Update(fTimeDelta);
	_matrix WorldMatrix = m_pTransformCom->Get_WorldMatrix();
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
	m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::SKYBOX);

	if (FAILED(m_pTextureCom->Bind_ShaderResource(_commandList, RootParameterIndex::TEXTURE))) {
		MSG_BOX("Failed to Bind Texture Resource in Skybox");
		return;
	}

	// 정점 버퍼 바인딩 및 렌더링
	m_pVIBufferCom->Render(_commandList);
}

HRESULT CSkybox::Ready_Components()
{
	if (FAILED(Add_Component(m_iModelLevelIndex, m_strVIBufferTag,
		TEXT("Com_VIBuffer"), reinterpret_cast<CComponent**>(&m_pVIBufferCom))))
	{
		MSG_BOX("Failed to Add Component : VIBuffer in Skybox");
		return E_FAIL;
	}

	if (FAILED(Add_Component(m_iModelLevelIndex, m_strTextureTag,
		TEXT("Com_Texture"), reinterpret_cast<CComponent**>(&m_pTextureCom))))
	{
		MSG_BOX("Failed to Add Component : Texture in Skybox");
		return E_FAIL;
	}

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
	Safe_Release(m_pTextureCom);
	Safe_Release(m_pVIBufferCom);
	__super::Free();
}