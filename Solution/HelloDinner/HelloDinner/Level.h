#pragma once

#include "Base.h"

class CLevel abstract : public CBase
{
protected:
	CLevel(EngineContext * pContext);
	virtual ~CLevel() = default;

public:
	virtual HRESULT Initialize();
	virtual void Update(_float fTimeDelta);
	virtual HRESULT Render();

	virtual void Add_Camera();

	void Bind_CameraBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex, CAMERA_TYPE _eType);
	XMFLOAT4X4 Get_CurrentCameraView () { return m_xmf4x4CurrentView; }
	XMFLOAT4X4 Get_CurrentCameraProjection () { return m_xmf4x4CurrentProjection; }

private:
	void Get_CameraMatrix ( CAMERA_TYPE _eType );

protected:
	EngineContext* m_pContext = { nullptr };
	class CGameInstance* m_pGameInstance = { nullptr };
	vector<class CCamera*> m_pCamera { nullptr };
	
	XMFLOAT4X4 m_xmf4x4CurrentView;
	XMFLOAT4X4 m_xmf4x4CurrentProjection;

public:
	virtual void Free() override;
};