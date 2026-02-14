#include "PipeLine.h"

CPipeLine::CPipeLine()
{

}

HRESULT CPipeLine::Initialize()
{
	return S_OK;
}

void CPipeLine::Update()
{
	for (size_t i = 0; i < D3DTS_END; i++)
	{
		XMStoreFloat4x4(&m_TransformInverseMatrices[i], 
			XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_TransformMatrices[i])));
	}

	XMStoreFloat4(&m_vCamPosition, XMLoadFloat4((_float4*)&m_TransformInverseMatrices[D3DTS_VIEW].m[3]));	
	m_ViewProjMatrix = XMLoadFloat4x4(&m_TransformMatrices[D3DTS_VIEW]) * XMLoadFloat4x4(&m_TransformMatrices[D3DTS_PROJ]);
}

CPipeLine * CPipeLine::Create()
{
	CPipeLine*		pInstance = new CPipeLine();

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : CPipeLine");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CPipeLine::Free()
{
	__super::Free();

}