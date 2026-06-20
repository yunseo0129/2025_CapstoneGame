#pragma once
#include "GameObject.h"

/*
    [CCharSelect_Pig]
    - 캐릭터 선택창에서 받침대 위에 세워둘 "프리뷰 전용" 캐릭터 오브젝트.
    - CPlayer_Pig 와 달리 총/콜라이더/입력 로직이 전혀 없는 경량 버전이다.
      (선택창에서는 모델을 보여주고 제자리 idle 애니메이션만 돌리면 충분하다)
    - 애니메이션 모델이라 인스턴스마다 자기 뼈 상태가 필요하므로
      모델은 Add_Component(Clone) 로 깊은 복사해서 가진다.
*/
class CModel;

class CCharSelect_Pig final: public CGameObject
{
public:
    struct CHARSELECT_PIG_DESC: public CGameObject::GAMEOBJECT_DESC
    {
        _float3   vPos = {0.f, 0.f, 0.f};
        _float3   vRotation = {0.f, 0.f, 0.f};   // 라디안 (카메라 보도록 보통 Y=PI)
        _float3   vScale = {1.f, 1.f, 1.f};
        _wstring  strModelTag = L"";               // 예) Prototype_Component_Pig_3rd
        _uint     iModelLevelIndex = 0;            // 예) LEVEL_CHARSELECT
        _uint     iAnimIndex = 0;                  // 0 = 정지 포즈(안전), 대기는 1/2
    };

private:
    CCharSelect_Pig(EngineContext* pContext);
    CCharSelect_Pig(const CCharSelect_Pig& Prototype);
    virtual ~CCharSelect_Pig() = default;

public:
    virtual HRESULT Initialize_Prototype();
    virtual HRESULT Initialize(void* pArg) override;

    virtual void    Priority_Update(_float fTimeDelta) override;
    virtual void    Update(_float fTimeDelta) override;
    virtual void    Late_Update(_float fTimeDelta) override;
    virtual void    Render(ID3D12GraphicsCommandList* _commandList) override;
    virtual void    ShadowRender(ID3D12GraphicsCommandList* _commandList) override;

private:
    HRESULT Ready_Components();

private:
    CModel* m_pModelCom = {nullptr};

    _wstring m_strModelTag = L"";
    _uint    m_iModelLevelIndex = 0;
    _uint    m_iAnimIndex = 0;

public:
    static CCharSelect_Pig* Create(EngineContext* pContext);
    virtual CGameObject* Clone(void* pArg) override;
    virtual void Free() override;
};
