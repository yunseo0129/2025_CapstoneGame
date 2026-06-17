#include "Input_Device.h"

CInput_Device::CInput_Device(void)
{

}

HRESULT CInput_Device::Initialize(HINSTANCE hInst, HWND hWnd)
{

	// DInput 컴객체를 생성하는 함수
	if (FAILED(DirectInput8Create(hInst,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(void**)&m_pInputSDK, nullptr)))
		return E_FAIL;


	// 키보드 객체 생성
	if (FAILED(m_pInputSDK->CreateDevice(GUID_SysKeyboard, &m_pKeyBoard, nullptr)))
		return E_FAIL;


	// 생성된 키보드 객체의 대한 정보를 컴 객체에게 전달하는 함수
	m_pKeyBoard->SetDataFormat(&c_dfDIKeyboard);

	// 장치에 대한 독점권을 설정해주는 함수, (클라이언트가 떠있는 상태에서 키 입력을 받을지 말지를 결정하는 함수)
	m_pKeyBoard->SetCooperativeLevel(hWnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);

	// 장치에 대한 access 버전을 받아오는 함수
	m_pKeyBoard->Acquire();


	// 마우스 객체 생성
	if (FAILED(m_pInputSDK->CreateDevice(GUID_SysMouse, &m_pMouse, nullptr)))
		return E_FAIL;

	// 생성된 마우스 객체의 대한 정보를 컴 객체에게 전달하는 함수
	m_pMouse->SetDataFormat(&c_dfDIMouse);

	// 장치에 대한 독점권을 설정해주는 함수, 클라이언트가 떠있는 상태에서 키 입력을 받을지 말지를 결정하는 함수
	m_pMouse->SetCooperativeLevel(hWnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);

	// 장치에 대한 access 버전을 받아오는 함수
	m_pMouse->Acquire();


	return S_OK;
}

void CInput_Device::Update_InputDev(void)
{
	HRESULT result = m_pKeyBoard->GetDeviceState(256, m_cKey);

	if (FAILED(result))
	{
		if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED)
		{
			m_pKeyBoard->Acquire();
		}
	}

	m_pMouse->GetDeviceState(sizeof(m_tMouseState), &m_tMouseState);

	for (int i = 0; i < 256; ++i)
	{
		if (Get_DIKeyState(i) & 0x80)
		{
			if (!m_tKeyInfo[i].bPress && !m_tKeyInfo[i].bDown)
			{
				m_tKeyInfo[i].bPress = true;
				m_tKeyInfo[i].bDown = true;
			}
			else if (m_tKeyInfo[i].bPress && m_tKeyInfo[i].bDown)
			{
				m_tKeyInfo[i].bDown = false;
			}
		}

		else if (m_tKeyInfo[i].bPress)
		{
			m_tKeyInfo[i].bPress = false;
			m_tKeyInfo[i].bDown = false;
			m_tKeyInfo[i].bUp = true;
		}

		else if (m_tKeyInfo[i].bUp)
			m_tKeyInfo[i].bUp = false;
	}


	Engine::MOUSEKEYSTATE eMouse;
	for (int i = 0; i < 3; ++i)
	{
		switch (i)
		{
		case 0:
			eMouse = Engine::MOUSEKEYSTATE::DIM_LB;
			break;
		case 1:
			eMouse = Engine::MOUSEKEYSTATE::DIM_RB;
			break;
		case 2:
			eMouse = Engine::MOUSEKEYSTATE::DIM_MB;
			break;
		default:
			break;
		}

		if (Get_DIMouseState(eMouse) & 0x80)
		{
			if (!m_tMouseInfo[i].bPress && !m_tMouseInfo[i].bDown)
			{
				m_tMouseInfo[i].bPress = true;
				m_tMouseInfo[i].bDown = true;
			}
			else if (m_tMouseInfo[i].bPress && m_tMouseInfo[i].bDown)
			{
				m_tMouseInfo[i].bDown = false;
			}
		}

		else if (m_tMouseInfo[i].bPress)
		{
			m_tMouseInfo[i].bPress = false;
			m_tMouseInfo[i].bDown = false;
			m_tMouseInfo[i].bUp = true;
		}

		else if (m_tMouseInfo[i].bUp)
			m_tMouseInfo[i].bUp = false;
	}
}

CInput_Device* CInput_Device::Create(HINSTANCE hInst, HWND hWnd)
{
	CInput_Device* pInstance = new CInput_Device();

	if (FAILED(pInstance->Initialize(hInst, hWnd)))
	{
		MSG_BOX("Failed to Created : CInput_Device");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CInput_Device::Free(void)
{
	Safe_Release(m_pKeyBoard);
	Safe_Release(m_pMouse);
	Safe_Release(m_pInputSDK);
}

