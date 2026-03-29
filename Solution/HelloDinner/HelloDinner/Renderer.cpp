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
	if (FAILED(Render_Priority( _CmdList )))
		return E_FAIL;
	if (FAILED(Render_NonBlend( _CmdList )))
		return E_FAIL;
	if (FAILED(Render_Blend( _CmdList )))
		return E_FAIL;
	//if (FAILED(Render_UI()))
		//return E_FAIL;

#ifdef _DEBUG
	if ( FAILED ( Render_Collider ( _CmdList ) ) )
		return E_FAIL;
#endif // DEBUG

	return S_OK;
}

HRESULT CRenderer::Render_Priority( ID3D12GraphicsCommandList* _CmdList )
{
	for (auto& pRenderObject : m_RenderObjects[RG_PRIORITY])
	{
		if (nullptr != pRenderObject) {
			pRenderObject->Render (_CmdList);
			Add_RenderCollider(pRenderObject);
		}
		Safe_Release(pRenderObject);
	}
	m_RenderObjects[RG_PRIORITY].clear();

	return S_OK;
}

HRESULT CRenderer::Render_NonBlend( ID3D12GraphicsCommandList* _CmdList )
{
	//m_pGameInstance->Render_BlockList();

	for (auto& pRenderObject : m_RenderObjects[RG_NONBLEND])
	{
		if (nullptr != pRenderObject) {
			pRenderObject->Render (_CmdList);
			Add_RenderCollider(pRenderObject);
		}
		Safe_Release(pRenderObject);
	}
	m_RenderObjects[RG_NONBLEND].clear();

	return S_OK;
}

HRESULT CRenderer::Render_Blend( ID3D12GraphicsCommandList* _CmdList )
{
	for (auto& pRenderObject : m_RenderObjects[RG_BLEND])
	{
		if ( nullptr != pRenderObject ) {
			pRenderObject->Render(_CmdList);
			Add_RenderCollider(pRenderObject);
		}
		Safe_Release(pRenderObject);
	}
	m_RenderObjects[RG_BLEND].clear();

	return S_OK;
}

void CRenderer::Add_RenderCollider(CGameObject*& pRenderObject)
{
	// 나중에 배열로 바꿔서 여러 콜라이더 태그를 검사할 수 있도록 수정하기
	const _tchar* szColliderTags[] = {
		TEXT("Com_Collider"),       // 기본 단일 콜라이더
		TEXT("Com_Collider_Head"),  // 머리
		TEXT("Com_Collider_Body"),  // 몸통
		TEXT("Com_Collider_L_UpperArm"), // 왼U팔
		TEXT("Com_Collider_R_UpperArm"), // 오른U팔
		TEXT("Com_Collider_L_LoewrArm"), // 왼L팔
		TEXT("Com_Collider_R_LoewrArm"), // 오른L팔
		TEXT("Com_Collider_L_Leg"), // 왼다리
		TEXT("Com_Collider_R_Leg")  // 오른다리
	};
	for (const _tchar* szTag : szColliderTags) {
		CComponent* pColliderCom = pRenderObject->Find_Component(szTag);
		if (nullptr != pColliderCom) {
			m_RenderColliders.push_back(static_cast<CCollider*>(pColliderCom));
			Safe_AddRef(m_RenderColliders.back());
		}
	}
}

#ifdef _DEBUG
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

	m_pDevice.Reset();
	m_pCommandlist.Reset();
	Safe_Release(m_pGameInstance);

	__super::Free();

}

