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

private:
    HRESULT Ready_Light();
    HRESULT Ready_Layer();

    // 네트워크 이벤트 처리 (PLAYER_ADD/REMOVE/MOVE 등)
    void    Process_NetworkEvents();

public:
    static CLevel_GamePlay* Create(EngineContext* pContext);
    virtual void Free() override;
};