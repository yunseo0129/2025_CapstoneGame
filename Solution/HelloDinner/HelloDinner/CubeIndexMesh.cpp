#include "CubeIndexMesh.h"


CCubeIndexMesh::CCubeIndexMesh(const ComPtr<ID3D12Device>& _device, const ComPtr<ID3D12GraphicsCommandList>& _commandList)
{
	vector<tVertex> vertices;

	const XMFLOAT3 LEFTDOWNFRONT = { -1.f, -1.f, -1.f };
	const XMFLOAT3 LEFTDOWNBACK = { -1.f, -1.f, +1.f };
	const XMFLOAT3 LEFTUPFRONT = { -1.f, +1.f, -1.f };
	const XMFLOAT3 LEFTUPBACK = { -1.f, +1.f, +1.f };
	const XMFLOAT3 RIGHTDOWNFRONT = { +1.f, -1.f, -1.f };
	const XMFLOAT3 RIGHTDOWNBACK = { +1.f, -1.f, +1.f };
	const XMFLOAT3 RIGHTUPFRONT = { +1.f, +1.f, -1.f };
	const XMFLOAT3 RIGHTUPBACK = { +1.f, +1.f, +1.f };

	vertices.emplace_back(LEFTUPBACK, XMFLOAT4{ 1.0f, 1.0f, 0.0f, 1.0f });
	vertices.emplace_back(RIGHTUPBACK, XMFLOAT4{ 0.0f, 0.5f, 0.5f, 1.0f });
	vertices.emplace_back(RIGHTUPFRONT, XMFLOAT4{ 0.5f, 0.5f, 0.0f, 1.0f });
	vertices.emplace_back(LEFTUPFRONT, XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f });

	vertices.emplace_back(LEFTDOWNBACK, XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f });
	vertices.emplace_back(RIGHTDOWNBACK, XMFLOAT4{ 0.0f, 1.0f, 1.0f, 1.0f });
	vertices.emplace_back(RIGHTDOWNFRONT, XMFLOAT4{ 1.0f, 0.0f, 1.0f, 1.0f });
	vertices.emplace_back(LEFTDOWNFRONT, XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f });

	vector<UINT> indices;
	indices.push_back(0); indices.push_back(1); indices.push_back(2);
	indices.push_back(0); indices.push_back(2); indices.push_back(3);

	indices.push_back(3); indices.push_back(2); indices.push_back(6);
	indices.push_back(3); indices.push_back(6); indices.push_back(7);

	indices.push_back(7); indices.push_back(6); indices.push_back(5);
	indices.push_back(7); indices.push_back(5); indices.push_back(4);

	indices.push_back(1); indices.push_back(0); indices.push_back(4);
	indices.push_back(1); indices.push_back(4); indices.push_back(5);

	indices.push_back(0); indices.push_back(3); indices.push_back(7);
	indices.push_back(0); indices.push_back(7); indices.push_back(4);

	indices.push_back(2); indices.push_back(1); indices.push_back(5);
	indices.push_back(2); indices.push_back(5); indices.push_back(6);

	m_iVertices = static_cast<UINT>(vertices.size());
	const UINT vertexBufferSize = m_iVertices * sizeof(tVertex);

	D3D12_HEAP_PROPERTIES DefaultheapProps;
	SetHeapProperties(DefaultheapProps, D3D12_HEAP_TYPE_DEFAULT);

	D3D12_HEAP_PROPERTIES UploadheapProps;
	SetHeapProperties(UploadheapProps, D3D12_HEAP_TYPE_UPLOAD);

	D3D12_RESOURCE_DESC resourceDesc;
	SetResourceDesc(resourceDesc, vertexBufferSize);

	ThrowIfFailed(_device->CreateCommittedResource(
		&DefaultheapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		NULL,
		IID_PPV_ARGS(m_pVertexBuffer.GetAddressOf())));

	ThrowIfFailed(_device->CreateCommittedResource(
		&UploadheapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,
		IID_PPV_ARGS(m_pVertexUploadBuffer.GetAddressOf())));

	D3D12_SUBRESOURCE_DATA vertexData{};
	vertexData.pData = vertices.data();
	vertexData.RowPitch = vertexBufferSize;
	vertexData.SlicePitch = vertexData.RowPitch;
	UpdateSubresources<1>(_commandList.Get(), m_pVertexBuffer.Get(), m_pVertexUploadBuffer.Get(), 0, 0, 1, &vertexData);

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_pVertexBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	_commandList->ResourceBarrier(1, &barrier);

	// 정점 버퍼 뷰 설정
	m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = vertexBufferSize;
	m_vertexBufferView.StrideInBytes = sizeof(tVertex);

	m_iIndices = static_cast<UINT>(indices.size());
	const UINT indexBufferSize = m_iIndices * sizeof(UINT);

	D3D12_RESOURCE_DESC resourceindexDesc;
	SetResourceDesc(resourceindexDesc, indexBufferSize);

	ThrowIfFailed(_device->CreateCommittedResource(
		&DefaultheapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceindexDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		NULL,
		IID_PPV_ARGS(m_pIndexBuffer.GetAddressOf())));

	ThrowIfFailed(_device->CreateCommittedResource(
		&UploadheapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceindexDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,
		IID_PPV_ARGS(m_pIndexUploadBuffer.GetAddressOf())));

	D3D12_SUBRESOURCE_DATA indexData{};
	indexData.pData = indices.data();
	indexData.RowPitch = indexBufferSize;
	indexData.SlicePitch = indexData.RowPitch;
	UpdateSubresources<1>(_commandList.Get(), m_pIndexBuffer.Get(), m_pIndexUploadBuffer.Get(), 0, 0, 1, &indexData);

	CD3DX12_RESOURCE_BARRIER indexBufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_pIndexBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_INDEX_BUFFER
	);
	_commandList->ResourceBarrier(1, &indexBufferBarrier);

	m_indexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_indexBufferView.SizeInBytes = indexBufferSize;
}
