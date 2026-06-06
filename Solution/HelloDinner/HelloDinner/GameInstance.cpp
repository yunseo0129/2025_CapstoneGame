#include "GameInstance.h"

#include "Graphic_Device.h"
#include "Renderer.h"
#include "Input_Device.h"
#include "Timer_Manager.h"
#include "Level_Manager.h"
#include "Object_Manager.h"
#include "Prototype_Manager.h"
#include "Load_Manager.h"
#include "Shader_Manager.h"
#include "Texture_Manager.h"
#include "Camera.h"
#include "Controller.h"
#include "Collision_Manager.h"
//#include "Player_1rd.h"

IMPLEMENT_SINGLETON(CGameInstance)

CGameInstance::CGameInstance()
{

}

// ------------------------------------------------------------------------
// GameInstance
// ------------------------------------------------------------------------
HRESULT CGameInstance::Initialize_Engine(const ENGINE_DESC& EngineDesc, EngineContext* _pcontext)
{
	m_pGraphic_Device = CGraphic_Device::Create(EngineDesc.hWnd, _pcontext);
	if (nullptr == m_pGraphic_Device)
		return E_FAIL;

	m_pInput_Device = CInput_Device::Create(EngineDesc.hInstance, EngineDesc.hWnd);
	if (nullptr == m_pInput_Device)
		return E_FAIL;

	m_pController = CController::Create();
	if (nullptr == m_pController)
		return E_FAIL;

	m_pTexture_Manager = CTexture_Manager::Create(_pcontext);
	if (nullptr == m_pTexture_Manager)
		return E_FAIL;

	m_pTimer_Manager = CTimer_Manager::Create();
	if (nullptr == m_pTimer_Manager)
		return E_FAIL;

	m_pLevel_Manager = CLevel_Manager::Create();
	if (nullptr == m_pLevel_Manager)
		return E_FAIL;

	m_pPrototype_Manager = CPrototype_Manager::Create(EngineDesc.iNumLevels);
	if (nullptr == m_pPrototype_Manager)
		return E_FAIL;

	m_pObject_Manager = CObject_Manager::Create(EngineDesc.iNumLevels);
	if (nullptr == m_pObject_Manager)
		return E_FAIL;

	m_pCollision_Manager = CCollision_Manager::Create();
	if (nullptr == m_pCollision_Manager)
		return E_FAIL;

	m_pShader_Manager = CShader_Manager::Create(_pcontext->device);

	m_pRenderer = CRenderer::Create(_pcontext->device, _pcontext->cmdList);
	if (nullptr == m_pRenderer)
		return E_FAIL;

	m_pLoad_Manager = CLoad_Manager::Create();
	if (nullptr == m_pLoad_Manager)
		return E_FAIL;

	// 테스트용
	// Camera.h 변경해야할 것들
	// 1. abstract 설정
	// 2. Clone = 0;
	// 3. 생성자, 소멸자 protected로 변경

	return S_OK;
}

void CGameInstance::Update_Engine(_float fTimeDelta)
{
	m_pInput_Device->Update_InputDev();
	m_pController->Update_Controller(fTimeDelta);
	m_pObject_Manager->Priority_Update(fTimeDelta);
	m_pCollision_Manager->Update_Collision();
	m_pObject_Manager->Update(fTimeDelta);
	m_pCollision_Manager->Clear_CollisionGroup();

	m_pLevel_Manager->Update(fTimeDelta);
    m_pLevel_Manager->Update_Shadows(fTimeDelta);
    m_pLevel_Manager->Reset_CullStats();

    //BVH
    const BoundingFrustum* pFrustum = m_pLevel_Manager->Get_CurrentFrustum();
    const BoundingSphere* pShadow = m_pLevel_Manager->Get_ShadowBounds();
    m_pCollision_Manager->Cull_StaticBVH(pFrustum, pShadow);

    m_pObject_Manager->Late_Update(fTimeDelta);
}

HRESULT CGameInstance::Render_Begin(const _float4& vClearColor)
{
	m_pGraphic_Device->BeforeRender(vClearColor);
	m_pCommandList = m_pGraphic_Device->GetCommandList ();
	Set_RootSignature(m_pCommandList.Get());
	m_pTexture_Manager->Bind_GlobalHeap(m_pCommandList.Get());
	return S_OK;
}

