#pragma once
#include "Base.h"
#include <map>

class CController final : public CBase
{
    DECLARE_SINGLETON(CController)
private:
    CController();
    virtual ~CController() = default;

public:
    void Update_Controller(_float fTimeDelta);
    void Set_Player(class CPlayer_1rd* _pPlayer);
    void Set_MouseSensitive(_float _val) { m_fMouseSensitive = _val; }
    void Clear_OtherPlayers();
	enum KEYS { KEYS_A, KEYS_S, KEYS_D, KEYS_W, KEYS_SPACE, KEYS_CTRL, KEYS_SHIFT, KEYS_R, KEYS_END };
    enum MOUSE { MOUSE_LB, MOUSE_RB, MOUSE_MB, MOUSE_END };
    void Set_Crosshair(class CUI_Crosshair* _pCrosshair);
    class CPlayer_1rd* Get_Player() const { return m_pPlayer; }     // 상점에서 무기 교체 시 사용

    // 상점 창 열었을 때 움직임 없게 하려고 만듦.(키보드/마우스 입력 무시)
    void Set_BlockInput(_bool _b) { m_bBlockInput = _b; }
    _bool Is_InputBlocked() const { return m_bBlockInput; }

    // 캐릭터 선택 정보 수신 시 호출 — 이미 스폰된 원격 플레이어면 올바른 모델로 재생성
    void Set_OtherPlayerCharType(int id, unsigned char charType);
    // 원격 플레이어 포물선 발사 (PLAYING 진입 시 Apply_SpawnLaunch에서 호출)
    void Launch_OtherPlayer(int id, const _float3& vTarget, float arc);
    // 원격 플레이어 즉시 위치 이동 (발사 전 싱크대에 세우기)
    void Teleport_OtherPlayer(int id, const _float3& vPos);
    // 원격 플레이어 사망 처리 (SC_DEATH 수신 시 호출)
    void Kill_OtherPlayer(int id);

private:
    void Update_Input();
    void Predict_Local(_float fTimeDelta);
    void Send_InputPacket(_float fTimeDelta);
    void Apply_ServerEvents(_float fTimeDelta);
    void Input_Player(_float fTimeDelta);
    void Input_UI(_float fTimeDelta);

    unsigned short Build_KeyBitFlags() const;

    void Spawn_OtherPlayer(int id, const float* worldMatrix);
    void Remove_OtherPlayer(int id);
    void Move_OtherPlayer(int id, const float* worldMatrix, unsigned short keyInput = 0);

private:
    class CGameInstance*    m_pGameInstance = nullptr;
    class CPlayer_1rd*      m_pPlayer = nullptr;
    class CUI_Crosshair* m_pCrosshair = nullptr;
    _float                  m_fMouseSensitive = 2.f;
    _bool                   m_isKeyboardInput[KEYS_END] = {};

    _float                  m_fAccumMouseYaw = 0.f;
    _float                  m_fMouseYawThisFrame = 0.f;
    _float                  m_fMousePitchThisFrame = 0.f;
    _float                  m_fSendTimer = 0.f;
    static constexpr _float m_fSendInterval = 1.f / 20.f;

    std::map<int, class CPlayer_Pig*> m_otherPlayers;
    std::map<int, unsigned char>      m_otherCharType; // id → 선택한 캐릭터(0=Pig,1=Chick,2=Fish)

    _bool                   m_isMouseInput[MOUSE_END] = {};
    _bool                   m_isPreMouseInput[MOUSE_END] = {};

    // 입력 차단 여부 (상점 창 열었을 때 움직임 없게 하려고 만듦)
    _bool                   m_bBlockInput = false;

    // 서버 에코 keyInput 상승 에지 감지용 (이전 프레임 값 보존)
    unsigned short          m_prevServerKeyInput = 0;
public:
    static CController* Create();
    virtual void Free();
};