#include "Collider.h"

#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"
#include "GameInstance.h"

#include <CommonStates.h>

CCollider::CCollider(EngineContext* pContext)
	: CComponent{ pContext }
{
}

CCollider::CCollider(const CCollider& Prototype)
	: CComponent{ Prototype.m_pContext }
	, m_eType{ Prototype.m_eType }
#ifdef _DEBUG
	, m_pEffect{ Prototype.m_pEffect }
	, m_pBatch{ Prototype.m_pBatch }
#endif // _DEBUG
{
}

HRESULT CCollider::Initialize_Prototype(COLLIDERTYPE eColliderType)
{
	m_eType = eColliderType;


#ifdef _DEBUG
	// 1. RenderTargetState 설정 
	DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	RenderTargetState rtState(renderTargetFormat, depthStencilFormat);

	// 2. PSO 설정
	EffectPipelineStateDescription pd(
		&VertexPositionColor::InputLayout,
		CommonStates::Opaque,
		CommonStates::DepthDefault,
		CommonStates::CullNone,
		rtState,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE
	);

	m_pEffect = new DirectX::BasicEffect(m_pContext->device, DirectX::EffectFlags::VertexColor, pd);
	m_pBatch = new DirectX::PrimitiveBatch<DirectX::VertexPositionColor>(m_pContext->device);
#endif
	return S_OK;
}

HRESULT CCollider::Initialize(void* pArg)
{
	CBounding::BOUND_DESC* pDesc = static_cast<CBounding::BOUND_DESC*>(pArg);

	switch (m_eType)
	{
	case TYPE_AABB:
		m_pBounding = CBounding_AABB::Create(m_pContext, pDesc);
		break;
	case TYPE_OBB:
		m_pBounding = CBounding_OBB::Create(m_pContext, pDesc);
		break;
	case TYPE_SPHERE:
		m_pBounding = CBounding_Sphere::Create(m_pContext, pDesc);
		break;
	}

	if (pDesc->pSocketMatrix != nullptr)
		m_pSocketMatrix = pDesc->pSocketMatrix;
	if (pDesc->pParentMatrix != nullptr)
		m_pParentMatrix = pDesc->pParentMatrix;

	m_isCloned = true;


	return S_OK;
}

void CCollider::Update(_fmatrix WorldMatrix)
{
	if (nullptr == m_pBounding)
		return;

	m_pBounding->Update(WorldMatrix);
}

void CCollider::Update()
{
	if (nullptr == m_pBounding)
		return;
	if (m_pSocketMatrix != nullptr && m_pParentMatrix != nullptr)
		m_pBounding->Update(XMLoadFloat4x4(m_pSocketMatrix) * XMLoadFloat4x4(m_pParentMatrix));
	else if (m_pParentMatrix != nullptr)
		m_pBounding->Update(XMLoadFloat4x4(m_pParentMatrix));
}

#ifdef _DEBUG
HRESULT CCollider::Render(ID3D12GraphicsCommandList* _pcmdList)
{
	XMFLOAT4X4 viewMatrix = m_pGameInstance->Get_CurrentCameraView ();
	XMFLOAT4X4 projMatrix = m_pGameInstance->Get_CurrentCameraProjection ();

	m_pEffect->SetWorld(XMMatrixIdentity());
	m_pEffect->SetView(XMLoadFloat4x4(&viewMatrix));
	m_pEffect->SetProjection(XMLoadFloat4x4(&projMatrix));

	m_pEffect->Apply(_pcmdList);
	// Begin End 최적화 필요 (프레임당 한번으로 수정)
	m_pBatch->Begin(_pcmdList);
	m_pBounding->Render(m_pBatch);
	m_pBatch->End ();

	return S_OK;
}
#endif

_bool CCollider::Intersect(const CCollider* pTargetCollider)
{
	return m_pBounding->Intersect(pTargetCollider->m_eType, pTargetCollider->m_pBounding);
}

_bool CCollider::Intersect_Offset(const CCollider* pTargetCollider, const _float3& vOffset)
{
	// CBounding 객체에게 상대방의 타입, 상대방의 바운딩 볼륨, 그리고 이동할 가짜 벡터(vOffset)를 전달합니다.
	return m_pBounding->Intersect_Offset(pTargetCollider->m_eType, pTargetCollider->m_pBounding, vOffset);
}

_float3 CCollider::Get_CollisionNormal(const CCollider* pTargetCollider)
{
	// 내 Bounding 객체에게 상대방의 타입과 Bounding 객체를 넘겨서 법선을 계산하라고 지시합니다.
	return m_pBounding->Get_CollisionNormal(pTargetCollider->m_eType, pTargetCollider->m_pBounding);
}

_float3 CCollider::Get_Center()
{
	return m_pBounding->Get_Center();
}

CCollider* CCollider::Create(EngineContext* pContext, COLLIDERTYPE eColliderType)
{
	CCollider* pInstance = new CCollider(pContext);

	if (FAILED(pInstance->Initialize_Prototype(eColliderType)))
	{
		MSG_BOX("Failed to Created : CCollider");
		Safe_Release(pInstance);
	}
	return pInstance;
}
CComponent* CCollider::Clone(void* pArg)
{
	CCollider* pInstance = new CCollider(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CCollider");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CCollider::Free()
{
	__super::Free();

	Safe_Release(m_pBounding);
#ifdef _DEBUG
	
	if (false == m_isCloned)
	{
		Safe_Delete(m_pBatch);
		Safe_Delete(m_pEffect);
	}
#endif
}