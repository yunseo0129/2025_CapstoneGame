#include "Base.h"

using namespace Engine;

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

	_byte	Get_DIMouseState(MOUSEKEYSTATE eMouse)
	{
		return m_tMouseState.rgbButtons[eMouse];
	}

	_long Get_DIMouseMove(MOUSEMOVESTATE eMouseState)
	{
		switch (eMouseState)
		{
		case MOUSEMOVESTATE::DIMS_X:
			return m_tMouseState.lX;
		case MOUSEMOVESTATE::DIMS_Y:
			return m_tMouseState.lY;
		case MOUSEMOVESTATE::DIMS_Z:
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
	KEYSTATE				m_tKeyInfo[256] = {};		// 키보드에 있는 모든 키값을 저장하기 위한 변수
	KEYSTATE				m_tMouseInfo[3] = {};
	DIMOUSESTATE			m_tMouseState = {};

public:
	static CInput_Device* Create(HINSTANCE hInst, HWND hWnd);
	virtual void	Free(void);

};
