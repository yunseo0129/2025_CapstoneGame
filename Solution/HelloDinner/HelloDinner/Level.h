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
	virtual void ShadowRender_Begin();
	virtual void ShadowRender(ID3D12GraphicsCommandList* cmdList);
	virtual HRESULT Render();

	virtual void Add_Camera();

	void Bind_CameraBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex, CAMERA_TYPE _eType);
    void Set_CurrentCamera(CAMERA_TYPE _eType) { m_pCurrentCamera = m_pCamera[_eType]; }
    class CCamera* Get_CurrentCamera() const { return m_pCurrentCamera; }
	XMFLOAT4X4 Get_CurrentCameraView () { return m_xmf4x4CurrentView; }
	XMFLOAT4X4 Get_CurrentCameraProjection () { return m_xmf4x4CurrentProjection; }

	void Bind_LightBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex);

private:
	void Get_CameraMatrix ( CAMERA_TYPE _eType );

protected:
	EngineContext* m_pContext = { nullptr };
	class CGameInstance* m_pGameInstance = { nullptr };

	vector<class CCamera*> m_pCamera { nullptr };
	vector<class CLight*> m_pLights;
	vector<class CShadow*> m_pShadows;
	
	class CCamera* m_pCurrentCamera = { nullptr };
	XMFLOAT4X4 m_xmf4x4CurrentView;
	XMFLOAT4X4 m_xmf4x4CurrentProjection;

public:
	virtual void Free() override;
};