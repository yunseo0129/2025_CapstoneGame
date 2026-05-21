#pragma once
#include "ContainerObj.h"
#include "Model.h"
#include "../Server/PlayerInfo.h"

class CPlayer_1rd final : public CContainerObj
{
public:
    struct Player_1RD_DESC : public CContainerObj::CONTAINEROBJ_DESC
    {
        _float3             vPos = _float3(1.f, 1.f, 1.f);
        _uint               iModelLevelIndex = 0;
        _float3             vRotation = {};
        _wstring            strModelTag = L"";
        class CCamera_FPV*  pCamera = nullptr;
    };
    enum PLAYER_1RD_COLLIDER_TYPE { COLLIDER_MAIN, COLLIDER_HEAD, COLLIDER_ARM_UP_L, COLLIDER_ARM_UP_R, COLLIDER_ARM_LOW_L, COLLIDER_ARM_LOW_R, COLLIDER_THIGH_L, COLLIDER_THIGH_R, COLLIDER_SHIN_L, COLLIDER_SHIN_R, COLLIDER_BODY, COLLIDER_END };

private:
    CPlayer_1rd(EngineContext* pContext);
    CPlayer_1rd(const CPlayer_1rd& Prototype);
    virtual ~CPlayer_1rd() = default;

public:
    virtual HRESULT     Initialize_Prototype() override;
    virtual HRESULT     Initialize(void* pArg) override;
    virtual void        Priority_Update(_float fTimeDelta) override;
    virtual void        Update(_float fTimeDelta) override;
    virtual void        Late_Update(_float fTimeDelta) override;
    virtual void        Render(ID3D12GraphicsCommandList* _commandList) override;
    virtual void        ShadowRender(ID3D12GraphicsCommandList* _commandList) override;

public:
    void PredictMove(unsigned char keyInput, float mouseYawDelta, float fTimeDelta);
    void Apply_ServerCorrection(const float* pServerMatrix, float fTimeDelta);
    const float* Get_PredictedMatrixPtr() const { return m_PredictedState.m; }

    void TurnPitch(_float _val);
    void Jump(_float _val);
    void Crouch(_float _val);

private:
    virtual HRESULT     Ready_PartObjects();
    virtual HRESULT     Ready_Components();

private:
    class CModel*               m_pModelCom = { nullptr };
    class CCamera_FPV*          m_pCamera = { nullptr };
    class CModel*               m_pFPSModelCom = { nullptr };
    _float4x4                   m_matFPSModel;
    vector<class CCollider*>    m_vColliderComs;
    vector<class CCollider*>    m_vMapColliderComs;
    _uint                       m_iState = 0;
    _int                        m_iHealth = 0;
    _wstring                    m_strModelTag = L"";
    _uint                       m_iModelLevelIndex = 0;
    _float                      m_fPitchRot = 0;

    WorldMatrixInfo             m_PredictedState{};
    bool                        m_bPredictionInit = false;
    float                       m_fPredictSpeed = 1.f;
    float                       m_fPredictRotation = 1.f;

    static constexpr float      SOFT_TOLERANCE = 0.05f;
    static constexpr float      HARD_TOLERANCE = 0.50f;
    static constexpr float      LERP_RATE = 12.f;

public:
    static CPlayer_1rd* Create(EngineContext* pContext);
    virtual CGameObject* Clone(void* pArg);
    virtual void Free() override;
};