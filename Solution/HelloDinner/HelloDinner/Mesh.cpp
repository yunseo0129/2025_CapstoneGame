#include "Mesh.h"
#include "GameInstance.h"


CMesh::CMesh(ComPtr<ID3D12Device>& pDevice)
	: CVIBuffer{ pDevice }
{
}

HRESULT CMesh::Initialize_Prototype(CModel::TYPE eModelType, class CModel* pModel, _fmatrix PreTransformMatrix)
{
	m_pGameInstance->Read_File(m_szName);

	// 이 매쉬의 정점 개수 저장해줘야할듯!
	m_pGameInstance->Read_File(m_iVertices);
	// 이 매수의 삼각형 개수 저장해줘야할듯함!
	m_pGameInstance->Read_File(m_iIndices);
	m_eIndexFormat = DXGI_FORMAT_R32_UINT;
	m_ePrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

#pragma region VERTEX_BUFFER	

	HRESULT hr = CModel::TYPE_NONANIM == eModelType ? Ready_VertexBuffer_For_NonAnim(PreTransformMatrix) : Ready_VertexBuffer_For_Anim(pModel);

	if (FAILED(hr))
		return E_FAIL;


#pragma endregion

#pragma region INDEX_BUFFER
	// 인덱스 정보 받아서 버퍼 생성

#pragma endregion

	return S_OK;
}

HRESULT CMesh::Initialize(void* pArg)
{
	return S_OK;
}

HRESULT CMesh::Ready_VertexBuffer_For_NonAnim(_fmatrix PreTransformMatrix)
{
	// 정보 받아서 buffer 생성
	return S_OK;
}

HRESULT CMesh::Ready_VertexBuffer_For_Anim(class CModel* pModel)
{
	// 정보 받아서 buffer 생성

	return S_OK;
}

CMesh* CMesh::Create(ComPtr<ID3D12Device>& pDevice, EngineContext* pContext, CModel::TYPE eModelType, class CModel* pModel, _fmatrix PreTransformMatrix)
{
	CMesh* pInstance = new CMesh(pDevice);

	if (FAILED(pInstance->Initialize_Prototype(eModelType, pModel, PreTransformMatrix)))
	{
		MSG_BOX("Failed to Created : CMesh");
		Safe_Release(pInstance);
	}
	return pInstance;
}

CComponent* CMesh::Clone(void* pArg)
{
	return nullptr;
}

void CMesh::Free()
{
	__super::Free();
}
