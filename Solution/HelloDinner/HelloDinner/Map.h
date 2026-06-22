#pragma once
#include "GameObject.h"
#include "Collider.h"

class CMap: public CGameObject
{
public:
    struct MAP_DESC: public CGameObject::GAMEOBJECT_DESC
    {
        _float3		vPosition = {};
        _float3		vRotation = {};		// degree 단위
        _float3		vScale = {1.f, 1.f, 1.f};
        _wstring	strModelTag = L"";
        _uint		iModelLevelIndex = 0;

        // collider 정보까지 넣어서 관리하자
        CCollider::COLLIDERTYPE eColliderType = CCollider::TYPE_END;
        _float3    vCenterCollider = {};
        _float3    vExtentsCollider = {};
        _float3    vRotationCollider = {};
        _float	   fRadius = 0.f;

        _bool bBreakable = false;   // 부서지는 맵인지 확인
        _uint iBreakPreset = 0;
        _uint iWallId = 0;          // [Fracture] 안정적 식별자(Loader 가 인스턴스 순번 부여)
    };

private:
    CMap(EngineContext* _pcontext);
    CMap(const CMap& Prototype);
    virtual ~CMap() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* pArg) override;
    virtual void Priority_Update(_float fTimeDelta) override;
    virtual void Update(_float fTimeDelta) override;
    virtual void Late_Update(_float fTimeDelta) override;
    virtual void Render(ID3D12GraphicsCommandList* _commandList) override;
    virtual void ShadowRender(ID3D12GraphicsCommandList* _commandList) override;
    virtual void Render_Blend(ID3D12GraphicsCommandList* _commandList) override;  // [투명] 유리 메시만

public:
    // Frustum Culling
    virtual bool Get_WorldBoundingSphere(_float3& pOutCenter, _float& pOutRadius) const override;

    // Instance
    const _wstring& Get_ModelTag() const { return m_strModelTag; }
    class CModel* Get_Model() const { return m_pModelCom; }
    const _float4x4& Get_CachedWorldMatrix() const { return m_xmf4x4CachedWorld; }

    // 부서지는 벽
    // 부서지는 벽인지 확인
    _bool Is_Breakable() const { return m_bBreakable; }
    _bool Is_Broken()    const { return m_bBroken; }
    void  Break();
    void  Restore();
    // 이웃 벽이 사라졌을 때 면 노출 구현
    void  Set_Neighbors(CMap* pLeft, CMap* pRight) { m_pLeftNeighbor = pLeft; m_pRightNeighbor = pRight; }
    void  Expose_Left();
    void  Expose_Right();
    _bool Is_LeftExposed()  const { return m_bLeftExposed; }
    _bool Is_RightExposed() const { return m_bRightExposed; }


private:
    HRESULT Ready_Components();

private:
    class CModel* m_pModelCom = {nullptr};
    class CCollider* m_pColliderCom = {nullptr};

    _wstring	m_strModelTag = L"";
    _uint		m_iModelLevelIndex = 0;
    _float4x4   m_xmf4x4CachedWorld;

    CCollider::COLLIDERTYPE m_eColliderType = CCollider::TYPE_END;
    _float3    m_vCenterCollider = {};
    _float3    m_vExtentsCollider = {};
    _float3    m_vRotationCollider = {};
    _float	   m_fRadius = 0.f;

    // 부서지는 벽 관련
    _bool m_bBreakable = false;
    _uint m_iBreakPreset = 0;
    _uint m_iWallId = 0;            // [Fracture] 안정적 식별자(MAP_DESC 에서 전달)
    _int  m_iFractureSlot = -1;    // Register 반환 슬롯(-1=미등록)
    _bool m_bBroken = false;
    CMap* m_pLeftNeighbor = nullptr;
    CMap* m_pRightNeighbor = nullptr;
    _bool m_bLeftExposed = false;
    _bool m_bRightExposed = false;

public:
    static CMap* Create(EngineContext* _pcontext);
    virtual CGameObject* Clone(void* pArg) override;
    virtual void Free() override;
};