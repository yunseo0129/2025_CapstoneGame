#ifndef InputDev_h__
#define InputDev_h__

#include "Base.h"

class CInput_Device : public CBase
{
private:
	CInput_Device(void);
	virtual ~CInput_Device(void) = default;

public:
	_byte	Get_DIKeyState(_ubyte byKeyID)
	{
		return m_cKey[byKeyID];
	}
	bool Key_Pressing(int _iKey)
	{
		return m_tKeyInfo[_iKey].bPress;
	}
	bool Key_Down(int _iKey)
	{
		return m_tKeyInfo[_iKey].bDown;
	}
	bool Key_Up(int _iKey)
	{
		return m_tKeyInfo[_iKey].bUp;
	}
	bool Mouse_Pressing(int _iKey)
	{
		return m_tMouseInfo[_iKey].bPress;
	}
	bool Mouse_Down(int _iKey)
	{
		return m_tMouseInfo[_iKey].bDown;
	}
	bool Mouse_Up(int _iKey)
	{
		return m_tMouseInfo[_iKey].bUp;
	}

	_byte Get_DIMouseState(Engine::MOUSEKEYSTATE eMouse) const
	{
		return m_tMouseState.rgbButtons[eMouse];
	}

	_long Get_DIMouseMove(Engine::MOUSEMOVESTATE eMouseState) const
	{
		switch (eMouseState)
		{
		case Engine::MOUSEMOVESTATE::DIMS_X:
			return m_tMouseState.lX;
		case Engine::MOUSEMOVESTATE::DIMS_Y:
			return m_tMouseState.lY;
		case Engine::MOUSEMOVESTATE::DIMS_Z:
			return m_tMouseState.lZ;
		default:
			return 0;
		}
	}

public:
	HRESULT Initialize(HINSTANCE hInst, HWND hWnd);
	void	Update_InputDev(void);

private:
	LPDIRECTINPUT8			m_pInputSDK = { nullptr };

private:
	LPDIRECTINPUTDEVICE8	m_pKeyBoard = { nullptr };
	LPDIRECTINPUTDEVICE8	m_pMouse = { nullptr };

private:
	list<unsigned char>		m_KeyList;
	unsigned char			m_cKey[256];
	KEYSTATE				m_tKeyInfo[256] = {};
	KEYSTATE				m_tMouseInfo[3] = {};
	DIMOUSESTATE			m_tMouseState = {};

public:
	static CInput_Device* Create(HINSTANCE hInst, HWND hWnd);
	virtual void	Free(void);

};
#endif // InputDev_h__