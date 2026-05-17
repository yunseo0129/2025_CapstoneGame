#include "Bone.h"

CBone::CBone()
{

}

HRESULT CBone::Initialize(_char* Name, _float4x4 TransformMatrix, _int ParentIndex, _float4x4 CombindTransformationMatrix)
{
	strcpy_s(m_szName, Name);
	m_TransformationMatrix = TransformMatrix;
	//XMStoreFloat4x4(&m_CombindTransformationMatrix, CombindTransformationMatrix);
	m_CombindTransformationMatrix = CombindTransformationMatrix;
	m_iParentBoneIndex = ParentIndex;

	return S_OK;
}

void CBone::Update_CombinedTransformationMatrix(const vector<CBone*>& Bones, _fmatrix PreTransformMatrix)
{
	// 루트 본 일 때		자신 트랜스폼에 프리트랜스폼만을 적용한 값을 자기 컴바인드에 저장
	if (-1 == m_iParentBoneIndex)
	{
		XMStoreFloat4x4(&m_CombindTransformationMatrix, XMLoadFloat4x4(&m_TransformationMatrix) * PreTransformMatrix);
		return;
	}
	// 루트 본 아닐 때		자신 트랜스폼에 부모 컴바인드행렬을 적용한 값을 자기 컴바인드에 저장
	XMStoreFloat4x4(&m_CombindTransformationMatrix, XMLoadFloat4x4(&m_TransformationMatrix) *
		XMLoadFloat4x4(&Bones[m_iParentBoneIndex]->m_CombindTransformationMatrix));
}

CBone* CBone::Create(_char* Name, _float4x4 TransformMatrix, _int ParentIndex, _float4x4 CombindTransformationMatrix)
{
	CBone* pInstance = new CBone();

	if (FAILED(pInstance->Initialize(Name, TransformMatrix, ParentIndex, CombindTransformationMatrix)))
	{
		MSG_BOX("Failed to Created : CBone");
		Safe_Release(pInstance);
	}
	return pInstance;
}

CBone* CBone::Clone()
{
	return new CBone(*this);
}

void CBone::Free()
{
	__super::Free();
}
