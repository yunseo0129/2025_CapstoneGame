#pragma once

#include "Base.h"

class CLevel abstract : public CBase
{
protected:
	CLevel(ID3D12Device* pDevice, EngineContext * pContext);
	virtual ~CLevel() = default;

public:
	virtual HRESULT Initialize();
	virtual void Update(_float fTimeDelta);
	virtual HRESULT Render();

	virtual void Add_Camera();

	void Bind_CameraBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex, CAMERA_TYPE _eType);

protected:
	ID3D12Device* m_pDevice = { nullptr };
	EngineContext* m_pContext = { nullptr };
	class CGameInstance* m_pGameInstance = { nullptr };
	vector<class CCamera*> m_pCamera { nullptr };
	

public:
	virtual void Free() override;
};