#pragma once

#include "GameObject.h"

class CCamera abstract : public CGameObject
{
public:
	struct CAMERA_DESC : public CGameObject::GAMEOBJECT_DESC
	{
		_float3			vEye = {};
		_float3			vAt = {};

		_float			fFovy = { 0.f };
		_float			fAspect = { 0.f };
		_float			fNear = { 0.f };
		_float			fFar = { 0.f };
	};

protected:
	CCamera(EngineContext* pContext);
	CCamera(const CCamera& Prototype);
	virtual ~CCamera() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg);
	virtual void Priority_Update(_float fTimeDelta);
	virtual void Update(_float fTimeDelta);
	virtual void Late_Update(_float fTimeDelta);
	virtual HRESULT Render();

	_float			Get_RotR() const { return m_fRotR; }
	_float			Get_RotY() const { return m_fRotY; }
	void			Move(_vector vTarget) { m_pTransformCom->Set_State(CTransform::STATE_POSITION, vTarget); }

protected:
	_float			m_fFovy = { 0.f };
	_float			m_fAspect = { 0.f };
	_float			m_fNear = { 0.f };
	_float			m_fFar = { 0.f };
	_float			m_fRotR = { 0.f };
	_float			m_fRotY = { 0.f };

protected:
	// Pipeline에 카메라 관련 행렬들을 계산해서 넘겨주는 함수
	void Compute_PipeLineMatrices();


public:
	virtual CGameObject* Clone(void* pArg) = 0;
	virtual void Free() override;
};