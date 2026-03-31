#include "Mesh.h"
#include "GameInstance.h"
#include "Bone.h"


CMesh::CMesh(EngineContext* _pContext)
	: CVIBuffer{ _pContext }
{
}

HRESULT CMesh::Initialize_Prototype(CModel::TYPE eModelType, class CModel* pModel, _fmatrix PreTransformMatrix)
{
	// MaterialIndex 읽기
	m_pGameInstance->Read_File(m_iMaterialIndex);

	// Mesh의 이름
	m_pGameInstance->Read_File(m_szName);
	// Mesh의 정점 개수
	m_pGameInstance->Read_File(m_iVertices);
	// Mesh의 삼각형의 개수
	_uint iNumTriangles;
	m_pGameInstance->Read_File(iNumTriangles);

	/*
	// 디버그 로그 출력
	char szLog[512];
	sprintf_s(szLog, "[Mesh] Name:%s | Vtx:%u | Tri:%u | Idx:%u | MatIdx:%u\n",
		m_szName, m_iVertices, iNumTriangles, iNumTriangles * 3, m_iMaterialIndex);
	OutputDebugStringA(szLog);
	*/

	m_iIndices = iNumTriangles * 3; // 인덱스 개수는 삼각형 개수 * 3

	m_eIndexFormat = DXGI_FORMAT_R32_UINT;
	m_ePrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;


#pragma region VERTEX_BUFFER	

	HRESULT hr = CModel::TYPE_NONANIM == eModelType ? Ready_VertexBuffer_For_NonAnim(PreTransformMatrix) : Ready_VertexBuffer_For_Anim(pModel);

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
	if (FAILED(Create_Buffer(m_pContext->cmdList, &m_pIndexBuffer, &m_pIndexUploadBuffer,
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

HRESULT CMesh::Ready_VertexBuffer_For_NonAnim(_fmatrix PreTransformMatrix)
{
	// 바이너리 저장 순서: Position[] → Normal[] → TexCoord[] → Tangent[]
	vector<VTXMESH> vertices;
	vertices.resize(m_iVertices);

	for (_uint i = 0; i < m_iVertices; ++i)
		m_pGameInstance->Read_File(vertices[i].vPosition);

	for (_uint i = 0; i < m_iVertices; ++i)
		m_pGameInstance->Read_File(vertices[i].vNormal);

	for (_uint i = 0; i < m_iVertices; ++i)
		m_pGameInstance->Read_File(vertices[i].vTexcoord);

	for (_uint i = 0; i < m_iVertices; ++i)
		m_pGameInstance->Read_File(vertices[i].vTangent);

	m_iVertexStride = sizeof(VTXMESH);
	_uint vertexBufferSize = sizeof(VTXMESH) * m_iVertices;

	if (FAILED(Create_Buffer(m_pContext->cmdList, &m_pVertexBuffer, &m_pVertexUploadBuffer,
		vertexBufferSize, vertices.data(), false)))
		return E_FAIL;

	m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = m_iVertexStride;
	m_vertexBufferView.SizeInBytes = vertexBufferSize;

	return S_OK;
}

HRESULT CMesh::Ready_VertexBuffer_For_Anim(class CModel* pModel)
{
	m_pGameInstance->Read_File(m_iNumBones);

	for (size_t i = 0; i < m_iNumBones; i++)
	{
		_float4x4	OffsetMatrix = {};
		m_pGameInstance->Read_File(OffsetMatrix);
		m_BoneOffsetMatrices.push_back(OffsetMatrix);
	}
	for (size_t i = 0; i < m_iNumBones; i++)
	{
		_uint		Boneindex;
		m_pGameInstance->Read_File(Boneindex);
		m_BoneIndices.push_back(Boneindex);
	}

	if (0 == m_iNumBones)
	{
		m_iNumBones = 1;

		m_BoneIndices.push_back(pModel->Get_BoneIndex(m_szName));

		_float4x4		OffsetMatrix;
		XMStoreFloat4x4(&OffsetMatrix, XMMatrixIdentity());

		m_BoneOffsetMatrices.push_back(OffsetMatrix);
	}

	vector<VTXANIMMESH> vertices;
	vertices.resize(m_iVertices);
	for (_uint i = 0; i < m_iVertices; ++i)
		m_pGameInstance->Read_File(vertices[i].vPosition);

	for (_uint i = 0; i < m_iVertices; ++i)
		m_pGameInstance->Read_File(vertices[i].vNormal);

	for (_uint i = 0; i < m_iVertices; ++i)
		m_pGameInstance->Read_File(vertices[i].vTexcoord);

	for (_uint i = 0; i < m_iVertices; ++i)
		m_pGameInstance->Read_File(vertices[i].vTangent);

	for (_uint i = 0; i < m_iVertices; ++i)
		m_pGameInstance->Read_File(vertices[i].vBlendIndices);

	for (_uint i = 0; i < m_iVertices; ++i)
		m_pGameInstance->Read_File(vertices[i].vBlendWeights);


	m_iVertexStride = sizeof(VTXANIMMESH);
	_uint vertexBufferSize = sizeof(VTXANIMMESH) * m_iVertices;

	if (FAILED(Create_Buffer(m_pContext->cmdList, &m_pVertexBuffer, &m_pVertexUploadBuffer,
		vertexBufferSize, vertices.data(), false)))
		return E_FAIL;

	m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = m_iVertexStride;
	m_vertexBufferView.SizeInBytes = vertexBufferSize;

	return S_OK;
}

void CMesh::SetUp_BoneMatrices(const vector<CBone*>& Bones, _float4x4* pBoneMatrices)
{
	for (_uint i = 0; i < m_iNumBones; ++i)
	{
		// OffsetMatrix * CombinedMatrix = 최종 본 매트릭스
		
		_matrix OffsetMatrix = XMLoadFloat4x4(&m_BoneOffsetMatrices[i]);

		_matrix CombinedMatrix = Bones[m_BoneIndices[i]]->Get_CombinedTransformationMatrix();
		XMStoreFloat4x4(&pBoneMatrices[i], OffsetMatrix * CombinedMatrix);

		//XMStoreFloat4x4(&pBoneMatrices[i], XMMatrixInverse(nullptr, OffsetMatrix));
	}
}

CMesh* CMesh::Create(EngineContext* pContext, CModel::TYPE eModelType, class CModel* pModel, _fmatrix PreTransformMatrix)
{
	CMesh* pInstance = new CMesh(pContext);

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
