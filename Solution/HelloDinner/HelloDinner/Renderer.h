#pragma once

#include "Base.h"

class CRenderer final : public CBase
{
public:
	enum RENDERGROUP { RG_PRIORITY, RG_NONBLEND, RG_BLEND, RG_UI, RG_END };

private:
	CRenderer(ID3D12Device* pDevice, ID3D12GraphicsCommandList* _pCommandlist);
	virtual ~CRenderer() = default;

public:
	HRESULT Initialize();
	HRESULT Add_RenderObject(RENDERGROUP eRenderGroup, class CGameObject* pRenderObject);
	HRESULT Draw_RenderObject(ID3D12GraphicsCommandList* _CmdList);

private:
	ComPtr<ID3D12Device> m_pDevice = { nullptr };
	ComPtr<ID3D12GraphicsCommandList> m_pCommandlist = { nullptr };
	class CGameInstance* m_pGameInstance = { nullptr };
	list<class CGameObject*>			m_RenderObjects[RG_END];
	list<class CCollider*>				m_RenderColliders;

private:
	HRESULT Render_Priority( ID3D12GraphicsCommandList* _CmdList );
	HRESULT Render_NonBlend( ID3D12GraphicsCommandList* _CmdList );
	HRESULT Render_Blend( ID3D12GraphicsCommandList* _CmdList );
	//HRESULT Render_UI();

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
