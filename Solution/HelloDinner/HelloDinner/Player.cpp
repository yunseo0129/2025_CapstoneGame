#include "player.h"

CPlayer::CPlayer() : CGameObject(), m_fSpeed{ 5.0f }
{
}

void CPlayer::MouseEvent(FLOAT _timeElapsed)
{
}

void CPlayer::KeyboardEvent(FLOAT _timeElapsed)
{
	XMFLOAT3 front{ m_pCamera->GetN() }; front.y = 0.f;
	front = Vector3::Normalize(front);
	XMFLOAT3 back{ Vector3::Negate(front) };
	XMFLOAT3 right{ m_pCamera->GetU() };
	XMFLOAT3 left{ Vector3::Negate(right) };
	XMFLOAT3 direction{};

	if (GetAsyncKeyState('W') && GetAsyncKeyState('A') & 0x8000) {
		direction = Vector3::Normalize(Vector3::Add(front, left));
	}
	else if (GetAsyncKeyState('W') && GetAsyncKeyState('D') & 0x8000) {
		direction = Vector3::Normalize(Vector3::Add(front, right));
	}
	else if (GetAsyncKeyState('S') && GetAsyncKeyState('A') & 0x8000) {
		direction = Vector3::Normalize(Vector3::Add(back, left));
	}
	else if (GetAsyncKeyState('S') && GetAsyncKeyState('D') & 0x8000) {
		direction = Vector3::Normalize(Vector3::Add(back, right));
	}
	else if (GetAsyncKeyState('W') & 0x8000) {
		direction = front;
	}
	else if (GetAsyncKeyState('A') & 0x8000) {
		direction = left;
	}
	else if (GetAsyncKeyState('S') & 0x8000) {
		direction = back;
	}
	else if (GetAsyncKeyState('D') & 0x8000) {
		direction = right;
	}
	if (GetAsyncKeyState('W') || GetAsyncKeyState('A') ||
		GetAsyncKeyState('S') || (GetAsyncKeyState('D') & 0x8000)) {
		XMFLOAT3 angle{ Vector3::Angle(m_xmf3Front, direction) };
		XMFLOAT3 cross{ Vector3::Cross(m_xmf3Front, direction) };
		if (cross.y >= 0.f) {
			Rotate(0.f, XMConvertToDegrees(angle.y) * 10.f * _timeElapsed, 0.f);
		}
		else {
			Rotate(0.f, -XMConvertToDegrees(angle.y) * 10.f * _timeElapsed, 0.f);
		}
		Transform(Vector3::Mul(m_xmf3Front, m_fSpeed * _timeElapsed));
	}
}


void CPlayer::Update(FLOAT _timeElapsed)
{
	if (m_pCamera) m_pCamera->UpdateEye(GetPosition());
}

void CPlayer::SetCamera(const shared_ptr<Camera>& _camera)
{
	m_pCamera = _camera;
}
