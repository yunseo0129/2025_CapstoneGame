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
	void ShadowRender(ID3D12GraphicsCommandList* cmdList);
	void Set_CurrentCamera(CAMERA_TYPE _etype);
	void Bind_CameraBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex, CAMERA_TYPE _eType);
	void Bind_LightBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex);
	XMFLOAT4X4 Get_CurrentCameraView();
	XMFLOAT4X4 Get_CurrentCameraProjection();
	_int Get_CurrentLevelID() const { return m_iCurrentLevelID; }
    class CCamera* Get_CurrentCamera();

    // [Frustum Culling]
    _bool IsSphereInFrustum(const _float3& vCenter, _float fRadius);
    void  Set_CullingEnabled(_bool b) { m_bCullingEnabled = b; }
    _bool Is_CullingEnabled() const { return m_bCullingEnabled; }
    void  Reset_CullStats() { m_iCullTotal = 0; m_iCullRendered = 0; }
    void  Add_CullStat(_bool bRendered);
    _uint Get_CullStat_Total()    const { return m_iCullTotal; }
    _uint Get_CullStat_Rendered() const { return m_iCullRendered; }

private:
	_int					m_iCurrentLevelID = { -1 };
	class CLevel* m_pCurrentLevel = { nullptr };
	class CGameInstance* m_pGameInstance = { nullptr };

    _bool m_bCullingEnabled = true;
    
    // Culling 통계 Debug용
    _uint m_iCullTotal = 0;
    _uint m_iCullRendered = 0;
public:
	static CLevel_Manager* Create();
	virtual void Free() override;
};
