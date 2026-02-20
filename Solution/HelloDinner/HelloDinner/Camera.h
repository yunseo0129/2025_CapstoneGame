#pragma once

#include "GameObject.h"

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
	HRESULT Bind_CameraBuffer ( ID3D12GraphicsCommandList* pCmdList , RootParameterIndex _eIndex );

	static void DebugPrintMatrix ( const char* name , const XMFLOAT4X4& m );

	static void DebugPrintFloat3 ( const char* name , const XMFLOAT3& v );

protected:
	_float			m_fFovy = { 0.f };
	_float			m_fAspect = { 0.f };
	_float			m_fNear = { 0.f };
	_float			m_fFar = { 0.f };
	_float			m_fRotR = { 0.f };
	_float			m_fRotY = { 0.f };

protected:
	// Pipeline에 카메라 관련 행렬들을 계산해서 넘겨주는 함수
	// void Compute_PipeLineMatrices();
	// (변경) 카메라가 직접 계산해서 자신이 가지고 있음
	// Camera 정보는 Render 시작할 때 RootSignature에 한번 바인딩 하면됨
	// GameObject로 관리하면 Render 할때마다 찾고 RenderQueue에서 카메라를 찾는 비용이 드니
	// Level에서 카메라를 관리하는 방식으로 변경

private:
	// 카메라 Constant Buffer 생성
	HRESULT Create_CameraBuffer();


private:
	ComPtr<ID3D12Device> m_pDevice;
	ComPtr<ID3D12Resource> m_pCameraBuffer;
	CB_VS_CAMERA* m_pCbMappedCamera = { nullptr };

	XMFLOAT4X4						m_xmf4x4View;
	XMFLOAT4X4						m_xmf4x4Projection;
	XMFLOAT3						m_xmf3Position;

public:
	
	virtual CGameObject* Clone(void* pArg);
	// Clone이 필요한가?
	virtual void Free() override;
}; 