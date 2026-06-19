#pragma once

#include "Base.h"
#include "Graphic_Device.h"

class CRenderer final : public CBase
{
public:
    enum RENDERGROUP { RG_PRIORITY, RG_NONBLEND, RG_BLEND, RG_UI, RG_TEXT, RG_END };

public:
    enum INSTANCE_PASS { PASS_MAIN, PASS_SHADOW, PASS_END };

private:
	CRenderer(ID3D12Device* pDevice, ID3D12GraphicsCommandList* _pCommandlist);
	virtual ~CRenderer() = default;

public:
    HRESULT Initialize();
    HRESULT Add_RenderObject(RENDERGROUP eRenderGroup, class CGameObject* pRenderObject);
    HRESULT Add_ShadowRenderObject(RENDERGROUP eRenderGroup, class CGameObject* pRenderObject);
    HRESULT Draw_RenderObject(ID3D12GraphicsCommandList* _CmdList);
    HRESULT Draw_ShadowQueue(ID3D12GraphicsCommandList* _CmdList);

    // Insatanced
    HRESULT Add_InstancedRenderObject(const _wstring& modelTag, class CGameObject* pObj);
    HRESULT Add_ShadowInstancedRenderObject(const _wstring& modelTag, class CGameObject* pObj);

    void    Set_InstancingEnabled(bool b) { m_bInstancingEnabled = b; }
    bool    Is_InstancingEnabled() const { return m_bInstancingEnabled; }
    _int    Get_DrawCallCount() const { return m_iDrawCallCount; }
    _int    Get_InstancedGroupCount() const { return m_iInstancedGroupCount; }

private:
	ComPtr<ID3D12Device> m_pDevice = { nullptr };
	ComPtr<ID3D12GraphicsCommandList> m_pCommandlist = { nullptr };
	class CGameInstance* m_pGameInstance = { nullptr };
	list<class CGameObject*>			m_RenderObjects[RG_END];
    list<class CGameObject*>            m_ShadowRenderObjects[RG_END];
	list<class CCollider*>				m_RenderColliders;

private:
    // instance queue
    unordered_map<_wstring, vector<class CGameObject*>> m_InstancedQueue;
    unordered_map<_wstring, vector<class CGameObject*>> m_ShadowInstancedQueue;
    
    bool m_bInstancingEnabled = true;
    _int m_iDrawCallCount = 0;
    _int m_iInstancedGroupCount = 0;
    
    // instance buffer
    static const _int FRAME_COUNT = CGraphic_Device::SWAP_CHAIN_BUFFER_COUNT;
    static const _int MAX_INSTANCES_PER_GROUP = 1024;

    struct InstanceBufferSlot
    {
        ComPtr<ID3D12Resource> pUploadBuffer;
        XMFLOAT4X4* pMapped = nullptr;
        D3D12_VERTEX_BUFFER_VIEW vbv = {};
    };
    // [frameIdx][passIdx] 2차원 풀
    vector<InstanceBufferSlot> m_InstanceBufferPool[FRAME_COUNT][PASS_END];
    _int                       m_iPoolCursor[FRAME_COUNT][PASS_END] = {0};


private:
    HRESULT Render_Priority(ID3D12GraphicsCommandList* _CmdList);
    HRESULT Render_NonBlend(ID3D12GraphicsCommandList* _CmdList);
    HRESULT Render_Blend(ID3D12GraphicsCommandList* _CmdList);
	HRESULT Render_UI(ID3D12GraphicsCommandList* _CmdList);
    HRESULT Render_Text(ID3D12GraphicsCommandList* _CmdList);
    // instanced rendering
    HRESULT Create_InstanceBufferSlot(InstanceBufferSlot& outSlot);
    InstanceBufferSlot* Acquire_InstanceBufferSlot(_int frameIdx, INSTANCE_PASS ePass);

    HRESULT Render_InstancedQueue(ID3D12GraphicsCommandList* cmd, INSTANCE_PASS ePass);

#ifdef _DEBUG
public:
	HRESULT Add_RenderCollider ( class CCollider* pColliderCom );
private:
	HRESULT Render_Collider ( ID3D12GraphicsCommandList* _CmdList );
#endif

public:
	static CRenderer* Create(ID3D12Device* pDevice, ID3D12GraphicsCommandList* _commandList);
	virtual void Free() override;
};
