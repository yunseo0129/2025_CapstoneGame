#pragma once
#include "Base.h"

class CController final : public CBase
{
	DECLARE_SINGLETON(CController)
private:
	CController();
	virtual ~CController() = default;

public:
	void Update_Controller(_float fTimeDelta);
	void Set_Player(class CPlayer_1rd* _pPlayer);

private:
	void Input_Player(_float fTimeDelta);
	void Input_UI(_float fTimeDelta);

private:
	class CGameInstance* m_pGameInstance = nullptr;
	class CPlayer_1rd* m_pPlayer = nullptr;

public:
	static CController* Create();
	virtual void Free();
};