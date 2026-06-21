#pragma once
#include "ContainerObj.h"
#include "Model.h"

class CPlayer_Fish final : public CContainerObj
{
public:
    struct PLAYER_FISH_DESC : public CContainerObj::CONTAINEROBJ_DESC
    {
        _float3 			vPos = _float3(1.f, 1.f, 1.f);
        _uint				iModelLevelIndex = 0;
        _float3				vRotation = {};
        _wstring			strModelTag = L"";
    };

private:
    CPlayer_Fish(EngineContext* pContext);
    CPlayer_Fish(const CPlayer_Fish& Prototype);
    virtual ~CPlayer_Fish() = default;

public:
    virtual HRESULT		Initialize_Prototype() override;
    virtual HRESULT		Initialize(void* pArg) override;
    virtual void		Priority_Update(_float fTimeDelta) override;
    virtual void		Update(_float fTimeDelta) override;
    virtual void		Late_Update(_float fTimeDelta) override;
    virtual void		Render(ID3D12GraphicsCommandList* _commandList) override;
    virtual void		ShadowRender(ID3D12GraphicsCommandList* _commandList) override;

private:
    virtual HRESULT				Ready_PartObjects();
    virtual HRESULT				Ready_Components();
    void                        Anim_Test();

private:
    class CModel* m_pModelCom = { nullptr };
    _uint				m_iState = 0;
    _int				m_iHealth = 0;
    _wstring			m_strModelTag = L"";
    _uint				m_iModelLevelIndex = 0;
    bool                m_isBlending = false;


public:
    static CPlayer_Fish* Create(EngineContext* pContext);
    virtual CGameObject* Clone(void* pArg);
    virtual void Free() override;


};