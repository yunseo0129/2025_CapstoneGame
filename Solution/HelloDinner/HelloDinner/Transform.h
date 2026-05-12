#pragma once
#include "Component.h"

class CTransform final : public CComponent
{
public:
	enum STATE { STATE_RIGHT, STATE_UP, STATE_LOOK, STATE_POSITION, STATE_END };

	struct TRANSFORM_DESC
	{
		_float		fSpeedPerSec{};
		_float		fRotationPerSec{};

	};
private:
	CTransform(EngineContext* _pcontext);
	CTransform(const CTransform& Prototype);
	virtual ~CTransform() = default;

public:
	void Set_State(STATE eState, _fvector vState) {
		XMStoreFloat4((_float4*)&m_WorldMatrix.m[eState], vState);
	}

	_vector Get_State(STATE eState) const {
		return XMLoadFloat4x4(&m_WorldMatrix).r[eState];
	}

	_matrix Get_WorldMatrix() const {
		return XMLoadFloat4x4(&m_WorldMatrix);
	}

	_float Get_SpeedPerSec() const { return m_fSpeedPerSec; }

	const _float4x4* Get_WorldFloat4x4_Ptr() {
		return &m_WorldMatrix;
	}

	// 1인칭 카메라 전용 함수임. 다른 곳 사용 절대 금지!
	void Set_WorldMatrix(_float4x4 _Matrix) {
		m_WorldMatrix = _Matrix;
	}

	_matrix Get_WorldMatrix_Inverse() const {
		return XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_WorldMatrix));
	}

	_float3 Compute_Scaled();

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg);

public:
	void Scaling(_float fX, _float fY, _float fZ);
	void Go_Look(_float fTimeDelta);
	void Go_OppLook(_float fTimeDelta);
	void Go_Straight(_float fTimeDelta);
	void Go_Backward(_float fTimeDelta);
	void Go_Left(_float fTimeDelta);
	void Go_Right(_float fTimeDelta);
	void Go_Up(_float fTimeDelta);
	void Move(_vector vDir, _float fTimeDelta);

	void LookAt(_fvector vAt);

	/* 기존 회전을 기준으로 추가로 정해진 속도로 회전한다. */
	void Turn(_fvector vAxis, _float fTimeDelta);
	/* 항등상태를 기준으로 지정한 각도로 회전한다. */
	void Rotation(_fvector vAxis, _float fRadian);
	/* 항등상태 기준 쿼터니언 회전 */
	void RotationQuaternion(_float fX, _float fY, _float fZ);

	void EulerRotationQuaternion(_float fX, _float fY, _float fZ);

	// HRESULT Bind_ShaderResource(class CShader* pShader, const _char* pConstantName);

private:
	_float4x4				m_WorldMatrix = {};
	_float					m_fSpeedPerSec = 1.f;
	_float					m_fRotationPerSec = 1.f;

public:
	static CTransform* Create(EngineContext* _pcontext);
	virtual CComponent* Clone(void* pArg);
	virtual void Free() override;
};