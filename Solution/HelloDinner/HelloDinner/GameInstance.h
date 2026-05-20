#pragma once
#include "Prototype_Manager.h"
#include "Renderer.h"

class CGameInstance final : public CBase
{
	DECLARE_SINGLETON(CGameInstance)
private:
	CGameInstance();
	virtual ~CGameInstance() = default;

public: /* For.GameInstance */
	HRESULT Initialize_Engine(const ENGINE_DESC& EngineDesc, EngineContext* _pcontext);
	void Update_Engine(_float fTimeDelta);
	HRESULT Render_Begin(const _float4& vClearColor = _float4(0.f, 1.f, 0.f, 0.f));
	HRESULT Draw();
	HRESULT ShadowDrow();
	HRESULT Render_End();
	void Clear(_int iLevelID);

	_float Compute_Random_Normal();
	_float Compute_Random(_float fMin, _float fMax);

public: /* for.Graphics_Device*/
	void ResetCmdList ();
	void CloseCmdList ();
	_int GetCurrentFrameIndex () const;

public: /* for.Input_Device */
	_byte Get_DIKeyState(_ubyte byKeyID);
	_byte Get_DIMouseState(Engine::MOUSEKEYSTATE eMouse);
	_long Get_DIMouseMove(Engine::MOUSEMOVESTATE eMouseState);
	bool Key_Pressing(int _iKey);
	bool Key_Down(int _iKey);
	bool Key_Up(int _iKey);
	bool Mouse_Pressing(int _iKey);
	bool Mouse_Down(int _iKey);
	bool Mouse_Up(int _iKey);

public: /* for.Controller */
	class CController* Get_Controller();
	void Set_MouseSensitive(_float _val);

public: /* For.timer_Manager */
	_float Get_TimeDelta(const _wstring& strTimerTag);
	HRESULT	Add_Timer(const _wstring& strTimerTag);
	void Update_TimeDelta(const _wstring& strTimerTag);

public: /* For.Load_Manager */
	HRESULT Open_File(const wchar_t* filename);
	void	Close_File();

	void	Read_File(_char& read);
	void	Read_File(size_t& read);
	void	Read_File(_int& read);
	void	Read_File(_uint& read);
	void	Read_File(_float& read);
	void	Read_File(_float2& read);
	void	Read_File(_float3& read);
	void	Read_File(_float4& read);
	void	Read_File(XMUINT4& read);
	void	Read_File(_float4x4& read);
	void	Read_File(VTXMESH& read);
	void	Read_File(VTXANIMMESH& read);
	void	Read_File(KEYFRAME& read);
	void	Read_File(char(&read)[MAX_PATH]);
	void	Read_File(_tchar(&read)[MAX_PATH]);

public: /* for.Level_Manager */
	HRESULT Open_Level(_int iLevelIndex, class CLevel* pNewLevel);
	_int    Get_CurrentLevelID();
	XMFLOAT4X4 Get_CurrentCameraView ();
	XMFLOAT4X4 Get_CurrentCameraProjection ();
    void Add_CullStat_Main_Bulk(_int r, _int t);
    void Add_CullStat_Shadow_Bulk(_int r, _int t);

public: /* for.Frustum_Culling */
    _bool IsSphereInFrustum(const _float3& vCenter, _float fRadius);
    _bool IsSphereInShadowFrustum(const _float3& vCenter, _float fRadius);

    void  Set_CullingEnabled(_bool b);
    _bool Is_CullingEnabled() const;
    void  Set_ShadowCullingEnabled(_bool b);
    _bool Is_ShadowCullingEnabled() const;

