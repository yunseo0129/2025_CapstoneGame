#pragma once
#include "PartObj.h"

class CKetchup_Gun final : public CPartObj
{
public:
	struct KETCHUP_GUN_DESC : public CPartObj::PARTOBJ_DESC
	{
		_float3 			vPos = _float3(1.f, 1.f, 1.f);
		_float3				vScale = { 1.f, 1.f, 1.f };
		_uint				iModelLevelIndex = 0;
		_wstring			strModelTag = L"";
		const _float4x4* pSocketMatrix;
	};

private:
	CKetchup_Gun(EngineContext* pContext);
	CKetchup_Gun(const CKetchup_Gun& Prototype);
	virtual ~CKetchup_Gun() = default;

public:
	virtual HRESULT		Initialize_Prototype() override;
	virtual HRESULT		Initialize(void* pArg) override;
	virtual void		Priority_Update(_float fTimeDelta) override;
	virtual void		Update(_float fTimeDelta) override;
	virtual void		Late_Update(_float fTimeDelta) override;
	virtual void		Render(ID3D12GraphicsCommandList* _commandList) override;
	virtual void		ShadowRender(ID3D12GraphicsCommandList* _commandList) override;

private:
	HRESULT				Ready_Components();

private:
	class CModel* m_pModelCom = { nullptr };
	_wstring			m_strModelTag = L"";
	_uint				m_iModelLevelIndex = 0;
	const _float4x4* m_pSocketMatrix = { nullptr };

public:
	static CKetchup_Gun* Create(EngineContext* pContext);
	virtual CGameObject* Clone(void* pArg);
	virtual void Free() override;
};