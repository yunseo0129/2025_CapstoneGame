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

	// CModel이 CMaterial 포인터 벡터를 가지고 있다고 가정 (vector<CMaterial*> m_Materials;)
	m_Materials.resize(m_iNumMaterials);

	for (size_t i = 0; i < m_iNumMaterials; i++)
	{
		// 1. 머티리얼 객체 생성
		m_Materials[i] = CMaterial::Create(m_pDevice);

		for (size_t j = 1; j < AI_TEXTURE_TYPE_MAX; j++)
		{
			_uint iNumTextures;
			m_pGameInstance->Read_File(iNumTextures);

			for (size_t k = 0; k < iNumTextures; k++)
			{
				_char   szExt[MAX_PATH] = "";
				m_pGameInstance->Read_File(szExt);
				_tchar  szPerfectPath[MAX_PATH] = TEXT("");
				m_pGameInstance->Read_File(szPerfectPath);

				// 2. Material이 Texture을 생성하도록 요청
				CTexture* pTexture = CTexture::Create(m_pDevice, m_pContext->cmdList, szPerfectPath, 1, TEXTURE_TYPE::TEX_2D);

				// 3. 생성된 텍스처를 머티리얼에 등록
				if (pTexture != nullptr)
				{
					m_Materials[i]->Add_Texture((aiTextureType)j, pTexture);
				}
				else
				{
					return E_FAIL; // 텍스처 생성 실패 시 에러 반환
				}
			}
		}
	}
	return S_OK;
}

HRESULT CModel::Bind_Material(_uint iMeshIndex, aiTextureType eType, _uint iTextureIndex)
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
