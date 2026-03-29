#pragma once
#include "Player_3rd.h"

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
	virtual HRESULT Ready_Components();

private:

public:
	static CPig_3rd* Create(EngineContext* _pcontext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};