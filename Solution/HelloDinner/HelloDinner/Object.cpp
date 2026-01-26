#include "Object.h"

CGameObject::CGameObject() : m_xmf3Right{ 1.f, 0.f, 0.f }, m_xmf3Up{ 0.f, 1.f, 0.f }, m_xmf3Front{ 0.f, 0.f, 1.f }
{
	XMStoreFloat4x4(&m_xmf4x4WorldMatrix, XMMatrixIdentity());
}

void CGameObject::Update(FLOAT _timeElapsed)
{

}

void CGameObject::Render(const ComPtr<ID3D12GraphicsCommandList>& _commandList) const
{
	UpdateShaderVariable(_commandList);
	m_pMesh->Render(_commandList);
}

void CGameObject::UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& _commandList) const
{
	XMFLOAT4X4 worldMatrix;
	XMStoreFloat4x4(&worldMatrix, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4WorldMatrix)));
	_commandList->SetGraphicsRoot32BitConstants(0, 16, &worldMatrix, 0);

	if (m_pTexture) m_pTexture->UpdateShaderVariable(_commandList);
}

void CGameObject::Transform(XMFLOAT3 _shift)
{
	SetPosition(Vector3::Add(GetPosition(), _shift));
}

void CGameObject::Rotate(FLOAT _pitch, FLOAT _yaw, FLOAT _roll)
{
	XMMATRIX rotate{ XMMatrixRotationRollPitchYaw(XMConvertToRadians(_pitch), XMConvertToRadians(_yaw), XMConvertToRadians(_roll)) };
	XMStoreFloat4x4(&m_xmf4x4WorldMatrix, rotate * XMLoadFloat4x4(&m_xmf4x4WorldMatrix));

	XMStoreFloat3(&m_xmf3Right, XMVector3TransformNormal(XMLoadFloat3(&m_xmf3Right), rotate));
	XMStoreFloat3(&m_xmf3Up, XMVector3TransformNormal(XMLoadFloat3(&m_xmf3Up), rotate));
	XMStoreFloat3(&m_xmf3Front, XMVector3TransformNormal(XMLoadFloat3(&m_xmf3Front), rotate));
}

void CGameObject::SetMesh(const shared_ptr<CMesh>& _mesh)
{
	m_pMesh = _mesh;
}

void CGameObject::SetTexture(const shared_ptr<CTexture>& _texture)
{
	m_pTexture = _texture;
}

void CGameObject::SetPosition(XMFLOAT3 _position)
{
	m_xmf4x4WorldMatrix._41 = _position.x;
	m_xmf4x4WorldMatrix._42 = _position.y;
	m_xmf4x4WorldMatrix._43 = _position.z;
}

XMFLOAT3 CGameObject::GetPosition() const
{
	return XMFLOAT3{ m_xmf4x4WorldMatrix._41, m_xmf4x4WorldMatrix._42, m_xmf4x4WorldMatrix._43 };
}

CRotatingObject::CRotatingObject() : CGameObject(), m_fRotatingSpeed{ }
{
}

void CRotatingObject::Update(FLOAT _timeElapsed)
{
	Rotate(0.f, m_fRotatingSpeed * _timeElapsed, 0.f);
}