    void  Reset_CullStats();
    void  Add_CullStat_Main(_bool b);
    void  Add_CullStat_Shadow(_bool b);
    _uint Get_CullStat_MainTotal() const;
    _uint Get_CullStat_MainRendered() const;
    _uint Get_CullStat_ShadowTotal() const;
    _uint Get_CullStat_ShadowRendered() const;

public: /* For.Prototype_Manager */
	HRESULT Add_Prototype(_uint iLevelIndex, const _wstring& strPrototypeTag, class CBase* pPrototype);
	class CBase* Clone_Prototype(Engine::PROTOTYPE eType, _uint iLevelIndex, const _wstring& strPrototypeTag, void* pArg = nullptr);
	void ReleaseUploadBuffers ( _uint iLevelIndex );


public: /* For.Object_Manager */
	HRESULT Add_GameObject_ToLayer(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag, _uint iLevelIndex, const _wstring& strLayerTag, void* pArg = nullptr);
	class CGameObject* Add_GameObject_ToLayer_Return_Obj(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag, _uint iLevelIndex, const _wstring& strLayerTag, void* pArg = nullptr);
	list<class CGameObject*> Get_List(_uint iLevelIndex, const _wstring& strLayerTag);
	class CGameObject* Get_GameObject_To_Layer(_uint iLevelIndex, const _wstring& strLayerTag, _uint Index);

public: /* For.Shader_Manager */
	void Set_PipelineState(ID3D12GraphicsCommandList* pCmdList, const PSO_TYPE& _eType);
	void Set_RootSignature(ID3D12GraphicsCommandList* pCmdList);

public: /* For.Texture_Manager */
	class CTexture* Get_Texture(_uint _eType);
	CD3DX12_GPU_DESCRIPTOR_HANDLE Get_GPUHandle(_uint _iIndex);
	CD3DX12_CPU_DESCRIPTOR_HANDLE Get_CPUHandle();
	_uint Get_CurrentIndex() const;
	void Offset_DescriptorHandle(_uint _iOffset);

public: /* For.Collision_Manager */
	void Update_Collision();
	void Clear_CollisionGroup();
	void Set_CollisionMatrix(int _lgroup, int _rgroup, _bool _is);
	void Add_CollisionGroup(int _eGroup, class CCollider* _pCollider);
	void Delete_CollisionGroup(int _eGroup, class CCollider* _pCollider);
	vector<class CCollider*> CollisionCheck_with_Group(class CCollider* _pCollider, int _eGroup);
	bool CollisionCheck_with_Collider(class CCollider* _pMyCollider, class CCollider* _pOtherCollider);
	bool CheckMove(CCollider* me, const XMFLOAT3& move, XMFLOAT3& outSlide);

    //BVH
    void  Build_StaticBVH();
    void  Invalidate_StaticBVH();
    void  Set_BVHEnabled(_bool b);
    _bool Is_BVHEnabled() const;
    _int  Get_BVHNodeCount() const;
    _int  Get_BVHPrimitiveCount() const;
    _int  Get_BVHMaxDepth() const;
    _int  Get_BVHLastQueryCandidates() const;
    void  Cull_StaticBVH(const DirectX::BoundingFrustum* p, const DirectX::BoundingSphere* s);
    void  Set_CullingBVHEnabled(_bool b);
    _bool Is_CullingBVHEnabled() const;
    _int  Get_LastFrustumCandidates() const;
    _int  Get_LastShadowCandidates() const;

public: /* For.Renderer */
    HRESULT Add_RenderObject(CRenderer::RENDERGROUP eRenderGroup, class CGameObject* pRenderObject);
    HRESULT Add_ShadowRenderObject(CRenderer::RENDERGROUP eRenderGroup, class CGameObject* pRenderObject);

#ifdef _DEBUG
	HRESULT Add_RenderCollider(class CCollider* pColliderCom);
#endif

private:
	class CGraphic_Device*		m_pGraphic_Device = { nullptr };
	class CInput_Device*		m_pInput_Device = { nullptr };
	class CTimer_Manager*		m_pTimer_Manager = { nullptr };
	class CLevel_Manager*		m_pLevel_Manager = { nullptr };
	class CPrototype_Manager*	m_pPrototype_Manager = { nullptr };
	class CObject_Manager*		m_pObject_Manager = { nullptr };
	class CLight_Manager*		m_pLight_Manager = { nullptr };
	class CPipeLine*			m_pPipeLine = { nullptr };
	class CShader_Manager*		m_pShader_Manager = { nullptr };
	class CRenderer*			m_pRenderer = { nullptr };
	class CLoad_Manager*		m_pLoad_Manager = { nullptr };
	class CTexture_Manager*		m_pTexture_Manager = { nullptr };
	class CController*			m_pController = { nullptr };
	class CCollision_Manager* m_pCollision_Manager = { nullptr };

	ComPtr<ID3D12GraphicsCommandList> m_pCommandList = { nullptr };

public:
	static void Release_Engine();

public:
	virtual void Free() override;
};