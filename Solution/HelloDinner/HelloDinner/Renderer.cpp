#include "Renderer.h"
#include "GameObject.h"
#include "GameInstance.h"

#include "Collider.h"

CRenderer::CRenderer(ID3D12Device* pDevice, ID3D12GraphicsCommandList* _pCommandlist)
	: m_pDevice{ pDevice }
	, m_pCommandlist{ _pCommandlist }
	, m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);
}

HRESULT CRenderer::Initialize()
{
	return S_OK;
}

HRESULT CRenderer::Add_ShadowRenderObject(RENDERGROUP eRenderGroup, CGameObject* pRenderObject)
{
    if (eRenderGroup >= RG_END || !pRenderObject) return E_FAIL;
    m_ShadowRenderObjects[eRenderGroup].push_back(pRenderObject);
    Safe_AddRef(pRenderObject);
    return S_OK;
}

HRESULT CRenderer::Add_RenderObject(RENDERGROUP eRenderGroup, CGameObject* pRenderObject)
{
	if (eRenderGroup >= RG_END ||
		nullptr == pRenderObject)
		return E_FAIL;

	m_RenderObjects[eRenderGroup].push_back(pRenderObject);

	Safe_AddRef(pRenderObject);

	return S_OK;
}

HRESULT CRenderer::Draw_RenderObject(ID3D12GraphicsCommandList* _CmdList)
{
    if (FAILED(Render_Priority(_CmdList))) return E_FAIL;
    if (FAILED(Render_NonBlend(_CmdList))) return E_FAIL;
    if (FAILED(Render_Blend(_CmdList))) return E_FAIL;
#ifdef _DEBUG
    if (FAILED(Render_Collider(_CmdList))) return E_FAIL;
#endif
    return S_OK;
}

HRESULT CRenderer::Draw_ShadowQueue(ID3D12GraphicsCommandList* _CmdList)
{
    // 그림자는 RG_PRIORITY + RG_NONBLEND만 그림
    for (auto& pObj : m_ShadowRenderObjects[RG_PRIORITY]) {
        if (pObj) pObj->ShadowRender(_CmdList);
        Safe_Release(pObj);
    }
    m_ShadowRenderObjects[RG_PRIORITY].clear();

    for (auto& pObj : m_ShadowRenderObjects[RG_NONBLEND]) {
        if (pObj) pObj->ShadowRender(_CmdList);
        Safe_Release(pObj);
    }
    m_ShadowRenderObjects[RG_NONBLEND].clear();

    return S_OK;
}


HRESULT CRenderer::Render_Priority(ID3D12GraphicsCommandList* _CmdList)
{
    for (auto& pObj : m_RenderObjects[RG_PRIORITY]) {
        if (pObj) pObj->Render(_CmdList);
        Safe_Release(pObj);
    }
    m_RenderObjects[RG_PRIORITY].clear();
    return S_OK;
}

HRESULT CRenderer::Render_NonBlend(ID3D12GraphicsCommandList* _CmdList)
{
    for (auto& pObj : m_RenderObjects[RG_NONBLEND]) {
        if (pObj) pObj->Render(_CmdList);
        Safe_Release(pObj);
    }
    m_RenderObjects[RG_NONBLEND].clear();
    return S_OK;
}


HRESULT CRenderer::Render_Blend(ID3D12GraphicsCommandList* _CmdList)
{
    for (auto& pObj : m_RenderObjects[RG_BLEND]) {
        if (pObj) pObj->Render(_CmdList);
        Safe_Release(pObj);
    }
    m_RenderObjects[RG_BLEND].clear();
    return S_OK;
}

#ifdef _DEBUG
HRESULT CRenderer::Add_RenderCollider(CCollider* pColliderCom )
{
	m_RenderColliders.push_back(pColliderCom);
	Safe_AddRef ( m_RenderColliders.back () );
	return S_OK;
}


HRESULT CRenderer::Render_Collider ( ID3D12GraphicsCommandList* _CmdList )
{	
	for ( auto& pRenderCollider : m_RenderColliders )
	{

		if ( nullptr != pRenderCollider )
			pRenderCollider->Render ( _CmdList );
		Safe_Release(pRenderCollider);
	}
	m_RenderColliders.clear ();

	return S_OK;
}
#endif

//HRESULT CRenderer::Render_UI()
//{
//
//	stable_sort(m_RenderObjects[RG_UI].begin(), m_RenderObjects[RG_UI].end(), [&](CGameObject* a, CGameObject* b) {return static_cast<CUIObject*>(a)->Get_Depth() > static_cast<CUIObject*>(b)->Get_Depth(); });
//
//	for (auto& pRenderObject : m_RenderObjects[RG_UI])
//	{
//		if (nullptr != pRenderObject)
//			pRenderObject->Render(m_pCommandlist);
//
//		Safe_Release(pRenderObject);
//	}
//	m_RenderObjects[RG_UI].clear();
//
//	return S_OK;
//}

CRenderer* CRenderer::Create(ID3D12Device* pDevice, ID3D12GraphicsCommandList* _commandList)
{
	CRenderer* pInstance = new CRenderer(pDevice, _commandList);

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : CRenderer");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CRenderer::Free()
{
	for (size_t i = 0; i < RG_END; i++)
	{
		for (auto& pRenderObject : m_RenderObjects[i])
			Safe_Release(pRenderObject);

		m_RenderObjects[i].clear();
	}

    for (size_t i = 0; i < RG_END; i++) {
        for (auto& p : m_RenderObjects[i])       Safe_Release(p);
        for (auto& p : m_ShadowRenderObjects[i]) Safe_Release(p);
        m_RenderObjects[i].clear();
        m_ShadowRenderObjects[i].clear();
    }

	m_pDevice.Reset();
	m_pCommandlist.Reset();
	Safe_Release(m_pGameInstance);

	__super::Free();

}

