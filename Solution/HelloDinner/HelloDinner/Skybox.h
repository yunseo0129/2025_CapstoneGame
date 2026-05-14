#pragma once
#include "GameObject.h"

class CSkybox final : public CGameObject
{
public:
	struct Skybox_DESC : public CGameObject::GAMEOBJECT_DESC
	{
		_wstring	strVIbufferTag = L"";
		_wstring	strTextureTag = L"";
		_uint		iModelLevelIndex = 0;
	};

protected:
	CSkybox(EngineContext* _pcontext);
	CSkybox(const CSkybox& Prototype);
	virtual ~CSkybox() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(_float fTimeDelta) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual void Render(ID3D12GraphicsCommandList* _commandList) override;

protected:
	HRESULT Ready_Components();

protected:
	class CTexture* m_pTextureCom { nullptr };
	class CVIBuffer* m_pVIBufferCom{ nullptr };

	_wstring	m_strVIBufferTag = L"";
	_wstring	m_strTextureTag = L"";
	_uint		m_iModelLevelIndex = 0;

public:
	static CSkybox* Create(EngineContext* _pcontext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};

