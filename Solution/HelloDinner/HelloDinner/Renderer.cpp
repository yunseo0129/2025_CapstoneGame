#include "Renderer.h"
#include "GameObject.h"
#include "GameInstance.h"

#include "Collider.h"
#include "Model.h"
#include "Map.h"
#include "UIObject.h"
#include "Font_Manager.h"

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
        for (_int p = 0; p < PASS_END; ++p) {
            m_InstanceBufferPool[f][p].reserve(8);
            InstanceBufferSlot slot;
            if (FAILED(Create_InstanceBufferSlot(slot))) return E_FAIL;
            m_InstanceBufferPool[f][p].push_back(slot);
        }
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

CRenderer::InstanceBufferSlot* CRenderer::Acquire_InstanceBufferSlot(_int frameIdx, INSTANCE_PASS ePass)
{
    if (m_iPoolCursor[frameIdx][ePass] >= (_int)m_InstanceBufferPool[frameIdx][ePass].size()) {
        InstanceBufferSlot slot;
        if (FAILED(Create_InstanceBufferSlot(slot))) return nullptr;
        m_InstanceBufferPool[frameIdx][ePass].push_back(slot);
    }
    return &m_InstanceBufferPool[frameIdx][ePass][m_iPoolCursor[frameIdx][ePass]++];
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

HRESULT CRenderer::Add_ShadowInstancedRenderObject(const _wstring& modelTag, CGameObject* pObj)
{
    if (!pObj) return E_FAIL;
    m_ShadowInstancedQueue[modelTag].push_back(pObj);
    Safe_AddRef(pObj);
    return S_OK;
}

HRESULT CRenderer::Draw_RenderObject(ID3D12GraphicsCommandList* _CmdList)
{
    m_iDrawCallCount = 0;

    if (FAILED(Render_Priority(_CmdList))) return E_FAIL;
    if (FAILED(Render_NonBlend(_CmdList))) return E_FAIL;
    if (m_bInstancingEnabled)
        Render_InstancedQueue(_CmdList, PASS_MAIN);
    if (FAILED(Render_Blend(_CmdList))) return E_FAIL;
#ifdef _DEBUG
    if (FAILED(Render_Collider(_CmdList))) return E_FAIL;
#endif
    return S_OK;
}

HRESULT CRenderer::Draw_UI(ID3D12GraphicsCommandList* _CmdList)
{
    if (FAILED(Render_UI(_CmdList))) return E_FAIL;
    if (FAILED(Render_Text(_CmdList))) return E_FAIL;
    return S_OK;
}

HRESULT CRenderer::Draw_ShadowQueue(ID3D12GraphicsCommandList* _CmdList)
{
    // 기존: 일반 ShadowRender 큐
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

    // 추가: instancing shadow 큐
    if (m_bInstancingEnabled)
        Render_InstancedQueue(_CmdList, PASS_SHADOW);

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
    auto& blendList = m_RenderObjects[RG_BLEND];

    if (blendList.empty())
        return S_OK;

    // [투명] 카메라 월드 위치 = 뷰 행렬의 역행렬 translation.
    XMFLOAT4X4 matView = m_pGameInstance->Get_CurrentCameraView();
    XMVECTOR det;
    XMMATRIX invView = XMMatrixInverse(&det, XMLoadFloat4x4(&matView));
    XMFLOAT3 vCamPos; XMStoreFloat3(&vCamPos, invView.r[3]);

    // [투명] 뒤→앞 정렬(먼 오브젝트 먼저). 오브젝트 중심은 월드행렬 translation.
    auto fnDistSq = [&](CGameObject* p) -> float {
        const _float4x4* w = p->Get_WorldMatrix4x4Ptr();
        float dx = w->_41 - vCamPos.x, dy = w->_42 - vCamPos.y, dz = w->_43 - vCamPos.z;
        return dx * dx + dy * dy + dz * dz;
        };
    blendList.sort([&](CGameObject* a, CGameObject* b) {
        return fnDistSq(a) > fnDistSq(b);
        });

    // [투명] 블렌드 PSO 1회 설정 후, 각 오브젝트의 유리 메시만 그린다.
    m_pGameInstance->Set_PipelineState(_CmdList, PSO_TYPE::ALPHA_BLEND);
    for (auto& pObj : blendList) {
        if (pObj) { pObj->Render_Blend(_CmdList); ++m_iDrawCallCount; }
        Safe_Release(pObj);
    }
    blendList.clear();
    return S_OK;
}

HRESULT CRenderer::Render_InstancedQueue(ID3D12GraphicsCommandList* cmd, INSTANCE_PASS ePass)
{
    _int frameIdx = m_pGameInstance->GetCurrentFrameIndex();
    m_iPoolCursor[frameIdx][ePass] = 0;

    // 큐 선택
    auto& queue = (ePass == PASS_MAIN) ? m_InstancedQueue : m_ShadowInstancedQueue;
    PSO_TYPE pso = (ePass == PASS_MAIN) ? PSO_TYPE::DEFAULT_INSTANCED : PSO_TYPE::SHADOW_STATIC_INSTANCED;
    bool bShadow = (ePass == PASS_SHADOW);

    // 통계는 메인만
    if (ePass == PASS_MAIN)
        m_iInstancedGroupCount = (_int)queue.size();

    for (auto& kv : queue)
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

        InstanceBufferSlot* pSlot = Acquire_InstanceBufferSlot(frameIdx, ePass);
        if (!pSlot) {
            for (auto* p : objs) Safe_Release(p);
            objs.clear();
            continue;
        }

        _uint instanceCount = 0;
        for (CGameObject* p : objs) {
            if (instanceCount >= MAX_INSTANCES_PER_GROUP) break;
            CMap* pMap = static_cast<CMap*>(p);
            pSlot->pMapped[instanceCount] = pMap->Get_CachedWorldMatrix();
            ++instanceCount;
        }

        m_pGameInstance->Set_PipelineState(cmd, pso);

        _uint iNumMeshes = pModel->Get_NumMeshes();
        for (_uint m = 0; m < iNumMeshes; ++m) {
            // [투명] 유리 메시는 불투명 인스턴스 패스(메인/그림자)에서 제외.
            //   메인은 블렌드 패스에서 비인스턴싱으로, 그림자는 생략.
            if (pModel->Is_MeshBlend(m)) continue;
            pModel->Render_Instanced(cmd, m, instanceCount, pSlot->vbv, bShadow);
            ++m_iDrawCallCount;
        }

        for (auto* p : objs) Safe_Release(p);
        objs.clear();
    }
    queue.clear();
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


HRESULT CRenderer::Render_UI(ID3D12GraphicsCommandList* _CmdList)
{
    m_RenderObjects[RG_UI].sort([](CGameObject* a, CGameObject* b) {
        return static_cast<CUIObject*>(a)->Get_Depth()
        > static_cast<CUIObject*>(b)->Get_Depth();
        });

    for (auto& pRenderObject : m_RenderObjects[RG_UI])
    {
        if (nullptr != pRenderObject)
            pRenderObject->Render(_CmdList);
        Safe_Release(pRenderObject);
    }
    m_RenderObjects[RG_UI].clear();

    return S_OK;
}

HRESULT CRenderer::Render_Text(ID3D12GraphicsCommandList* _CmdList)
{
    CFont_Manager* pFont = m_pGameInstance->Get_Font_Manager();
    if (nullptr == pFont)
    {
        for (auto& p : m_RenderObjects[RG_TEXT]) Safe_Release(p);
        m_RenderObjects[RG_TEXT].clear();
        return S_OK;
    }

    m_RenderObjects[RG_TEXT].sort([](CGameObject* a, CGameObject* b) {
        return static_cast<CUIObject*>(a)->Get_Depth()
         > static_cast<CUIObject*>(b)->Get_Depth();
        });

    m_pGameInstance->Font_Begin(_CmdList);
    for (auto& pRenderObject : m_RenderObjects[RG_TEXT]) {
        if (nullptr != pRenderObject) pRenderObject->Render(_CmdList);
        Safe_Release(pRenderObject);
    }
    m_pGameInstance->Font_End();
    m_pGameInstance->Bind_GlobalHeap(_CmdList);

    m_RenderObjects[RG_TEXT].clear();
    return S_OK;
}

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
    for (size_t i = 0; i < RG_END; i++) {
        for (auto& p : m_RenderObjects[i])       Safe_Release(p);
        for (auto& p : m_ShadowRenderObjects[i]) Safe_Release(p);
        m_RenderObjects[i].clear();
        m_ShadowRenderObjects[i].clear();
    }

    for (_int f = 0; f < FRAME_COUNT; ++f) {
        for (_int p = 0; p < PASS_END; ++p) {
            for (auto& slot : m_InstanceBufferPool[f][p]) {
                if (slot.pUploadBuffer && slot.pMapped) {
                    slot.pUploadBuffer->Unmap(0, nullptr);
                    slot.pMapped = nullptr;
                }
            }
            m_InstanceBufferPool[f][p].clear();
        }
    }

    for (auto& kv : m_InstancedQueue)
        for (auto* p : kv.second) Safe_Release(p);
    m_InstancedQueue.clear();

    for (auto& kv : m_ShadowInstancedQueue)
        for (auto* p : kv.second) Safe_Release(p);
    m_ShadowInstancedQueue.clear();

	m_pDevice.Reset();
	m_pCommandlist.Reset();
	Safe_Release(m_pGameInstance);

	__super::Free();

}

