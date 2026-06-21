#include "Bounding_AABB.h"

#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"

CBounding_AABB::CBounding_AABB(EngineContext* pContext)
	: CBounding{ pContext }
{
}

HRESULT CBounding_AABB::Initialize(const BOUND_DESC* pBoundDesc)
{
	const BOUND_AABB_DESC* pDesc = static_cast<const BOUND_AABB_DESC*>(pBoundDesc);

	m_pOriginalBoundDesc = new BoundingBox(pDesc->vCenter, pDesc->vExtents);
	m_pBoundDesc = new BoundingBox(*m_pOriginalBoundDesc);

	return S_OK;
}

void CBounding_AABB::Update(_fmatrix WorldMatrix)
{
	_matrix			TransformMatrix = WorldMatrix;

	TransformMatrix.r[0] = XMVectorSet(1.f, 0.f, 0.f, 0.f) * XMVector3Length(TransformMatrix.r[0]);
	TransformMatrix.r[1] = XMVectorSet(0.f, 1.f, 0.f, 0.f) * XMVector3Length(TransformMatrix.r[1]);
	TransformMatrix.r[2] = XMVectorSet(0.f, 0.f, 1.f, 0.f) * XMVector3Length(TransformMatrix.r[2]);

	m_pOriginalBoundDesc->Transform(*m_pBoundDesc, TransformMatrix);
}

void CBounding_AABB::Render(PrimitiveBatch<VertexPositionColor>* pBatch)
{
	DX::Draw(pBatch, *m_pBoundDesc, m_isColl == true ? XMVectorSet(1.f, 0.f, 0.f, 1.f) : XMVectorSet(0.f, 1.f, 0.f, 1.f));
}

_bool CBounding_AABB::Intersect(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding)
{
	m_isColl = false;

	switch (eType)
	{
	case CCollider::TYPE_AABB:
		m_isColl = m_pBoundDesc->Intersects(*static_cast<CBounding_AABB*>(pTargetBounding)->Get_Desc());		
		break;

	case CCollider::TYPE_OBB:
		m_isColl = m_pBoundDesc->Intersects(*static_cast<CBounding_OBB*>(pTargetBounding)->Get_Desc());
		break;

	case CCollider::TYPE_SPHERE:
		m_isColl = m_pBoundDesc->Intersects(*static_cast<CBounding_Sphere*>(pTargetBounding)->Get_Desc());
		break;
	}

	return m_isColl;
}

bool CBounding_AABB::IntersectsRay(FXMVECTOR rayOrigin, FXMVECTOR rayDir, float& distance)
{
    return m_pBoundDesc->Intersects(
        rayOrigin,
        rayDir,
        distance);
}

_bool CBounding_AABB::Intersect_Offset(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding, const _float3& vOffset)
{
	m_isColl = false;

	XMVECTOR vOff = XMLoadFloat3(&vOffset);

	switch (eType)
	{
	case CCollider::TYPE_AABB:
	{
		// 임시 바운딩 박스 생성
		DirectX::BoundingBox tempDesc = *static_cast<CBounding_AABB*>(pTargetBounding)->Get_Desc();

		// 임시 바운딩 박스에 Offset 적용
		XMVECTOR vCenter = XMLoadFloat3(&tempDesc.Center);
		vCenter = XMVectorAdd(vCenter, vOff); 
		XMStoreFloat3(&tempDesc.Center, vCenter);

		// 나와 임시 바운딩 박스의 충돌 여부 체크
		m_isColl = m_pBoundDesc->Intersects(tempDesc);
		break;
	}

	case CCollider::TYPE_OBB:
	{
		DirectX::BoundingOrientedBox tempDesc = *static_cast<CBounding_OBB*>(pTargetBounding)->Get_Desc();

		XMVECTOR vCenter = XMLoadFloat3(&tempDesc.Center);
		vCenter = XMVectorAdd(vCenter, vOff);
		XMStoreFloat3(&tempDesc.Center, vCenter);

		m_isColl = m_pBoundDesc->Intersects(tempDesc);
		break;
	}

	case CCollider::TYPE_SPHERE:
	{
		DirectX::BoundingSphere tempDesc = *static_cast<CBounding_Sphere*>(pTargetBounding)->Get_Desc();

		XMVECTOR vCenter = XMLoadFloat3(&tempDesc.Center);
		vCenter = XMVectorAdd(vCenter, vOff);
		XMStoreFloat3(&tempDesc.Center, vCenter);

		m_isColl = m_pBoundDesc->Intersects(tempDesc);
		break;
	}
	}

	return m_isColl;
}

