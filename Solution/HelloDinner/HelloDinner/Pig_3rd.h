#pragma once
#include "Player_3rd.h"
#include "Collider.h"

class CPig_3rd : public CPlayer_3rd
{
public:
	enum PIG_3RD_STATE {
		STATE_NONE,
		STATE_IDLE
	};

private:
	CPig_3rd(EngineContext* _pcontext);
	CPig_3rd(const CPig_3rd& Prototype);
	virtual ~CPig_3rd() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(_float fTimeDelta) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual void Render(ID3D12GraphicsCommandList* _commandList) override;

private:
	HRESULT Ready_Components();

private:
	class CCollider* m_pMainColliderCom = { nullptr };
	class CCollider* m_pHeadColliderCom = { nullptr };
	class CCollider* m_pBodyColliderCom = { nullptr };
	class CCollider* m_pLLegColliderCom = { nullptr };
	class CCollider* m_pRLegColliderCom = { nullptr };
	class CCollider* m_pRUpperArmColliderCom = { nullptr };
	class CCollider* m_pLUpperArmColliderCom = { nullptr };
	class CCollider* m_pRLowerArmColliderCom = { nullptr };
	class CCollider* m_pLLowerArmColliderCom = { nullptr };

public:
	static CPig_3rd* Create(EngineContext* _pcontext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};