HRESULT CGameInstance::Draw()
{
	m_pLevel_Manager->Set_CurrentCamera(CAMERA_FPV);

    m_pLevel_Manager->Begin_ShadowPass(m_pCommandList.Get());
    m_pRenderer->Draw_ShadowQueue(m_pCommandList.Get());
    m_pLevel_Manager->End_ShadowPass(m_pCommandList.Get());

	m_pGraphic_Device->initRenderTargetAndDepthStencil(m_pCommandList.Get());
	m_pLevel_Manager->Bind_CameraBuffer(m_pCommandList.Get(), RootParameterIndex::Camera, CAMERA_FPV);
	m_pLevel_Manager->Bind_LightBuffer(m_pCommandList.Get(), RootParameterIndex::Light);
	m_pRenderer->Draw_RenderObject ( m_pCommandList.Get () );
	return S_OK;
}


HRESULT CGameInstance::Render_End()
{
	m_pGraphic_Device->AfterRender();

	return S_OK;
}

void CGameInstance::Clear(_int iLevelID)
{
	/* 특정 레벨용 객체들을 지운다. */
	m_pObject_Manager->Clear(iLevelID);

	m_pPrototype_Manager->Clear(iLevelID);
}

// ------------------------------------------------------------------------
// Input_Device
// ------------------------------------------------------------------------

_byte CGameInstance::Get_DIKeyState(_ubyte byKeyID)
{
	return m_pInput_Device->Get_DIKeyState(byKeyID);
}

_byte CGameInstance::Get_DIMouseState(Engine::MOUSEKEYSTATE eMouse)
{
	return m_pInput_Device->Get_DIMouseState(eMouse);
}

_long CGameInstance::Get_DIMouseMove(Engine::MOUSEMOVESTATE eMouseState)
{
	return m_pInput_Device->Get_DIMouseMove(eMouseState);
}

bool CGameInstance::Key_Pressing(int _iKey)
{
	return m_pInput_Device->Key_Pressing(_iKey);
}

bool CGameInstance::Key_Down(int _iKey)
{
	return m_pInput_Device->Key_Down(_iKey);
}

bool CGameInstance::Key_Up(int _iKey)
{
	return m_pInput_Device->Key_Up(_iKey);
}

bool CGameInstance::Mouse_Pressing(int _iKey)
{
	return m_pInput_Device->Mouse_Pressing(_iKey);
}

bool CGameInstance::Mouse_Down(int _iKey)
{
	return m_pInput_Device->Mouse_Down(_iKey);
}

bool CGameInstance::Mouse_Up(int _iKey)
{
	return m_pInput_Device->Mouse_Up(_iKey);
}

// ------------------------------------------------------------------------
// CController
// ------------------------------------------------------------------------

CController* CGameInstance::Get_Controller()
{
	return m_pController;
}

void CGameInstance::Set_MouseSensitive(_float _val)
{
	m_pController->Set_MouseSensitive(_val);
}

// ------------------------------------------------------------------------
// Graphic_Device_Manager
// ------------------------------------------------------------------------

_int CGameInstance::GetCurrentFrameIndex () const
{
	if ( nullptr == m_pGraphic_Device )
		return 0;

	return m_pGraphic_Device->GetCurrentFrameIndex ();
}

void CGameInstance::ResetCmdList ()
{
	if ( nullptr == m_pGraphic_Device )
		return;

	m_pGraphic_Device->ResetCmdList ();
}

void CGameInstance::CloseCmdList ()
{
	if ( nullptr == m_pGraphic_Device )
		return;

	m_pGraphic_Device->CloseCmdList ();
}

// ------------------------------------------------------------------------
// Timer_Manager
// ------------------------------------------------------------------------

_float CGameInstance::Get_TimeDelta(const _wstring& strTimerTag)
{
	if (nullptr == m_pTimer_Manager)
		return 0.f;

	return m_pTimer_Manager->Get_TimeDelta(strTimerTag);
}

HRESULT CGameInstance::Add_Timer(const _wstring& strTimerTag)
{
	if (nullptr == m_pTimer_Manager)
		return E_FAIL;

	return m_pTimer_Manager->Add_Timer(strTimerTag);
}

void CGameInstance::Update_TimeDelta(const _wstring& strTimerTag)
{
	if (nullptr == m_pTimer_Manager)
		return;

	return m_pTimer_Manager->Update_TimeDelta(strTimerTag);
}


// ------------------------------------------------------------------------
// Load_Device
// ------------------------------------------------------------------------

HRESULT CGameInstance::Open_File(const wchar_t* filename)
{
	return m_pLoad_Manager->Open_File(filename);
}

