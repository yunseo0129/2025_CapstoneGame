#pragma once
#include "ContainerObj.h"
#include "Model.h"

class CPlayer_1rd abstract : public CContainerObj
{
public:
	struct Player_1RD_DESC : public CContainerObj::CONTAINEROBJ_DESC
	{
		_vector				vPos = {};
		_uint				iModelLevelIndex = 0;
		_wstring			strModelTag = L"";
	};

protected:
	CPlayer_1rd(EngineContext* pContext);
	CPlayer_1rd(const CPlayer_1rd& Prototype);
	virtual ~CPlayer_1rd() = default;

public:
	virtual HRESULT		Initialize_Prototype() override;
	virtual HRESULT		Initialize(void* pArg) override;
	virtual void		Priority_Update(_float fTimeDelta) override;
	virtual void		Update(_float fTimeDelta) override;
	virtual void		Late_Update(_float fTimeDelta) override;
	virtual void		Render(ID3D12GraphicsCommandList* _commandList) override;

	//CCollider* Get_CollisionCom() const { return m_pColliderCom; }
	virtual void TakeDamage(int iDamage) PURE;

protected:
	class CModel* m_pModelCom = { nullptr };
	// CCollider* m_pColliderCom = { nullptr };
	_uint				m_iState = 0;
	_int				m_iHealth = 0;
	_wstring			m_strModelTag = L"";
	_uint				m_iModelLevelIndex = 0;

protected:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();

public:
	static CPlayer_1rd* Create(EngineContext* pContext);
	virtual CGameObject* Clone(void* pArg);
	virtual void Free() override;


};