_float3 CBounding_AABB::Get_CollisionNormal(CCollider::COLLIDERTYPE eTargetType, CBounding* pTargetBounding)
{
	XMVECTOR vMeCenter = XMLoadFloat3(&m_pBoundDesc->Center);
	XMVECTOR vTargetCenter = XMVectorZero();

	switch (eTargetType)
	{
	case CCollider::TYPE_AABB:
		vTargetCenter = XMLoadFloat3(&static_cast<CBounding_AABB*>(pTargetBounding)->Get_Desc()->Center);
		break;
	case CCollider::TYPE_OBB:
		vTargetCenter = XMLoadFloat3(&static_cast<CBounding_OBB*>(pTargetBounding)->Get_Desc()->Center);
		break;
	case CCollider::TYPE_SPHERE:
		vTargetCenter = XMLoadFloat3(&static_cast<CBounding_Sphere*>(pTargetBounding)->Get_Desc()->Center);
		break;
	}

	// 내 중심에서 상대 중심으로 향하는 벡터의 가장 큰 축이 충돌 면의 법선
	XMVECTOR vDelta = XMVectorSubtract(vMeCenter, vTargetCenter);
	_float3 fDelta;
	XMStoreFloat3(&fDelta, vDelta);

	_float fAbsX = fabsf(fDelta.x);
	_float fAbsY = fabsf(fDelta.y);
	_float fAbsZ = fabsf(fDelta.z);

	_float3 vNormal {0.f, 0.f, 0.f};
	if (fAbsX >= fAbsY && fAbsX >= fAbsZ)
		vNormal.x = (fDelta.x >= 0.f) ? 1.f : -1.f;
	else if (fAbsY >= fAbsX && fAbsY >= fAbsZ)
		vNormal.y = (fDelta.y >= 0.f) ? 1.f : -1.f;
	else
		vNormal.z = (fDelta.z >= 0.f) ? 1.f : -1.f;

	return vNormal;
}

void CBounding_AABB::Get_SphereBound(_float3& outCenter, _float& outRadius) const
{
    outCenter = m_pBoundDesc->Center;
    XMVECTOR vExt = XMLoadFloat3(&m_pBoundDesc->Extents);
    XMStoreFloat(&outRadius, XMVector3Length(vExt));  // 대각선 절반 = 외접구 반지름
}

void CBounding_AABB::Get_AABBBound(_float3& c, _float3& e) const {
    c = m_pBoundDesc->Center;
    e = m_pBoundDesc->Extents;
}

CBounding_AABB* CBounding_AABB::Create(EngineContext* pContext, const BOUND_DESC* pBoundDesc)
{
	CBounding_AABB* pInstance = new CBounding_AABB(pContext);

	if (FAILED(pInstance->Initialize(pBoundDesc)))
	{
		MSG_BOX("Failed to Created : CBounding_AABB");
		Safe_Release(pInstance);
	}
	return pInstance;
}

void CBounding_AABB::Free()
{
	__super::Free();

	Safe_Delete(m_pOriginalBoundDesc);
	Safe_Delete(m_pBoundDesc);
}

_float3 CBounding_AABB::Get_Center()
{
	return m_pBoundDesc->Center;
}