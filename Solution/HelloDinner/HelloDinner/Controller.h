#pragma once
#include "Base.h"

class CController final : public CBase
{
	DECLARE_SINGLETON(CController)
private:
	CController();
	virtual ~CController() = default;

	enum KEYS { KEYS_A, KEYS_S, KEYS_D, KEYS_W, KEYS_SPACE, KEYS_CTRL, KEYS_END };

public:
	void Update_Controller(_float fTimeDelta);
	void Set_Player(class CPlayer_1rd* _pPlayer);
	void Set_MouseSensitive(_float _val)
	{
		m_fMouseSensitive = _val;
	}

private:
	void Input_Player(_float fTimeDelta);
	void Input_UI(_float fTimeDelta);
	void Update_Input();

private:
	class CGameInstance*	m_pGameInstance = nullptr;
	class CPlayer_1rd*		m_pPlayer = nullptr;
	_float					m_fMouseSensitive = 2.f;
	_bool					m_isKeyboardInput[KEYS_END];

public:
	static CController* Create();
	virtual void Free();
};