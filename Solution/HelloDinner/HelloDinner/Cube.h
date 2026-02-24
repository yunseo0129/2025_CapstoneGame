#pragma once
#include "GameObject.h"

class CCube : public CGameObject
{
private:
	CCube(EngineContext* _pcontext);
	CCube(const CCube& Prototype);
	virtual ~CCube() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(_float fTimeDelta) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual void Render(ID3D12GraphicsCommandList* _commandList) override;

protected:
	virtual HRESULT Ready_Components();
	virtual HRESULT Bind_ShaderResources();

private:
	// ÄÄÆ÷³ÍÆ®µé
	class CVIBuffer* m_pVIBufferCom = { nullptr };
	class CTexture* m_pTextureCom = { nullptr };

	ID3D12Device* m_pDevice = { nullptr };

public:
	static CCube* Create(EngineContext* _pcontext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;

};

