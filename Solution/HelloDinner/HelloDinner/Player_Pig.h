#pragma once
#include "ContainerObj.h"
#include "Model.h"

class CPlayer_Pig final : public CContainerObj
{
public:
	struct PLAYER_PIG_DESC : public CContainerObj::CONTAINEROBJ_DESC
	{
		_float3 			vPos = _float3(1.f, 1.f, 1.f);
		_uint				iModelLevelIndex = 0;
		_float3				vRotation = {};
		_wstring			strModelTag = L"";
	};
	enum PLAYER_1RD_COLLIDER_TYPE { COLLIDER_HEAD, COLLIDER_ARM_UP_L, COLLIDER_ARM_UP_R, COLLIDER_ARM_LOW_L, COLLIDER_ARM_LOW_R, COLLIDER_THIGH_L, COLLIDER_THIGH_R, COLLIDER_SHIN_L, COLLIDER_SHIN_R, COLLIDER_BODY, COLLIDER_END };

private:
	CPlayer_Pig(EngineContext* pContext);
	CPlayer_Pig(const CPlayer_Pig& Prototype);
	virtual ~CPlayer_Pig() = default;

public:
	virtual HRESULT		Initialize_Prototype() override;
	virtual HRESULT		Initialize(void* pArg) override;
	virtual void		Priority_Update(_float fTimeDelta) override;
	virtual void		Update(_float fTimeDelta) override;
	virtual void		Late_Update(_float fTimeDelta) override;
	virtual void		Render(ID3D12GraphicsCommandList* _commandList) override;
	virtual void		ShadowRender(ID3D12GraphicsCommandList* _commandList) override;
    virtual bool        Get_WorldBoundingSphere(_float3& outCenter, _float& outRadius) const override;
    virtual void        Die(_float _val);
    virtual void        Revive() override;

	//CCollider* Get_CollisionCom() const { return m_pColliderCom; }
	//virtual void TakeDamage(int iDamage) PURE;

private:
	virtual HRESULT				Ready_PartObjects();
	virtual HRESULT				Ready_Components();
    void                        Drive_Animation();
    void                        Anim_Test();

private:
	class CModel* m_pModelCom = { nullptr };
	vector<class CCollider*> m_vColliderComs;
	vector<class CCollider*> m_vMapColliderComs;
	_uint				m_iState = 0;
	_int				m_iHealth = 0;
	_wstring			m_strModelTag = L"";
	_uint				m_iModelLevelIndex = 0;
    bool                m_isBlending   = false;
    bool                m_bIsJumping   = false;
    bool                m_isReloading = false;
    bool                m_bShootPending = false;
    bool                m_bReloadPending = false;
    unsigned short      m_keyInput     = 0;
    unsigned short      m_prevKeyInput = 0;
    int                 m_iNetworkId   = -1;

    // Dead reckoning
    void                Update_DeadReckoning(float fTimeDelta);

    // 포물선 발사 — 탄도 적분 (Update에서 m_bLaunching 중 호출)
    void                Update_Launch(_float fTimeDelta);
    bool                m_bLaunching       = false;
    _float3             m_vLaunchVel       = _float3(0.f, 0.f, 0.f);
    float               m_fLaunchLandY     = 0.f;
    bool                m_bLaunchDescending = false;
    bool                m_bLaunchFalling   = false;
    XMFLOAT4X4          m_drCurrentMat = {};
    XMFLOAT4X4          m_drTargetMat  = {};
    XMFLOAT3            m_drVelocity   = {};
    float               m_drTimeSince  = 0.f;
    float               m_drInterval   = 0.05f;
    bool                m_drHasData    = false;

public:
    virtual int Get_NetworkId() const override { return m_iNetworkId; }
    void        Set_NetworkId(int id)           { m_iNetworkId = id; }

	void Apply_NetworkMatrix(const float* pMatrix, unsigned short keyInput = 0);
    // 재생성 시 현재 위치 보존용 — m_drTargetMat 스냅샷
    void Get_NetworkMatrix(float* outMatrix) const;

    // ---- 포물선 낙하(스폰) — 원격 플레이어 연출용 ----
    void Launch_To(const _float3& vTarget, _float fArcHeight = 3.f);
    bool Is_Launching() const { return m_bLaunching; }

    // 즉시 위치 이동 (포물선 발사 시작 전 싱크대 위치 세팅용)
    void Set_Position(const _float3& vPos);

public:
	static CPlayer_Pig* Create(EngineContext* pContext);
	virtual CGameObject* Clone(void* pArg);
	virtual void Free() override;


};