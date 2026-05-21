#pragma once
#include "Base.h"

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

private:
    void Update_Input();
    void Predict_Local(_float fTimeDelta);       // 본인 캐릭터에 즉시 예측 이동 적용
    void Send_InputPacket(_float fTimeDelta);    // 키 입력을 서버로 전송
    void Apply_ServerEvents(_float fTimeDelta);  // 서버 결과로 보정
    void Input_UI(_float fTimeDelta);

    unsigned char Build_KeyBitFlags() const;

private:
    class CGameInstance*    m_pGameInstance = nullptr;
    class CPlayer_1rd*      m_pPlayer = nullptr;
    _float                  m_fMouseSensitive = 2.f;
    _bool                   m_isKeyboardInput[KEYS_END] = {};

    _float                  m_fAccumMouseYaw = 0.f;        // 전송 주기 사이 Yaw 누산
    _float                  m_fMouseYawThisFrame = 0.f;   // 이번 프레임 Yaw (예측용)
    _float                  m_fMousePitchThisFrame = 0.f; // 이번 프레임 Pitch (예측용)
    _float                  m_fSendTimer = 0.f;
    static constexpr _float m_fSendInterval = 1.f / 20.f;  // 초당 20회 전송

public:
    static CController* Create();
    virtual void Free();
};