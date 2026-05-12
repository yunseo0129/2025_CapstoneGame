#pragma once
#include "Camera.h"

class CCamera_FPV final : public CCamera
{
public:
	struct FPV_CAMERA_DESC : public CCamera::CAMERA_DESC
	{
		_float			fCamMouseSensor = {};
		_float			fCamSpeedPerSec = {};
	};

private:
	CCamera_FPV(EngineContext* pContext);
	CCamera_FPV(const CCamera_FPV& Prototype);
	virtual ~CCamera_FPV();

public:
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(_float fTimeDelta) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	void Set_WorldMatrix(_float4x4 _mat);

private:
	_float			m_fCamMouseSensor = {};
	_float			m_fCamSpeedPerSec = {};

public:
	static CCamera_FPV* Create(EngineContext* _pcontext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};