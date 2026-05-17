#pragma once

#include "Level.h"

class CLevel_Loading final : public CLevel
{
private:
	CLevel_Loading(EngineContext* pContext);
	virtual ~CLevel_Loading() = default;

public:
	virtual HRESULT Initialize(LEVELID eNextLevelID);
	virtual void Update(_float fTimeDelta) override;
	virtual HRESULT Render() override;

private:
	LEVELID			m_eNextLevelID = { LEVEL_END };
	
	class CLoader* m_pLoader = {nullptr};
	class CLoadingWindow* m_pLoadingWindow = {nullptr};

	class CLevel* Create_NextLevel();


public:
	static CLevel_Loading* Create(EngineContext* pContext, LEVELID eNextLevelID);
	virtual void Free() override;
};