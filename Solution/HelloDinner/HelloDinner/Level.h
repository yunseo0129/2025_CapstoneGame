#pragma once

#include "Base.h"

class CLevel abstract: public CBase
{
protected:
    CLevel(EngineContext* pContext);
    virtual ~CLevel() = default;

public:
    virtual HRESULT Initialize();
    virtual void Update(_float fTimeDelta);
    virtual HRESULT Render();

    virtual void Update_Shadows(_float fTimeDelta);
    virtual void Begin_ShadowPass(ID3D12GraphicsCommandList* cmdList);
    virtual void End_ShadowPass(ID3D12GraphicsCommandList* cmdList);
    _bool Get_ShadowLightVP(_float4x4& outView, _float4x4& outProj) const;   // [파편 그림자]
    _bool IsSphereInShadowFrustum(const _float3& vCenter, _float fRadius) const;

    // [맵 RTT] 탑다운 맵 렌더 타겟 패스. 기본 no-op (Gameplay 레벨만 오버라이드).
    //  반환 false 면 이번 프레임에 그릴 게 없음(패스 스킵 가능).
    virtual _bool Begin_MapRTPass(ID3D12GraphicsCommandList* cmdList) { return false; }
    virtual _bool Is_MapRTActive() const { return false; }
    virtual void  Render_MapRT(ID3D12GraphicsCommandList* cmdList) {}
    virtual void  End_MapRTPass(ID3D12GraphicsCommandList* cmdList) {}
    virtual _uint Get_MapRT_SRVIndex() const { return 0; }
    virtual void  Set_MapRTView(const _float3& vCenterXZ, _float fHalfExtent, const _float3& vUpDirXZ) {}
    virtual void  Set_MapRTActive(_bool b) {}


    virtual void Add_Camera();

    void Bind_CameraBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex, CAMERA_TYPE _eType);
    void Set_CurrentCamera(CAMERA_TYPE _eType) { m_pCurrentCamera = m_pCamera[_eType]; }
    class CCamera* Get_CurrentCamera() const { return m_pCurrentCamera; }
    XMFLOAT4X4 Get_CurrentCameraView() { return m_xmf4x4CurrentView; }
    XMFLOAT4X4 Get_CurrentCameraProjection() { return m_xmf4x4CurrentProjection; }

    void Bind_LightBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex);

    const DirectX::BoundingFrustum* Get_CurrentFrustum() const;
    const DirectX::BoundingSphere* Get_ShadowBounds() const;

private:
    void Get_CameraMatrix(CAMERA_TYPE _eType);

protected:
    EngineContext* m_pContext = {nullptr};
    class CGameInstance* m_pGameInstance = {nullptr};

    vector<class CCamera*> m_pCamera {nullptr};
    vector<class CLight*> m_pLights;
    vector<class CShadow*> m_pShadows;

    class CCamera* m_pCurrentCamera = {nullptr};
    XMFLOAT4X4 m_xmf4x4CurrentView;
    XMFLOAT4X4 m_xmf4x4CurrentProjection;

public:
    virtual void Free() override;
};