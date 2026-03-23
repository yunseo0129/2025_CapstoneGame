#include "Mesh.h"
#include "GameInstance.h"


CMesh::CMesh(ID3D12Device* pDevice)
	: CVIBuffer{ pDevice }
{
}

HRESULT CMesh::Initialize_Prototype(CModel::TYPE eModelType, class CModel* pModel, _fmatrix PreTransformMatrix, ID3D12GraphicsCommandList* cmdList)
{
	// Mesh의 이름
	m_pGameInstance->Read_File(m_szName);
	// Mesh의 정점 개수
	m_pGameInstance->Read_File(m_iVertices);
	// Mesh의 인덱스 개수
	m_pGameInstance->Read_File(m_iIndices);

	m_eIndexFormat = DXGI_FORMAT_R32_UINT;
	m_ePrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

#pragma region VERTEX_BUFFER	

	HRESULT hr = CModel::TYPE_NONANIM == eModelType ? Ready_VertexBuffer_For_NonAnim(cmdList , PreTransformMatrix) : Ready_VertexBuffer_For_Anim(pModel);

	if (FAILED(hr))
		return E_FAIL;


#pragma endregion

#pragma region INDEX_BUFFER
	// 인덱스 정보 받아서 버퍼 생성
	vector<_uint> indices;
	indices.resize(m_iIndices);

	for (_uint i = 0; i < m_iIndices; ++i)
	{
		m_pGameInstance->Read_File(indices[i]);
	}

	_uint indexBufferSize = sizeof(_uint) * m_iIndices;

	// 부모의 버퍼 생성 함수를 이용해 Index Buffer 생성
	if (FAILED(Create_Buffer(cmdList, m_pIndexBuffer.GetAddressOf(), m_pIndexUploadBuffer.GetAddressOf(),
		indexBufferSize, indices.data(), true)))
		return E_FAIL;

	// 뷰 설정
	m_indexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = m_eIndexFormat;
	m_indexBufferView.SizeInBytes = indexBufferSize;
#pragma endregion

	return S_OK;
}

HRESULT CMesh::Initialize(void* pArg)
{
	return S_OK;
}

HRESULT CMesh::Ready_VertexBuffer_For_NonAnim(ID3D12GraphicsCommandList* cmdList, _fmatrix PreTransformMatrix)
{
	// 바이너리 저장 순서: Position[] → Normal[] → TexCoord[] → Tangent[]
	vector<VTXMESH> vertices;

	for (_uint i = 0; i < m_iVertices; ++i)
		m_pGameInstance->Read_File(vertices[i]);

	_uint vertexBufferSize = m_iVertexStride * m_iVertices;

	if (FAILED(Create_Buffer(cmdList, m_pVertexBuffer.GetAddressOf(), m_pVertexUploadBuffer.GetAddressOf(),
		vertexBufferSize, vertices.data(), false)))
		return E_FAIL;

	m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = m_iVertexStride;
	m_vertexBufferView.SizeInBytes = vertexBufferSize;

	return S_OK;
}

HRESULT CMesh::Ready_VertexBuffer_For_Anim(class CModel* pModel)
{
	// 정보 받아서 buffer 생성

	return S_OK;
}

CMesh* CMesh::Create(ID3D12Device* pDevice, EngineContext* pContext, CModel::TYPE eModelType, class CModel* pModel, _fmatrix PreTransformMatrix)
{
	CMesh* pInstance = new CMesh(pDevice);

	if (FAILED(pInstance->Initialize_Prototype(eModelType, pModel, PreTransformMatrix, pContext->cmdList)))
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
