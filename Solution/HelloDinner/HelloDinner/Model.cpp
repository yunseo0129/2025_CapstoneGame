#include "Model.h"
#include "Mesh.h"
#include "Shader.h"
#include "GameInstance.h"

CModel::CModel(ID3D12Device* pDevice, EngineContext* pContext)
	: CComponent{ pContext }
	, m_pDevice{ pDevice }
{
}

CModel::CModel(const CModel& Prototype)
	: CComponent{ nullptr }
	, m_eModelType{ Prototype.m_eModelType }
	, m_iNumMeshes{ Prototype.m_iNumMeshes }
	, m_Meshes{ Prototype.m_Meshes }
	, m_PreTransformMatrix{ Prototype.m_PreTransformMatrix }
{
	for (auto& pMesh : m_Meshes)
		Safe_AddRef(pMesh);
}

HRESULT CModel::Initialize_Prototype(TYPE eModelType, const wchar_t* pModelFilePath, _fmatrix PreTransformMatrix)
{
	m_eModelType = eModelType;

	XMStoreFloat4x4(&m_PreTransformMatrix, PreTransformMatrix);

	m_pGameInstance->Open_File(pModelFilePath);


	if (FAILED(Ready_Meshes()))
		return E_FAIL;

	m_pGameInstance->Close_File();
	return S_OK;
}

HRESULT CModel::Initialize(void* pArg)
{

	return S_OK;
}

HRESULT CModel::Render(_uint iMeshIndex)
{
	if (iMeshIndex >= m_iNumMeshes)
		return E_FAIL;

	// 잠깐 주석처리
	// m_Meshes[iMeshIndex]->Render();
	
	return S_OK;
}


HRESULT CModel::Ready_Meshes()
{
	m_pGameInstance->Read_File(m_iNumMeshes);

	/*
	for (size_t i = 0; i < m_iNumMeshes; i++)
	{
		CMesh* pMesh = CMesh::Create(m_pDevice, m_pContext, m_eModelType, this, XMLoadFloat4x4(&m_PreTransformMatrix));
		if (nullptr == pMesh)
			return E_FAIL;

		m_Meshes.push_back(pMesh);
	}
	*/

	return S_OK;
}


CModel* CModel::Create(ID3D12Device* pDevice, EngineContext* pContext, TYPE eModelType, const wchar_t* pModelFilePath, _fmatrix PreTransformMatrix)
{
	CModel* pInstance = new CModel(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype(eModelType, pModelFilePath, PreTransformMatrix)))
	{
		MSG_BOX("Failed to Created : CModel");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CComponent* CModel::Clone(void* pArg)
{
	CModel* pInstance = new CModel(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CModel");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CModel::Free()
{
	__super::Free();

	for (auto& pMesh : m_Meshes)
		Safe_Release(pMesh);

	m_Meshes.clear();

}
