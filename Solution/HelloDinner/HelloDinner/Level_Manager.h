#pragma once

#include "Base.h"


/* 1. 현재 화면에 보여줘야할 레벨객체를 들고 있는다. */
/* 2. 새로운 레벨로 교체하고 기존레베릉ㄹ 삭제한다. */
/* 2_1. 기존 레벨용 자원(리소스, 게임오브젝트)들을 삭제한다. */
/* 3. 활성화된 레벨의 반복적인 업데이트를 수행한다. */

class CLevel_Manager final : public CBase
{
private:
	CLevel_Manager();
	virtual ~CLevel_Manager() = default;

public:
	HRESULT Open_Level(_int iLevelIndex, class CLevel* pNewLevel);
	void Update(_float fTimeDelta);
	HRESULT Render();
	void Bind_CameraBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex, CAMERA_TYPE _eType);
	XMFLOAT4X4 Get_CurrentCameraView();
	XMFLOAT4X4 Get_CurrentCameraProjection();

private:
	_int					m_iCurrentLevelID = { -1 };
	class CLevel* m_pCurrentLevel = { nullptr };
	class CGameInstance* m_pGameInstance = { nullptr };

public:
	static CLevel_Manager* Create();
	virtual void Free() override;
};
