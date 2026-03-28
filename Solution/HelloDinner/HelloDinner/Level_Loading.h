#pragma once

#include "Level.h"

/* 로딩화면을 구성하기위한 객체(로딩화면의 배경, 로딩 바, 로딩 폰트)를 만들어낸다. */
/* 다음 레벨에 필요한 자원을 로드하는 역활을 하는 객체를 생성해준다.  */

class CLevel_Loading final : public CLevel
{
private:
	CLevel_Loading(EngineContext* pContext);
	virtual ~CLevel_Loading() = default;

public:
	virtual HRESULT Initialize(LEVELID eNextLevelID);
	virtual void Update(_float fTimeDelta) override;
	virtual HRESULT Render() override;
	virtual void Add_Camera() override;

private:
	LEVELID			m_eNextLevelID = { LEVEL_END };

	HRESULT Ready_TestLoader();

public:
	static CLevel_Loading* Create(EngineContext* pContext, LEVELID eNextLevelID);
	virtual void Free() override;
};