#include "Model.h"
#include "Mesh.h"
#include "GameInstance.h"
#include "Texture.h"
#include "Material.h"

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
	
	/*
	if (FAILED(Ready_Bones()))
		return E_FAIL;
	*/

	if (FAILED(Ready_Meshes()))
		return E_FAIL;

	/*
	if (FAILED(Ready_Animations()))
		return E_FAIL;
	*/

	if (FAILED(Ready_Materials(pModelFilePath)))
		return E_FAIL;

	m_pGameInstance->Close_File();
	return S_OK;
}

HRESULT CModel::Initialize(void* pArg)
{

	return S_OK;
}

HRESULT CModel::Render(ID3D12GraphicsCommandList* _commandList, _uint iMeshIndex)
{
	if (iMeshIndex >= m_iNumMeshes)
		return E_FAIL;

	// 1. 이 메쉬가 참조하는 머티리얼의 Diffuse 텍스처 바인딩
	_uint iMaterialIndex = m_Meshes[iMeshIndex]->Get_MaterialIndex();

	if (iMaterialIndex < m_iNumMaterials)
	{
		// Diffuse (슬롯 1)
		Bind_Material(iMaterialIndex, (TextureType)TextureType_DIFFUSE, 0);

		// 필요 시 추가 텍스처 바인딩
		// Bind_Material(iMaterialIndex, (TextureType)TextureType_NORMALS, 0);
	}

	// 2. 메쉬 렌더 (IASet + DrawIndexedInstanced)
	m_Meshes[iMeshIndex]->Render(_commandList);


	return S_OK;
}


HRESULT CModel::Ready_Meshes()
{
	m_pGameInstance->Read_File(m_iNumMeshes);

	
	for (size_t i = 0; i < m_iNumMeshes; i++)
	{
		CMesh* pMesh = CMesh::Create(m_pDevice, m_pContext, m_eModelType, this, XMLoadFloat4x4(&m_PreTransformMatrix));
		if (nullptr == pMesh)
			return E_FAIL;

		m_Meshes.push_back(pMesh);
	}
	

	return S_OK;
}

HRESULT CModel::Ready_Materials(const wchar_t* pModelFilePath)
{
	m_pGameInstance->Read_File(m_iNumMaterials);

	m_Materials.resize(m_iNumMaterials);

	for (size_t i = 0; i < m_iNumMaterials; i++)
	{
		m_Materials[i] = CMaterial::Create(m_pDevice);

		for (size_t j = 0; j < 25; j++)
		{
			// 각 슬롯마다 MAX_PATH바이트 경로를 읽음
			_char szPath[MAX_PATH] = "";
			m_pGameInstance->Read_File(szPath);

			// "Not_Data"이면 텍스처 없음 → 스킵
			if (strcmp(szPath, "Not_Data") == 0)
				continue;


			// char → wchar_t 변환
			_tchar szPerfectPath[MAX_PATH] = TEXT("");
			MultiByteToWideChar(CP_ACP, 0, szPath, -1, szPerfectPath, MAX_PATH);

			CTexture* pTexture = CTexture::Create(m_pDevice, m_pContext->cmdList, szPerfectPath);
			if (pTexture != nullptr)
			{
				m_Materials[i]->Add_Texture((TextureType)j, pTexture);
			}
		}
	}
	return S_OK;
}

HRESULT CModel::Bind_Material(_uint iMeshIndex, TextureType eType, _uint iTextureIndex)
{
	if (iMeshIndex >= m_iNumMaterials)
		return E_FAIL;

	// RootParameterIndex 추가 후 변경
	RootParameterIndex rootParameterIndex = RootParameterIndex::TEXTURE;

	m_Materials[iMeshIndex]->Bind_ShaderResource(m_pContext->cmdList, eType, rootParameterIndex, iTextureIndex);
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
