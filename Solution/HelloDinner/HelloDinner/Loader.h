#pragma once

#include "Base.h"

class CLoader final: public CBase
{
private:
    CLoader(EngineContext* pContext);
    virtual ~CLoader() = default;

public:
    HRESULT Initialize(LEVELID eNextLevelID);
    HRESULT Loading();

#ifdef _DEBUG
public:
    void Show_Debug();
#endif

public:
    _bool   isFinished() const { return m_isFinished.load(); }
    LEVELID Get_NextLevelID() const { return m_eNextLevelID; }

    void    Get_LoadingText(_tchar* pOutBuffer, size_t bufferSize);

private:
    EngineContext* m_pContext = {nullptr};
    LEVELID                 m_eNextLevelID = {LEVEL_END};
    std::thread             m_LoadingThread;
    std::mutex              m_LoadingLock;
    class CGameInstance* m_pGameInstance = {nullptr};

private:
    std::atomic<_bool>      m_isFinished = {false};
    _tchar                  m_szLoadingText[MAX_PATH] = {};

private:
    HRESULT Loading_Level_Logo();
    HRESULT Loading_Level_GamePlay();
    HRESULT Loading_Level_MapLoading();

    void    Set_LoadingText(const _tchar* pText);

public:
    static CLoader* Create(EngineContext* pContext, LEVELID eNextLevelID);
    virtual void Free() override;
};
