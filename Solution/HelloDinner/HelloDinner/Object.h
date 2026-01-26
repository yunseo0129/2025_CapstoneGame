#pragma once
#include "stdafx.h"
#include "mesh.h"
#include "Texture.h"

class CGameObject
{
public:
	CGameObject();
	~CGameObject() = default;

	virtual void Update(FLOAT _timeElapsed);
	virtual void Render(const ComPtr<ID3D12GraphicsCommandList>& _commandList) const;
	virtual void UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& _commandList) const;

	void Transform(XMFLOAT3 _shift);
	void Rotate(FLOAT _pitch, FLOAT _yaw, FLOAT _roll);

	void SetMesh(const shared_ptr<CMesh>& _mesh);
	void SetTexture(const shared_ptr<CTexture>& _texture);

	void SetPosition(XMFLOAT3 _position);
	XMFLOAT3 GetPosition() const;

protected:
	XMFLOAT4X4			m_xmf4x4WorldMatrix;

	XMFLOAT3			m_xmf3Right;
	XMFLOAT3			m_xmf3Up;
	XMFLOAT3			m_xmf3Front;

	shared_ptr<CMesh>	m_pMesh;
	shared_ptr<CTexture>	m_pTexture;
};

class CRotatingObject : public CGameObject
{
public:
	CRotatingObject();
	~CRotatingObject() = default;

	void Update(FLOAT _timeElapsed) override;

private:
	FLOAT m_fRotatingSpeed;
};
