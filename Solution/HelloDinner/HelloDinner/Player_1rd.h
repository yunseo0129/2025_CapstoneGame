#pragma once
#include "ContainerObj.h"
#include "Model.h"

class CPlayer_1rd final : public CContainerObj
{
public:
	struct Player_1RD_DESC : public CContainerObj::CONTAINEROBJ_DESC
	{
		_float3 			vPos = _float3(1.f, 1.f, 1.f);
		_uint				iModelLevelIndex = 0;
		_float3				vRotation = {};
		_wstring			strModelTag = L"";
		class CCamera_FPV*	pCamera = nullptr;
	};
	enum PLAYER_1RD_COLLIDER_TYPE { COLLIDER_MAIN, COLLIDER_HEAD, COLLIDER_ARM_UP_L, COLLIDER_ARM_UP_R, COLLIDER_ARM_LOW_L, COLLIDER_ARM_LOW_R, COLLIDER_THIGH_L, COLLIDER_THIGH_R, COLLIDER_SHIN_L, COLLIDER_SHIN_R , COLLIDER_BODY, COLLIDER_END };
    enum PLAYER_1RD_ANIMATION_UPPER { UPPER_DEFAULT, UPPER_IDLE, UPPER_SHOOT, UPPER_RELOAD, UPPER_JUMP, UPPER_DIE, UPPER_END };
    enum PLAYER_1RD_ANIMATION_LOWER { LOWER_DEFAULT, LOWER_IDLE, LOWER_WALK, LOWER_RUN, LOWER_SIT, LOWER_SITWALK, LOWER_JUMP, LOWER_DIE, LOWER_END };
private:
	CPlayer_1rd(EngineContext* pContext);
	CPlayer_1rd(const CPlayer_1rd& Prototype);
	virtual ~CPlayer_1rd() = default;

public:
	virtual HRESULT		Initialize_Prototype() override;
	virtual HRESULT		Initialize(void* pArg) override;
	virtual void		Priority_Update(_float fTimeDelta) override;
	virtual void		Update(_float fTimeDelta) override;
	virtual void		Late_Update(_float fTimeDelta) override;
	virtual void		Render(ID3D12GraphicsCommandList* _commandList) override;
	virtual void		ShadowRender(ID3D12GraphicsCommandList* _commandList) override;
    virtual bool        Get_WorldBoundingSphere(_float3& outCenter, _float& outRadius) const override;
	//CCollider* Get_CollisionCom() const { return m_pColliderCom; }
	//virtual void TakeDamage(int iDamage) PURE;

public:					// 플레이어 조종에 관련한 함수 구현
	void Move(_float _fLook, _float _fRight, _float _val);
	void TurnYaw(_float _val);
	void TurnPitch(_float _val);
	void Jump(_float _val);
	void Crouch(_float _val);
    void Run(_float _val);
    void Reload(_float _val);
    void Shoot(_float _val);
    void Die(_float _val);
    _bool Get_Die() { return m_isDie; }


    // 무기 교체
    void Set_Weapon(_int iIndex);
    _int Get_Weapon() const { return m_iWeapon; } 

    // ---- 포물선 낙하(스폰) ----
    //  현재 위치에서 vTarget 까지 공을 던지듯 위로 솟구쳤다가 떨어진다.
    //   - 비행 중에는 충돌 처리 없음(요구사항). 이동 입력 무시(시점 회전은 가능).
    //   - 정점 높이 = max(현재y, 목표y) + fArcHeight 가 되도록 초기 수직속도 계산.
    //   - 하강하다가 y 가 fLandY(목표 높이) 이하로 내려오면 그 지점에서 멈춤.
    void Launch_To(const _float3& vTarget, _float fArcHeight = 3.f);
    _bool Is_Launching() const { return m_bLaunching; }

private:
	virtual HRESULT				Ready_PartObjects();
	virtual HRESULT				Ready_Components();
    void Resolve_Movement(_float fTimeDelta);
    void Anima();
    void Update_Launch(_float fTimeDelta); // 비행 중 탄도 적분(충돌 없음)

private:
	class CModel*		m_pModelCom = { nullptr };
	class CCamera_FPV*	m_pCamera = { nullptr };
	class CModel*		m_pFPSModelCom = { nullptr };
	_float4x4			m_matFPSModel;
	vector<class CCollider*> m_vColliderComs;
	vector<class CCollider*> m_vMapColliderComs  ;
	_uint				m_iState = 0;
	_int				m_iHealth = 0;
	_wstring			m_strModelTag = L"";
	_uint				m_iModelLevelIndex = 0;
	_float				m_fPitchRot = 0;

    // 중력
    _float              m_fMoveLook = 0.f;
    _float              m_fMoveRight = 0.f;
    _float              m_fVerticalVelocity = 0.f;
    _bool               m_bIsGrounded = false;


    _bool               m_isReloading = false;
    _uint               m_iAmmo = 30;
    _bool               m_isCrouch = false;
    _bool               m_isRun = false;
    _bool               m_isMove = false;
    _bool               m_isDie = false;

    // 현재 무기 인덱스 (0: 케첩건, 1: 마요건)
    _int                m_iWeapon = 0;

    // ---- 포물선 낙하(스폰) 상태 ----
    _bool               m_bLaunching = false;       // 비행 중?
    _float3             m_vLaunchVel = _float3(0.f, 0.f, 0.f); // 현재 속도(월드, units/s)
    _float              m_fLaunchLandY = 0.f;       // 이 높이 이하로 떨어지면 착지
    _bool               m_bLaunchDescending = false;// 정점을 지나 하강 시작했는지

public:
	static CPlayer_1rd* Create(EngineContext* pContext);
	virtual CGameObject* Clone(void* pArg);
	virtual void Free() override;


};