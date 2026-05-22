#include "Renderer.h"
#include "GameObject.h"
#include "GameInstance.h"

#include "Collider.h"
#include "Model.h"
#include "Map.h"

CRenderer::CRenderer(ID3D12Device* pDevice, ID3D12GraphicsCommandList* _pCommandlist)
	: m_pDevice{ pDevice }
	, m_pCommandlist{ _pCommandlist }
	, m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);
}

HRESULT CRenderer::Initialize()
{
    for (_int f = 0; f < FRAME_COUNT; ++f) {
        m_InstanceBufferPool[f].reserve(8);
        InstanceBufferSlot slot;
        if (FAILED(Create_InstanceBufferSlot(slot))) return E_FAIL;
        m_InstanceBufferPool[f].push_back(slot);
    }
	return S_OK;
}

HRESULT CRenderer::Create_InstanceBufferSlot(InstanceBufferSlot& outSlot)
{
    _uint bufferSize = sizeof(XMFLOAT4X4) * MAX_INSTANCES_PER_GROUP;

    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bufferSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(m_pDevice->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&outSlot.pUploadBuffer))))
        return E_FAIL;

    outSlot.pUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&outSlot.pMapped));

    outSlot.vbv.BufferLocation = outSlot.pUploadBuffer->GetGPUVirtualAddress();
    outSlot.vbv.StrideInBytes = sizeof(XMFLOAT4X4);
    outSlot.vbv.SizeInBytes = bufferSize;
    return S_OK;
}

CRenderer::InstanceBufferSlot* CRenderer::Acquire_InstanceBufferSlot(_int frameIdx)
{
    // 현재 프레임에서 사용 가능한 슬롯이 부족하면 새로 생성
    if (m_iPoolCursor[frameIdx] >= (_int)m_InstanceBufferPool[frameIdx].size()) {
        InstanceBufferSlot slot;
        if (FAILED(Create_InstanceBufferSlot(slot))) return nullptr;
        m_InstanceBufferPool[frameIdx].push_back(slot);
    }
    return &m_InstanceBufferPool[frameIdx][m_iPoolCursor[frameIdx]++];
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

HRESULT CRenderer::Add_InstancedRenderObject(const _wstring& modelTag, CGameObject* pObj)
{
    if (!pObj) return E_FAIL;
    m_InstancedQueue[modelTag].push_back(pObj);
    Safe_AddRef(pObj);
    return S_OK;
}

HRESULT CRenderer::Draw_RenderObject(ID3D12GraphicsCommandList* _CmdList)
{
    m_iDrawCallCount = 0;

    if (FAILED(Render_Priority(_CmdList))) return E_FAIL;
    if (FAILED(Render_NonBlend(_CmdList))) return E_FAIL;
    if (m_bInstancingEnabled)
        Render_InstancedQueue(_CmdList, false);
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

HRESULT CRenderer::Render_InstancedQueue(ID3D12GraphicsCommandList* cmd, bool bShadow)
{
    _int frameIdx = m_pGameInstance->GetCurrentFrameIndex();
    m_iPoolCursor[frameIdx] = 0;   // 프레임 시작 시 풀 커서 리셋

    m_iInstancedGroupCount = (_int)m_InstancedQueue.size();

    for (auto& kv : m_InstancedQueue)
    {
        auto& objs = kv.second;
        if (objs.empty()) continue;

        CMap* pFirst = dynamic_cast<CMap*>(objs[0]);
        if (!pFirst) {
            for (auto* p : objs) Safe_Release(p);
            objs.clear();
            continue;
        }
        CModel* pModel = pFirst->Get_Model();
        if (!pModel) {
            for (auto* p : objs) Safe_Release(p);
            objs.clear();
            continue;
        }

        // 인스턴스 버퍼 슬롯 확보 + world matrix 채우기
        InstanceBufferSlot* pSlot = Acquire_InstanceBufferSlot(frameIdx);
        if (!pSlot) {
            for (auto* p : objs) Safe_Release(p);
            objs.clear();
            continue;
        }

        _uint instanceCount = 0;
        for (CGameObject* p : objs) {
            if (instanceCount >= MAX_INSTANCES_PER_GROUP) break;
            CMap* pMap = static_cast<CMap*>(p);
            pSlot->pMapped[instanceCount++] = pMap->Get_CachedWorldMatrix();
        }

        // PSO 설정 (인스턴싱 셰이더로)
        if (!bShadow)
            m_pGameInstance->Set_PipelineState(cmd, PSO_TYPE::DEFAULT_INSTANCED);
        else
            m_pGameInstance->Set_PipelineState(cmd, PSO_TYPE::SHADOW_STATIC);  // 이후 SHADOW_STATIC_INSTANCED로 교체

        // 메쉬별 인스턴스 그리기
        _uint iNumMeshes = pModel->Get_NumMeshes();
        for (_uint m = 0; m < iNumMeshes; ++m) {
            pModel->Render_Instanced(cmd, m, instanceCount, pSlot->vbv, bShadow);
            ++m_iDrawCallCount;
        }

        // 큐 비우기 + AddRef 해제
        for (auto* p : objs) Safe_Release(p);
        objs.clear();
    }
    m_InstancedQueue.clear();   // 다음 프레임을 위해
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

    for (_int f = 0; f < FRAME_COUNT; ++f) {
        for (auto& slot : m_InstanceBufferPool[f]) {
            if (slot.pUploadBuffer && slot.pMapped) {
                slot.pUploadBuffer->Unmap(0, nullptr);
                slot.pMapped = nullptr;
            }
        }
        m_InstanceBufferPool[f].clear();
    }
    for (auto& kv : m_InstancedQueue)
        for (auto* p : kv.second) Safe_Release(p);
    m_InstancedQueue.clear();

	m_pDevice.Reset();
	m_pCommandlist.Reset();
	Safe_Release(m_pGameInstance);

	__super::Free();

}

