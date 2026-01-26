#include "Camera.h"

Camera::Camera() : m_eye{ 0.f, 0.f, 0.f }, m_at{ 0.f, 0.f, 1.f }, m_up{ 0.f, 1.f, 0.f },
m_u{ 1.f, 0.f, 0.f }, m_v{ 0.f, 1.f, 0.f }, m_n{ 0.f, 0.f, 1.f }
{
	XMStoreFloat4x4(&m_viewMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&m_projectionMatrix, XMMatrixIdentity());
}

void Camera::UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& commandList)
{
	XMStoreFloat4x4(&m_viewMatrix, XMMatrixLookAtLH(XMLoadFloat3(&m_eye), XMLoadFloat3(&m_at), XMLoadFloat3(&m_up)));

	XMFLOAT4X4 viewMatrix;
	XMStoreFloat4x4(&viewMatrix, XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix)));
	commandList->SetGraphicsRoot32BitConstants(1, 16, &viewMatrix, 0);

	XMFLOAT4X4 projectionMatrix;
	XMStoreFloat4x4(&projectionMatrix, XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix)));
	commandList->SetGraphicsRoot32BitConstants(1, 16, &projectionMatrix, 16);
}

void Camera::SetLens(FLOAT fovy, FLOAT aspect, FLOAT minZ, FLOAT maxZ)
{
	XMStoreFloat4x4(&m_projectionMatrix, XMMatrixPerspectiveFovLH(fovy, aspect, minZ, maxZ));
}

XMFLOAT3 Camera::GetEye() const
{
	return m_eye;
}

XMFLOAT3 Camera::GetU() const
{
	return m_u;
}

XMFLOAT3 Camera::GetV() const
{
	return m_v;
}

XMFLOAT3 Camera::GetN() const
{
	return m_n;
}

void Camera::UpdateBasis()
{
	m_n = Vector3::Normalize(Vector3::Sub(m_at, m_eye));
	m_u = Vector3::Normalize(Vector3::Cross(m_up, m_n));
	m_v = Vector3::Normalize(Vector3::Cross(m_n, m_u));
}

ThirdPersonCamera::ThirdPersonCamera() : Camera{}, m_radius{ 45.0f },
m_phi{ XM_PIDIV2 - 0.3f }, m_theta{ 0.0f }
{

}

void ThirdPersonCamera::Update(FLOAT timeElapsed)
{

}

void ThirdPersonCamera::UpdateEye(XMFLOAT3 position)
{
	XMFLOAT3 offset{
		static_cast<FLOAT>(m_radius * sin(m_phi) * cos(m_theta)),
		static_cast<FLOAT>(m_radius * cos(m_phi)),
		static_cast<FLOAT>(m_radius * sin(m_phi) * sin(m_theta))
	};
	m_eye = Vector3::Add(position, offset);
	m_at = position;
	UpdateBasis();
}

void ThirdPersonCamera::RotatePitch(FLOAT radian)
{
	m_phi += radian;
	m_phi = std::clamp(m_phi, XM_PIDIV2 - 0.6f, XM_PIDIV2 + 0.2f);
}

void ThirdPersonCamera::RotateYaw(FLOAT radian)
{
	m_theta += radian;
}
