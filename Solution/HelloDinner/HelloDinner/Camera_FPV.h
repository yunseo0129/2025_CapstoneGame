#pragma once
#include "Camera.h"

class CCamera_FPV final : public CCamera
{
public:
	typedef struct : public CCamera::CAMERA_DESC
	{
		_float			fMouseSensor = {};
		_float			fSpeedPerSec = {};
	}FPV_CAMERA_DESC;

private:
	CCamera_FPV(EngineContext* pContext);
	CCamera_FPV(const CCamera_FPV& Prototype);
	virtual ~CCamera_FPV();

public:
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(_float fTimeDelta) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;

private:
	_float			m_fMouseSensor = {};
	_float			m_fSpeedPerSec = {};

public:
	static CCamera_FPV* Create(EngineContext* _pcontext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};