#pragma once
#include "ContainerObj.h"
#include "Model.h"

class CBullets final : public CGameObject
{
public:
    struct BULLET_DESC : public CGameObject::GAMEOBJECT_DESC
    {
    };
    struct BULLET
    {
        _vector vLook;
        _vector vPos;
        _vector vTarget;
        _float  fLifeTime;
        _bool   isHit = false;
        _bool   isOn = false;
    };
private:
    CBullets(EngineContext* pContext);
    CBullets(const CBullets& Prototype);
    virtual ~CBullets() = default;

public:
    virtual HRESULT		Initialize_Prototype() override;
    virtual HRESULT		Initialize(void* pArg) override;
    virtual void		Priority_Update(_float fTimeDelta) override;
    virtual void		Update(_float fTimeDelta) override;
    virtual void		Late_Update(_float fTimeDelta) override;
    virtual void		Render(ID3D12GraphicsCommandList* _commandList) override;

    void                NewBullet(BULLET _bullet);
    vector<BULLET>      Get_Bullets() { return m_vBullets; }

private:
    virtual HRESULT				Ready_Components();

private:
    vector<BULLET> m_vBullets;

public:
    static CBullets* Create(EngineContext* pContext);
    virtual CGameObject* Clone(void* pArg);
    virtual void Free() override;


};