void CGameInstance::Close_File()
{
	m_pLoad_Manager->Close_File();
}

void CGameInstance::Read_File(_char& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(size_t& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_int& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_uint& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_float& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_float2& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_float3& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_float4& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(XMUINT4& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_float4x4& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(VTXMESH& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(VTXANIMMESH& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(KEYFRAME& read)
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(char(&read)[MAX_PATH])
{
	m_pLoad_Manager->Read_File(read);
}

void CGameInstance::Read_File(_tchar(&read)[MAX_PATH])
{
	m_pLoad_Manager->Read_File(read);
}

// ------------------------------------------------------------------------
// Level_Manager
// ------------------------------------------------------------------------

HRESULT CGameInstance::Open_Level(_int iLevelIndex, CLevel* pNewLevel)
{
	if (nullptr == m_pLevel_Manager)
		return E_FAIL;

	return m_pLevel_Manager->Open_Level(iLevelIndex, pNewLevel);
}

_int CGameInstance::Get_CurrentLevelID()
{
	return m_pLevel_Manager->Get_CurrentLevelID();
}

XMFLOAT4X4 CGameInstance::Get_CurrentCameraView()
{
	if (nullptr == m_pLevel_Manager)
		return XMFLOAT4X4();
	return m_pLevel_Manager->Get_CurrentCameraView();
}

XMFLOAT4X4 CGameInstance::Get_CurrentCameraProjection()
{
	if (nullptr == m_pLevel_Manager)
		return XMFLOAT4X4();
	return m_pLevel_Manager->Get_CurrentCameraProjection();
}

void CGameInstance::Add_CullStat_Main_Bulk(_int r, _int t)
{
    if (m_pLevel_Manager) m_pLevel_Manager->Add_CullStat_Main_Bulk(r, t);
}
void CGameInstance::Add_CullStat_Shadow_Bulk(_int r, _int t)
{
    if (m_pLevel_Manager) m_pLevel_Manager->Add_CullStat_Shadow_Bulk(r, t);
}

// ------------------------------------------------------------------------
// Frustum Culling (위임만)
// ------------------------------------------------------------------------
_bool CGameInstance::IsSphereInFrustum(const _float3& vCenter, _float fRadius)
{
    if (nullptr == m_pLevel_Manager) return true;
    return m_pLevel_Manager->IsSphereInFrustum(vCenter, fRadius);
}

_bool CGameInstance::IsSphereInShadowFrustum(const _float3& c, _float r)
{
    return m_pLevel_Manager ? m_pLevel_Manager->IsSphereInShadowFrustum(c, r) : true;
}

void CGameInstance::Set_CullingEnabled(_bool b)
{
    if (nullptr == m_pLevel_Manager) return;
    m_pLevel_Manager->Set_CullingEnabled(b);
}

_bool CGameInstance::Is_CullingEnabled() const
{
    if (nullptr == m_pLevel_Manager) return false;
    return m_pLevel_Manager->Is_CullingEnabled();
}

void  CGameInstance::Set_ShadowCullingEnabled(_bool b)
{
    if (m_pLevel_Manager) m_pLevel_Manager->Set_ShadowCullingEnabled(b);
}

_bool CGameInstance::Is_ShadowCullingEnabled() const
{
    return m_pLevel_Manager ? m_pLevel_Manager->Is_ShadowCullingEnabled() : false;
}

void CGameInstance::Reset_CullStats()
{
    if (nullptr == m_pLevel_Manager) return;
    m_pLevel_Manager->Reset_CullStats();
}

void  CGameInstance::Add_CullStat_Main(_bool b)
{
    if (m_pLevel_Manager) m_pLevel_Manager->Add_CullStat_Main(b);
}

void  CGameInstance::Add_CullStat_Shadow(_bool b)
{
    if (m_pLevel_Manager) m_pLevel_Manager->Add_CullStat_Shadow(b);
}

_uint CGameInstance::Get_CullStat_MainTotal()      const { return m_pLevel_Manager ? m_pLevel_Manager->Get_CullStat_MainTotal() : 0; }
_uint CGameInstance::Get_CullStat_MainRendered()   const { return m_pLevel_Manager ? m_pLevel_Manager->Get_CullStat_MainRendered() : 0; }
_uint CGameInstance::Get_CullStat_ShadowTotal()    const { return m_pLevel_Manager ? m_pLevel_Manager->Get_CullStat_ShadowTotal() : 0; }
_uint CGameInstance::Get_CullStat_ShadowRendered() const { return m_pLevel_Manager ? m_pLevel_Manager->Get_CullStat_ShadowRendered() : 0; }

// ------------------------------------------------------------------------
// Prototype_Manager
// ------------------------------------------------------------------------

HRESULT CGameInstance::Add_Prototype(_uint iLevelIndex, const _wstring& strPrototypeTag, CBase* pPrototype)
{
	if (nullptr == m_pPrototype_Manager)
		return E_FAIL;

	return m_pPrototype_Manager->Add_Prototype(iLevelIndex, strPrototypeTag, pPrototype);
}

CBase* CGameInstance::Clone_Prototype(Engine::PROTOTYPE eType, _uint iLevelIndex, const _wstring& strPrototypeTag, void* pArg)
{
	if (nullptr == m_pPrototype_Manager)
		return nullptr;

	return m_pPrototype_Manager->Clone_Prototype(eType, iLevelIndex, strPrototypeTag, pArg);
}

void CGameInstance::ReleaseUploadBuffers ( _uint iLevelIndex )
{
	if ( nullptr == m_pPrototype_Manager )
		return;

	m_pPrototype_Manager->ReleaseUploadBuffers ( iLevelIndex );
}

// ------------------------------------------------------------------------
// Object_Manager
// ------------------------------------------------------------------------

HRESULT CGameInstance::Add_GameObject_ToLayer(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag, _uint iLevelIndex, const _wstring& strLayerTag, void* pArg)
{
	if (nullptr == m_pObject_Manager)
		return E_FAIL;

	return m_pObject_Manager->Add_GameObject_ToLayer(iPrototypeLevelIndex, strPrototypeTag, iLevelIndex, strLayerTag, pArg);
}

CGameObject* CGameInstance::Add_GameObject_ToLayer_Return_Obj(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag, _uint iLevelIndex, const _wstring& strLayerTag, void* pArg)
{
	if (nullptr == m_pObject_Manager)
		return nullptr;

	return m_pObject_Manager->Add_GameObject_ToLayer_Return_Obj(iPrototypeLevelIndex, strPrototypeTag, iLevelIndex, strLayerTag, pArg);
}

list<class CGameObject*> CGameInstance::Get_List(_uint iLevelIndex, const _wstring& strLayerTag)
{
	return m_pObject_Manager->Get_List(iLevelIndex, strLayerTag);
}

CGameObject* CGameInstance::Get_GameObject_To_Layer(_uint iLevelIndex, const _wstring& strLayerTag, _uint Index)
{
	return m_pObject_Manager->Get_GameObject_To_Layer(iLevelIndex, strLayerTag, Index);
}


// ------------------------------------------------------------------------
// Shader_Manager
// ------------------------------------------------------------------------

void CGameInstance::Set_PipelineState(ID3D12GraphicsCommandList* pCmdList, const PSO_TYPE& _eType)
{
	if (nullptr == m_pShader_Manager)
		return;
	m_pShader_Manager->Set_PipelineState(pCmdList, _eType);
}

void CGameInstance::Set_RootSignature(ID3D12GraphicsCommandList* pCmdList)
{
	if (nullptr == m_pShader_Manager) {
		MSG_BOX("CGameInstance::Set_RootSignature() : Shader_Manager is nullptr");
		return;
	}
	m_pShader_Manager->Set_RootSignature(pCmdList);
}

// ------------------------------------------------------------------------
// Texture_Manager
// ------------------------------------------------------------------------

CTexture* CGameInstance::Get_Texture(_uint _eType)
{
	if (nullptr == m_pTexture_Manager) {
		MSG_BOX("CGameInstance::Get_Texture() : DefaultTexture_Manager is nullptr");
		return nullptr;
	}
	return	m_pTexture_Manager->Get_DefaultNormalTexture();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CGameInstance::Get_GPUHandle(_uint _iIndex)
{
	if (nullptr == m_pTexture_Manager) {
		MSG_BOX("CGameInstance::Get_GPUHandle() : DefaultTexture_Manager is nullptr");
		return CD3DX12_GPU_DESCRIPTOR_HANDLE();
	}
	return m_pTexture_Manager->Get_GPUHandle(_iIndex);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CGameInstance::Get_CPUHandle()
{
	if (nullptr == m_pTexture_Manager) {
		MSG_BOX("CGameInstance::Get_CPUHandle() : DefaultTexture_Manager is nullptr");
		return CD3DX12_CPU_DESCRIPTOR_HANDLE();
	}
	return m_pTexture_Manager->Get_CPUHandle();
}

_uint CGameInstance::Get_CurrentIndex() const
{
	if (nullptr == m_pTexture_Manager) {
		MSG_BOX("CGameInstance::Get_CurrentIndex() : DefaultTexture_Manager is nullptr");
		return 0;
	}
	return m_pTexture_Manager->Get_CurrentIndex();
}

void CGameInstance::Offset_DescriptorHandle(_uint _iOffset)
{
	if (nullptr == m_pTexture_Manager) {
		MSG_BOX("CGameInstance::Offset_DescriptorHandle() : DefaultTexture_Manager is nullptr");
		return;
	}
	m_pTexture_Manager->Offset_DescriptorHandle(_iOffset);
}

// ------------------------------------------------------------------------
// Collision_Manager
// ------------------------------------------------------------------------
void CGameInstance::Update_Collision() {
	m_pCollision_Manager->Update_Collision();
}

void CGameInstance::Clear_CollisionGroup() {
	m_pCollision_Manager->Clear_CollisionGroup();
}

void CGameInstance::Set_CollisionMatrix(int _lgroup, int _rgroup, _bool _is) {
	m_pCollision_Manager->Set_CollisionMatrix(CCollision_Manager::COLLISION_GROUP(_lgroup), CCollision_Manager::COLLISION_GROUP(_rgroup), _is);
}

void CGameInstance::Add_CollisionGroup(int _eGroup, class CCollider* _pCollider) {
	m_pCollision_Manager->Add_CollisionGroup(CCollision_Manager::COLLISION_GROUP(_eGroup), _pCollider);
}

void CGameInstance::Delete_CollisionGroup(int _eGroup, class CCollider* _pCollider) {
	m_pCollision_Manager->Delete_CollisionGroup(CCollision_Manager::COLLISION_GROUP(_eGroup), _pCollider);
}

vector<class CCollider*> CGameInstance::CollisionCheck_with_Group(class CCollider* _pCollider, int _eGroup) {
	return m_pCollision_Manager->CollisionCheck_with_Group(_pCollider, CCollision_Manager::COLLISION_GROUP(_eGroup));
}

bool CGameInstance::CollisionCheck_with_Collider(class CCollider* _pMyCollider, class CCollider* _pOtherCollider) {
	return m_pCollision_Manager->CollisionCheck_with_Collider(_pMyCollider, _pOtherCollider);
}

bool CGameInstance::CheckMove(CCollider* me, const XMFLOAT3& move, XMFLOAT3& outSlide) {
	return m_pCollision_Manager->CheckMove(me, move, outSlide);
}

bool CGameInstance::CheckMove(CCollider* me, const XMFLOAT3& move, XMFLOAT3& outSlide, vector<CCollider*>* outHits) {
    return m_pCollision_Manager->CheckMove(me, move, outSlide, outHits);
}

//BVH
void  CGameInstance::Build_StaticBVH() { if (m_pCollision_Manager) m_pCollision_Manager->Build_StaticBVH(); }
void  CGameInstance::Invalidate_StaticBVH() { if (m_pCollision_Manager) m_pCollision_Manager->Invalidate_StaticBVH(); }
void  CGameInstance::Set_BVHEnabled(_bool b) { if (m_pCollision_Manager) m_pCollision_Manager->Set_BVHEnabled(b); }
_bool CGameInstance::Is_BVHEnabled()    const { return m_pCollision_Manager ? m_pCollision_Manager->Is_BVHEnabled() : false; }
_int  CGameInstance::Get_BVHNodeCount() const { return m_pCollision_Manager ? m_pCollision_Manager->Get_BVHNodeCount() : 0; }
_int  CGameInstance::Get_BVHPrimitiveCount() const { return m_pCollision_Manager ? m_pCollision_Manager->Get_BVHPrimitiveCount() : 0; }
_int  CGameInstance::Get_BVHMaxDepth()  const { return m_pCollision_Manager ? m_pCollision_Manager->Get_BVHMaxDepth() : 0; }
_int  CGameInstance::Get_BVHLastQueryCandidates() const { return m_pCollision_Manager ? m_pCollision_Manager->Get_LastQueryCandidates() : 0; }
void CGameInstance::Cull_StaticBVH(const BoundingFrustum* p, const BoundingSphere* s)
{
    if (m_pCollision_Manager) m_pCollision_Manager->Cull_StaticBVH(p, s);
}

void  CGameInstance::Set_CullingBVHEnabled(_bool b)
{
    if (m_pCollision_Manager) m_pCollision_Manager->Set_CullingBVHEnabled(b);
}

_bool CGameInstance::Is_CullingBVHEnabled() const
{
    return m_pCollision_Manager ? m_pCollision_Manager->Is_CullingBVHEnabled() : false;
}

_int  CGameInstance::Get_LastFrustumCandidates() const
{
    return m_pCollision_Manager ? m_pCollision_Manager->Get_LastFrustumCandidates() : 0;
}

_int  CGameInstance::Get_LastShadowCandidates() const
{
    return m_pCollision_Manager ? m_pCollision_Manager->Get_LastShadowCandidates() : 0;
}

// ------------------------------------------------------------------------
// Renderer
// ------------------------------------------------------------------------
HRESULT CGameInstance::Add_RenderObject(CRenderer::RENDERGROUP eRenderGroup, CGameObject* pRenderObject)
{
	if (nullptr == m_pRenderer)
		return E_FAIL;

	return m_pRenderer->Add_RenderObject(eRenderGroup, pRenderObject);
}

HRESULT CGameInstance::Add_ShadowRenderObject(CRenderer::RENDERGROUP g, CGameObject* p)
{
    return m_pRenderer ? m_pRenderer->Add_ShadowRenderObject(g, p) : E_FAIL;
}

HRESULT CGameInstance::Add_InstancedRenderObject(const _wstring& tag, CGameObject* p)
{
    return m_pRenderer ? m_pRenderer->Add_InstancedRenderObject(tag, p) : E_FAIL;
}

HRESULT CGameInstance::Add_ShadowInstancedRenderObject(const _wstring& tag, CGameObject* p)
{
    return m_pRenderer ? m_pRenderer->Add_ShadowInstancedRenderObject(tag, p) : E_FAIL;
}

void CGameInstance::Set_InstancingEnabled(bool b)
{ 
    if (m_pRenderer) m_pRenderer->Set_InstancingEnabled(b);
}

bool CGameInstance::Is_InstancingEnabled() const
{
    return m_pRenderer ? m_pRenderer->Is_InstancingEnabled() : false;
}

_int CGameInstance::Get_DrawCallCount() const
{
    return m_pRenderer ? m_pRenderer->Get_DrawCallCount() : 0;
}

_int CGameInstance::Get_InstancedGroupCount() const
{
    return m_pRenderer ? m_pRenderer->Get_InstancedGroupCount() : 0;
}

#ifdef _DEBUG
HRESULT CGameInstance::Add_RenderCollider(CCollider* pColliderCom)
{
	if (nullptr == m_pRenderer)
		return E_FAIL;
	return m_pRenderer->Add_RenderCollider(pColliderCom);
}
#endif

void CGameInstance::Release_Engine()
{
	CGameInstance::GetInstance()->Free();

	CGameInstance::GetInstance()->DestroyInstance();
}

void CGameInstance::Free()
{

	if ( m_pGraphic_Device )
		m_pGraphic_Device->WaitForGpuComplete ();

	// 2. ComPtr로 보유한 커맨드 리스트 참조 해제
	m_pCommandList.Reset ();

	// 3. 렌더러 해제 (Device, CmdList를 Safe_AddRef로 보유 중)
	Safe_Release ( m_pRenderer );

	// 4. 게임 오브젝트 해제 (컴포넌트 → 텍스처/버퍼의 ComPtr 해제)
	Safe_Release( m_pCollision_Manager );
	Safe_Release ( m_pObject_Manager );
	Safe_Release( m_pController );

	// 5. 레벨 해제
	Safe_Release ( m_pLevel_Manager );

	// 6. 프로토타입 해제 (텍스처/버퍼 원본 리소스 해제)
	Safe_Release ( m_pPrototype_Manager );

	// 8. 셰이더 매니저 해제 (PSO, RootSignature 해제)
	Safe_Release ( m_pShader_Manager );
	Safe_Release(m_pTexture_Manager);

	// 9. 나머지 매니저 해제
	Safe_Release ( m_pLoad_Manager );
	Safe_Release ( m_pTimer_Manager );
	Safe_Release ( m_pInput_Device );

	// 10. Device를 가장 마지막에 해제
	Safe_Release ( m_pGraphic_Device );

	__super::Free();
}