#pragma once

#include "GameObject.h"
#include "Graphic_Device.h"

class CCamera  : public CGameObject
{
protected:
	struct CAMERA_DESC : public CGameObject::GAMEOBJECT_DESC
	{
		_float3			vEye = {};
		_float3			vAt = {};

		_float			fFovy = { 0.f };
		_float			fAspect = { 0.f };
		_float			fNear = { 0.f };
		_float			fFar = { 0.f };
	};

public:
	CCamera(EngineContext* pContext);
	CCamera(const CCamera& Prototype);
	virtual ~CCamera() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg);
	virtual void Priority_Update(_float fTimeDelta);
	virtual void Update(_float fTimeDelta);
	virtual void Late_Update(_float fTimeDelta);
	virtual HRESULT Render();

	_float			Get_RotR() const { return m_fRotR; }
	_float			Get_RotY() const { return m_fRotY; }
	void			Move(_vector vTarget) { m_pTransformCom->Set_State(CTransform::STATE_POSITION, vTarget); }

	// CBV 바인딩 함수
	HRESULT Bind_CameraBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex);
	XMFLOAT4X4 Get_CameraView() {
		return m_xmf4x4View;
	}
	XMFLOAT4X4 Get_CameraProjection() {
		return m_xmf4x4Projection;
	}

    // Frustum Culling
    void  Update_ViewProjection();
    _bool IsSphereInFrustum(const _float3& vCenter, _float fRadius) const;

    // DebugRendering을 위한 디버그 출력 함수
	static void DebugPrintMatrix ( const char* name , const XMFLOAT4X4& m );
	static void DebugPrintFloat3 ( const char* name , const XMFLOAT3& v );

protected:
	_float			m_fFovy = { 0.f };
	_float			m_fAspect = { 0.f };
	_float			m_fNear = { 0.f };
	_float			m_fFar = { 0.f };
	_float			m_fRotR = { 0.f };
	_float			m_fRotY = { 0.f };

private:
	// 카메라 Constant Buffer 생성
	HRESULT Create_CameraBuffer();


private:
	static const _int FRAME_COUNT = CGraphic_Device::SWAP_CHAIN_BUFFER_COUNT;
	ComPtr<ID3D12Resource> m_pCameraBuffers[FRAME_COUNT];
	CB_VS_CAMERA* m_pCbMappedCameras[FRAME_COUNT] = {};

	XMFLOAT4X4						m_xmf4x4View;
	XMFLOAT4X4						m_xmf4x4Projection;
	XMFLOAT3						m_xmf3Position;

    // Frustum Culling
    DirectX::BoundingFrustum    m_FrustumWorld;
    _bool                        m_bFrustumValid = false;

public:
	
	virtual CGameObject* Clone(void* pArg);
	// Clone이 필요한가?
	virtual void Free() override;
}; 