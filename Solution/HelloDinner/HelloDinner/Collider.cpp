#include "Collider.h"

#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"
#include "GameInstance.h"

CCollider::CCollider(EngineContext* pContext)
	: CComponent{ pContext }
{
}

CCollider::CCollider(const CCollider& Prototype)
	: CComponent{ Prototype.m_pContext }
	, m_eType{ Prototype.m_eType }
#ifdef _DEBUG
	/*
	, m_pEffect{ Prototype.m_pEffect }
	, m_pBatch{ Prototype.m_pBatch }
	, m_pInputLayout{ Prototype.m_pInputLayout }
	*/
#endif // _DEBUG

	
{
#ifdef _DEBUG
	//Safe_AddRef(m_pInputLayout);
#endif // _DEBUG
}

HRESULT CCollider::Initialize_Prototype(COLLIDERTYPE eColliderType)
{
	m_eType = eColliderType;


#ifdef _DEBUG
	
	/* 박스, 구를 그려내기위한 객체들을 선언해놓는다. */
	/*
	m_pBatch = new PrimitiveBatch<VertexPositionColor>(m_pContext);
	m_pEffect = new BasicEffect(m_pDevice);

	const void* pShaderByteCode = { nullptr };
	size_t		iShaderByteCodeLength = { 0 };

	m_pEffect->SetVertexColorEnabled(true);

	m_pEffect->GetVertexShaderBytecode(&pShaderByteCode, &iShaderByteCodeLength);

	if (FAILED(m_pDevice->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount, pShaderByteCode, iShaderByteCodeLength, &m_pInputLayout)))
		return E_FAIL;
	*/
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

	m_isCloned = true;

	return S_OK;
}

void CCollider::Update(_fmatrix WorldMatrix)
{
	if (nullptr == m_pBounding)
		return;

	m_pBounding->Update(WorldMatrix);
}

#ifdef _DEBUG
/*
HRESULT CCollider::Render()
{
	m_pEffect->SetWorld(XMMatrixIdentity());
	m_pEffect->SetView(m_pGameInstance->Get_TransformMatrix(CPipeLine::D3DTS_VIEW));
	m_pEffect->SetProjection(m_pGameInstance->Get_TransformMatrix(CPipeLine::D3DTS_PROJ));

	m_pEffect->Apply(m_pContext);
	m_pContext->IASetInputLayout(m_pInputLayout);

	m_pBatch->Begin();

	m_pBounding->Render(m_pBatch);

	m_pBatch->End();

	return S_OK;
}
*/
#endif

_bool CCollider::Intersect(const CCollider* pTargetCollider)
{

	return m_pBounding->Intersect(pTargetCollider->m_eType, pTargetCollider->m_pBounding);
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
	/*
	if (false == m_isCloned)
	{
		Safe_Delete(m_pBatch);
		Safe_Delete(m_pEffect);
	}

	Safe_Release(m_pInputLayout);
	*/
#endif
}