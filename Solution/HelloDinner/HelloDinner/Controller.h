#pragma once
#include "Base.h"

class CController final : public CBase
{
	DECLARE_SINGLETON(CController)
private:
	CController();
	virtual ~CController() = default;

	enum KEYS { KEYS_A, KEYS_S, KEYS_D, KEYS_W, KEYS_SPACE, KEYS_CTRL, KEYS_SHIFT, KEYS_R, KEYS_END };
    enum MOUSE { MOUSE_LB, MOUSE_RB, MOUSE_MB, MOUSE_END };
public:
	void Update_Controller(_float fTimeDelta);
	void Set_Player(class CPlayer_1rd* _pPlayer);
    class CPlayer_1rd* Get_Player() const { return m_pPlayer; }     // 상점에서 무기 교체 시 사용
	void Set_MouseSensitive(_float _val)
	{
		m_fMouseSensitive = _val;
	}

    // 상점 창 열었을 때 움직임 없게 하려고 만듦.(키보드/마우스 입력 무시)
    void Set_BlockInput(_bool _b) { m_bBlockInput = _b; }
    _bool Is_InputBlocked() const { return m_bBlockInput; }

private:
	void Input_Player(_float fTimeDelta);
	void Input_UI(_float fTimeDelta);
	void Update_Input();

private:
	class CGameInstance*	m_pGameInstance = nullptr;
	class CPlayer_1rd*		m_pPlayer = nullptr;
	_float					m_fMouseSensitive = 2.f;
	_bool					m_isKeyboardInput[KEYS_END];
    _bool                   m_isMouseInput[MOUSE_END];
    _bool                   m_isPreMouseInput[MOUSE_END];

    // 입력 차단 여부 (상점 창 열었을 때 움직임 없게 하려고 만듦)
    _bool                   m_bBlockInput = false;
public:
	static CController* Create();
	virtual void Free();
};