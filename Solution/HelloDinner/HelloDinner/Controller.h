#pragma once
#include "Base.h"
#include <map>

class CController final : public CBase
{
    DECLARE_SINGLETON(CController)
private:
    CController();
    virtual ~CController() = default;

    enum KEYS { KEYS_A, KEYS_S, KEYS_D, KEYS_W, KEYS_SPACE, KEYS_CTRL, KEYS_END };

public:
    void Update_Controller(_float fTimeDelta);
    void Set_Player(class CPlayer_1rd* _pPlayer);
    void Set_MouseSensitive(_float _val) { m_fMouseSensitive = _val; }
    void Clear_OtherPlayers();

private:
    void Update_Input();
    void Predict_Local(_float fTimeDelta);
    void Send_InputPacket(_float fTimeDelta);
    void Apply_ServerEvents(_float fTimeDelta);
    void Input_UI(_float fTimeDelta);

    unsigned char Build_KeyBitFlags() const;

    void Spawn_OtherPlayer(int id, const float* worldMatrix);
    void Remove_OtherPlayer(int id);
    void Move_OtherPlayer(int id, const float* worldMatrix);

private:
    class CGameInstance*    m_pGameInstance = nullptr;
    class CPlayer_1rd*      m_pPlayer = nullptr;
    _float                  m_fMouseSensitive = 2.f;
    _bool                   m_isKeyboardInput[KEYS_END] = {};

    _float                  m_fAccumMouseYaw = 0.f;
    _float                  m_fMouseYawThisFrame = 0.f;
    _float                  m_fMousePitchThisFrame = 0.f;
    _float                  m_fSendTimer = 0.f;
    static constexpr _float m_fSendInterval = 1.f / 20.f;

    std::map<int, class CPlayer_Pig*> m_otherPlayers;

public:
    static CController* Create();
    virtual void Free();
};