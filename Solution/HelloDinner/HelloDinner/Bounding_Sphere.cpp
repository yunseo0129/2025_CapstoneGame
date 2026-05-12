#include "Bounding_Sphere.h"
#include "Bounding_AABB.h"
#include "Bounding_OBB.h"

CBounding_Sphere::CBounding_Sphere(EngineContext* pContext)
	: CBounding{ pContext }
{
}

HRESULT CBounding_Sphere::Initialize(const BOUND_DESC* pBoundDesc)
{
	const BOUND_SPHERE_DESC* pDesc = static_cast<const BOUND_SPHERE_DESC*>(pBoundDesc);

	m_pOriginalBoundDesc = new BoundingSphere(pDesc->vCenter, pDesc->fRadius);
	m_pBoundDesc = new BoundingSphere(*m_pOriginalBoundDesc);

	return S_OK;
}

void CBounding_Sphere::Update(_fmatrix WorldMatrix)
{

	m_pOriginalBoundDesc->Transform(*m_pBoundDesc, WorldMatrix);
}


void CBounding_Sphere::Render(PrimitiveBatch<VertexPositionColor>* pBatch)
{
	DX::Draw(pBatch, *m_pBoundDesc, m_isColl == true ? XMVectorSet(1.f, 0.f, 0.f, 1.f) : XMVectorSet(0.f, 1.f, 0.f, 1.f));
}


_bool CBounding_Sphere::Intersect(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding)
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

_bool CBounding_Sphere::Intersect_Offset(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding, const _float3& vOffset)
{
	m_isColl = false;
	XMVECTOR vOff = XMLoadFloat3(&vOffset);

	switch (eType)
	{
	case CCollider::TYPE_AABB:
	{
		DirectX::BoundingBox tempDesc = *static_cast<CBounding_AABB*>(pTargetBounding)->Get_Desc();
		XMVECTOR vCenter = XMLoadFloat3(&tempDesc.Center);
		XMStoreFloat3(&tempDesc.Center, XMVectorAdd(vCenter, vOff));
		m_isColl = m_pBoundDesc->Intersects(tempDesc);
		break;
	}
	case CCollider::TYPE_OBB:
	{
		DirectX::BoundingOrientedBox tempDesc = *static_cast<CBounding_OBB*>(pTargetBounding)->Get_Desc();
		XMVECTOR vCenter = XMLoadFloat3(&tempDesc.Center);
		XMStoreFloat3(&tempDesc.Center, XMVectorAdd(vCenter, vOff));
		m_isColl = m_pBoundDesc->Intersects(tempDesc);
		break;
	}
	case CCollider::TYPE_SPHERE:
	{
		DirectX::BoundingSphere tempDesc = *static_cast<CBounding_Sphere*>(pTargetBounding)->Get_Desc();
		XMVECTOR vCenter = XMLoadFloat3(&tempDesc.Center);
		XMStoreFloat3(&tempDesc.Center, XMVectorAdd(vCenter, vOff));
		m_isColl = m_pBoundDesc->Intersects(tempDesc);
		break;
	}
	}

	return m_isColl;
}

