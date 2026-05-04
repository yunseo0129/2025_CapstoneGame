#include "Bounding_OBB.h"
#include "Bounding_AABB.h"
#include "Bounding_Sphere.h"
CBounding_OBB::CBounding_OBB(EngineContext* pContext)
	: CBounding{ pContext }
{
}

HRESULT CBounding_OBB::Initialize(const BOUND_DESC* pBoundDesc)
{
	const BOUND_OBB_DESC* pDesc = static_cast<const BOUND_OBB_DESC*>(pBoundDesc);

	_float4		vRotation;
	XMStoreFloat4(&vRotation, XMQuaternionRotationRollPitchYaw(pDesc->vRotation.x, pDesc->vRotation.y, pDesc->vRotation.z));

	m_pOriginalBoundDesc = new BoundingOrientedBox(pDesc->vCenter, pDesc->vExtents, vRotation);
	m_pBoundDesc = new BoundingOrientedBox(*m_pOriginalBoundDesc);

	return S_OK;
}

void CBounding_OBB::Update(_fmatrix WorldMatrix)
{
	m_pOriginalBoundDesc->Transform(*m_pBoundDesc, WorldMatrix);
}

void CBounding_OBB::Render(PrimitiveBatch<VertexPositionColor>* pBatch)
{
	DX::Draw(pBatch, *m_pBoundDesc, m_isColl == true ? XMVectorSet(1.f, 0.f, 0.f, 1.f) : XMVectorSet(0.f, 1.f, 0.f, 1.f));
}

_bool CBounding_OBB::Intersect(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding)
{
	m_isColl = false;

	switch (eType)
	{
	case CCollider::TYPE_AABB:
		m_isColl = m_pBoundDesc->Intersects(*static_cast<CBounding_AABB*>(pTargetBounding)->Get_Desc());
		break;

	case CCollider::TYPE_OBB:
		m_isColl = Intersect_to_OBB(static_cast<CBounding_OBB*>(pTargetBounding));
		break;

	case CCollider::TYPE_SPHERE:
		m_isColl = m_pBoundDesc->Intersects(*static_cast<CBounding_Sphere*>(pTargetBounding)->Get_Desc());
		break;
	}

	return m_isColl;
}

_bool CBounding_OBB::Intersect_Offset(CCollider::COLLIDERTYPE eType, CBounding* pTargetBounding, const _float3& vOffset)
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

_float3 CBounding_OBB::Get_CollisionNormal(CCollider::COLLIDERTYPE eTargetType, CBounding* pTargetBounding)
{
	XMVECTOR vMeCenter = XMLoadFloat3(&m_pBoundDesc->Center);
	XMVECTOR vOrientation = XMLoadFloat4(&m_pBoundDesc->Orientation);

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

	// 내 로컬 좌표계 기준으로 상대 중심 위치 계산
	XMVECTOR vDelta = XMVectorSubtract(vTargetCenter, vMeCenter);
	XMVECTOR vInvOrient = XMQuaternionConjugate(vOrientation);
	XMVECTOR vLocalDelta = XMVector3Rotate(vDelta, vInvOrient);

	_float3 fLocal;
	XMStoreFloat3(&fLocal, vLocalDelta);

	// 로컬 좌표에서 가장 큰 축 결정
	_float fAbsX = fabsf(fLocal.x);
	_float fAbsY = fabsf(fLocal.y);
	_float fAbsZ = fabsf(fLocal.z);

	_float3 vLocalNormal {0.f, 0.f, 0.f};
	if (fAbsX >= fAbsY && fAbsX >= fAbsZ)
		vLocalNormal.x = (fLocal.x >= 0.f) ? -1.f : 1.f;   // 상대 반대방향
	else if (fAbsY >= fAbsX && fAbsY >= fAbsZ)
		vLocalNormal.y = (fLocal.y >= 0.f) ? -1.f : 1.f;
	else
		vLocalNormal.z = (fLocal.z >= 0.f) ? -1.f : 1.f;

	// 다시 월드 좌표계로 회전
	XMVECTOR vWorldNormal = XMVector3Rotate(XMLoadFloat3(&vLocalNormal), vOrientation);

	_float3 result;
	XMStoreFloat3(&result, vWorldNormal);
	return result;
}

