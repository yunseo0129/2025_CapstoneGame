#pragma once

#include "Level.h"

class CLevel_GamePlay final: public CLevel
{
private:
    CLevel_GamePlay(EngineContext* pContext);
    virtual ~CLevel_GamePlay() = default;

public:
    virtual HRESULT Initialize();
    virtual void   Update(_float fTimeDelta) override;
    virtual HRESULT Render() override;
    virtual void   Add_Camera() override;

    // [맵 RTT] 탑다운 맵 렌더 패스 오버라이드
    virtual _bool Begin_MapRTPass(ID3D12GraphicsCommandList* cmdList) override;
    virtual _bool Is_MapRTActive() const override { return (m_pMapRT != nullptr) && m_bMapRTActive; }
    virtual void  Render_MapRT(ID3D12GraphicsCommandList* cmdList) override;
    virtual void  End_MapRTPass(ID3D12GraphicsCommandList* cmdList) override;
    virtual _uint Get_MapRT_SRVIndex() const override;

    // 맵 RTT 카메라 영역 설정 (Game_Manager 등에서 호출)
    //  vCenterXZ: 창 중심 월드(x,z) / fHalfExtent: 반경 / vUpDirXZ: 위쪽 수평 방향
    virtual void Set_MapRTView(const _float3& vCenterXZ, _float fHalfExtent, const _float3& vUpDirXZ) override;
    // 이번 프레임 맵 RTT 를 그릴지 여부 (패널 열렸을 때만 true 로)
    virtual void Set_MapRTActive(_bool b) override { m_bMapRTActive = b; }

private:
    HRESULT Ready_Light();
    HRESULT Ready_Layer();
    HRESULT Ready_MapRT();

    // 네트워크 이벤트 처리 (PLAYER_ADD/REMOVE/MOVE 등)
    void    Process_NetworkEvents();

private:
    class CGame_Manager* m_pGameManager = nullptr;
    class CMapRenderTarget* m_pMapRT = nullptr;
    _bool  m_bMapRTActive = false;
    HRESULT Ready_UI();

public:
    static CLevel_GamePlay* Create(EngineContext* pContext);
    virtual void Free() override;
};