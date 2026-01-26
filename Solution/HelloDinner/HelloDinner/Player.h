#pragma once
#include "object.h"
#include "Camera.h"

class CPlayer : public CGameObject
{
public:
	CPlayer();
	~CPlayer() = default;

	void MouseEvent(FLOAT _timeElapsed);
	void KeyboardEvent(FLOAT _timeElapsed);
	virtual void Update(FLOAT _timeElapsed) override;

	void SetCamera(const shared_ptr<Camera>& _camera);

private:
	shared_ptr<Camera> m_pCamera;

	FLOAT m_fSpeed;
};