_float3 CBounding_OBB::Get_Center()
{
	return m_pBoundDesc->Center;
}


_bool CBounding_OBB::Intersect_to_OBB(CBounding_OBB* pTarget)
{
	m_isColl = false;

	OBBDESC		OBBDesc[2];

	OBBDesc[0] = Compute_OBB();
	OBBDesc[1] = pTarget->Compute_OBB();

	_float		fLength[3] = {};

	for (size_t i = 0; i < 2; i++)
	{
		for (size_t j = 0; j < 3; j++)
		{
			_vector	vCenterDir = XMLoadFloat3(&OBBDesc[1].vCenter) - XMLoadFloat3(&OBBDesc[0].vCenter);

			fLength[0] = fabs(XMVectorGetX(XMVector3Dot(vCenterDir, XMLoadFloat3(&OBBDesc[i].vAlignDir[j]))));

			fLength[1] = fabs(XMVectorGetX(XMVector3Dot(XMLoadFloat3(&OBBDesc[0].vExtentDir[0]), XMLoadFloat3(&OBBDesc[i].vAlignDir[j])))) +
				fabs(XMVectorGetX(XMVector3Dot(XMLoadFloat3(&OBBDesc[0].vExtentDir[1]), XMLoadFloat3(&OBBDesc[i].vAlignDir[j])))) +
				fabs(XMVectorGetX(XMVector3Dot(XMLoadFloat3(&OBBDesc[0].vExtentDir[2]), XMLoadFloat3(&OBBDesc[i].vAlignDir[j]))));

			fLength[2] = fabs(XMVectorGetX(XMVector3Dot(XMLoadFloat3(&OBBDesc[1].vExtentDir[0]), XMLoadFloat3(&OBBDesc[i].vAlignDir[j])))) +
				fabs(XMVectorGetX(XMVector3Dot(XMLoadFloat3(&OBBDesc[1].vExtentDir[1]), XMLoadFloat3(&OBBDesc[i].vAlignDir[j])))) +
				fabs(XMVectorGetX(XMVector3Dot(XMLoadFloat3(&OBBDesc[1].vExtentDir[2]), XMLoadFloat3(&OBBDesc[i].vAlignDir[j]))));

			if (fLength[0] > fLength[1] + fLength[2])
				return m_isColl;
		}
	}

	return m_isColl = true;
}

CBounding_OBB::OBBDESC CBounding_OBB::Compute_OBB()
{
	OBBDESC		OBBDesc{};

	_float3		vPoints[8];

	m_pBoundDesc->GetCorners(vPoints);

	OBBDesc.vCenter = m_pBoundDesc->Center;

	_float3		vAxis[3] = {};

	XMStoreFloat3(&vAxis[0], XMLoadFloat3(&vPoints[5]) - XMLoadFloat3(&vPoints[4]));
	XMStoreFloat3(&vAxis[1], XMLoadFloat3(&vPoints[7]) - XMLoadFloat3(&vPoints[4]));
	XMStoreFloat3(&vAxis[2], XMLoadFloat3(&vPoints[0]) - XMLoadFloat3(&vPoints[4]));

	for (size_t i = 0; i < 3; i++)
		XMStoreFloat3(&OBBDesc.vExtentDir[i], XMLoadFloat3(&vAxis[i]) * 0.5f);

	for (size_t i = 0; i < 3; i++)
		XMStoreFloat3(&OBBDesc.vAlignDir[i], XMVector3Normalize(XMLoadFloat3(&vAxis[i])));

	return OBBDesc;
}

CBounding_OBB* CBounding_OBB::Create(EngineContext* pContext, const BOUND_DESC* pBoundDesc)
{
	CBounding_OBB* pInstance = new CBounding_OBB(pContext);

	if (FAILED(pInstance->Initialize(pBoundDesc)))
	{
		MSG_BOX("Failed to Created : CBounding_OBB");
		Safe_Release(pInstance);
	}
	return pInstance;
}

void CBounding_OBB::Free()
{
	__super::Free();

	Safe_Delete(m_pOriginalBoundDesc);
	Safe_Delete(m_pBoundDesc);
}