_float3 CBounding_Sphere::Get_CollisionNormal(CCollider::COLLIDERTYPE eTargetType, CBounding* pTargetBounding)
{
	XMVECTOR vMe = XMLoadFloat3(&m_pBoundDesc->Center);
	XMVECTOR vNormal = XMVectorSet(0.f, 1.f, 0.f, 0.f); // �⺻�� (����)

	switch (eTargetType)
	{
	case CCollider::TYPE_AABB:
	{
		const DirectX::BoundingBox* pAABB = static_cast<CBounding_AABB*>(pTargetBounding)->Get_Desc();
		XMVECTOR vAABBCenter = XMLoadFloat3(&pAABB->Center);
		XMVECTOR vExtents = XMLoadFloat3(&pAABB->Extents);

		XMVECTOR vMin = XMVectorSubtract(vAABBCenter, vExtents);
		XMVECTOR vMax = XMVectorAdd(vAABBCenter, vExtents);

		XMVECTOR vClosestPoint = XMVectorClamp(vMe, vMin, vMax);
		XMVECTOR vDir = XMVectorSubtract(vMe, vClosestPoint);

		if (XMVector3Equal(vDir, XMVectorZero()))
			vDir = XMVectorSubtract(vMe, vAABBCenter);

		vNormal = XMVector3Normalize(vDir);
		break;
	}

	case CCollider::TYPE_OBB:
	{
		const DirectX::BoundingOrientedBox* pOBB = static_cast<CBounding_OBB*>(pTargetBounding)->Get_Desc();

		XMVECTOR vOBBCenter = XMLoadFloat3(&pOBB->Center);
		XMVECTOR vExtents = XMLoadFloat3(&pOBB->Extents);
		XMVECTOR vOrientation = XMLoadFloat4(&pOBB->Orientation); // OBB�� ȸ���� (���ʹϾ�)

		// 1. OBB�� �߽��� �������� ���� ��, ��(��ü) �߽��� ��ġ ����
		XMVECTOR vLocalSphereCenter = XMVectorSubtract(vMe, vOBBCenter);

		// 2. �� ��ġ�� OBB�� ��ȸ��(Conjugate)���� ������ ���� ����(ȸ�� �� �� AABB ����)���� ����
		XMVECTOR vInverseOrientation = XMQuaternionConjugate(vOrientation);
		vLocalSphereCenter = XMVector3Rotate(vLocalSphereCenter, vInverseOrientation);

		// 3. ���� ����(AABB)���� Extents�� �̿��� ���� ����� �� ã�� (Clamp)
		XMVECTOR vMin = XMVectorNegate(vExtents); // -Extents
		XMVECTOR vMax = vExtents;                 // +Extents
		XMVECTOR vLocalClosestPoint = XMVectorClamp(vLocalSphereCenter, vMin, vMax);

		// 4. ã�� ���� ��ǥ�� ���� ���� OBB�� ȸ�������� �ٽ� ��������
		XMVECTOR vWorldClosestPoint = XMVector3Rotate(vLocalClosestPoint, vOrientation);

		// 5. OBB�� �߽����� �ٽ� ���ؼ� �Ϻ��� ���� ��ǥ ǥ���� ������ ����
		vWorldClosestPoint = XMVectorAdd(vWorldClosestPoint, vOBBCenter);

		// 6. ���� ����� ��(ǥ��)���� �� ��ü�� ���ϴ� ���� ���� ����
		XMVECTOR vDir = XMVectorSubtract(vMe, vWorldClosestPoint);

		// ���� ó�� (�Ĺ����� ���)
		if (XMVector3Equal(vDir, XMVectorZero()))
			vDir = XMVectorSubtract(vMe, vOBBCenter);

		vNormal = XMVector3Normalize(vDir);
		break;
	}

	case CCollider::TYPE_SPHERE:
	{
		const DirectX::BoundingSphere* pOtherSphere = static_cast<CBounding_Sphere*>(pTargetBounding)->Get_Desc();
		XMVECTOR vOther = XMLoadFloat3(&pOtherSphere->Center);

		vNormal = XMVector3Normalize(XMVectorSubtract(vMe, vOther));
		break;
	}
	}

	_float3 result;
	XMStoreFloat3(&result, vNormal);
	return result;
}

_float3 CBounding_Sphere::Get_Center()
{
	return m_pBoundDesc->Center;
}

CBounding_Sphere* CBounding_Sphere::Create(EngineContext* pContext, const BOUND_DESC* pBoundDesc)
{
	CBounding_Sphere* pInstance = new CBounding_Sphere(pContext);

	if (FAILED(pInstance->Initialize(pBoundDesc)))
	{
		MSG_BOX("Failed to Created : CBounding_Sphere");
		Safe_Release(pInstance);
	}
	return pInstance;
}

void CBounding_Sphere::Free()
{
	__super::Free();

	Safe_Delete(m_pOriginalBoundDesc);
	Safe_Delete(m_pBoundDesc);
}
