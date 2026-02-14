#pragma once

#include "Base.h"

class CPipeLine final : public CBase
{
public:
	enum D3DTRANSFORMSTATE { D3DTS_VIEW, D3DTS_PROJ, D3DTS_END };
private:
	CPipeLine();
	virtual ~CPipeLine() = default;

public:
	void Set_Transform(D3DTRANSFORMSTATE eState, _fmatrix TransformMatrix) {
		XMStoreFloat4x4(&m_TransformMatrices[eState], TransformMatrix);
	}

	_matrix Get_TransformMatrix(D3DTRANSFORMSTATE eState) {
		return XMLoadFloat4x4(&m_TransformMatrices[eState]);
	}
	_matrix Get_ViewProjMatrix() {
		return m_ViewProjMatrix;
	}
	_float4x4 Get_TransformFloat4x4(D3DTRANSFORMSTATE eState) {
		return m_TransformMatrices[eState];
	}
	const _float4* Get_CamPosition() const {
		return &m_vCamPosition;
	}
public:
	HRESULT Initialize();
	void Update();

private:
	_float4x4				m_TransformMatrices[D3DTS_END];
	_float4x4				m_TransformInverseMatrices[D3DTS_END];
	_matrix					m_ViewProjMatrix;
	_float4					m_vCamPosition = {};

public:
	static CPipeLine* Create();
	virtual void Free() override;
};