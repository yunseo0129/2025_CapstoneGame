#include "SkyboxMesh.h"


CSkyboxMesh::CSkyboxMesh(const ComPtr<ID3D12Device>& _device, const ComPtr<ID3D12GraphicsCommandList>& _commandList)
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

	// Front
	vertices.emplace_back(LEFTUPFRONT);
	vertices.emplace_back(RIGHTUPFRONT);
	vertices.emplace_back(RIGHTDOWNFRONT);

	vertices.emplace_back(LEFTUPFRONT);
	vertices.emplace_back(RIGHTDOWNFRONT);
	vertices.emplace_back(LEFTDOWNFRONT);

	// Up
	vertices.emplace_back(LEFTUPBACK);
	vertices.emplace_back(RIGHTUPBACK);
	vertices.emplace_back(RIGHTUPFRONT);

	vertices.emplace_back(LEFTUPBACK);
	vertices.emplace_back(RIGHTUPFRONT);
	vertices.emplace_back(LEFTUPFRONT);

	// Back
	vertices.emplace_back(LEFTDOWNBACK);
	vertices.emplace_back(RIGHTDOWNBACK);
	vertices.emplace_back(RIGHTUPBACK);

	vertices.emplace_back(LEFTDOWNBACK);
	vertices.emplace_back(RIGHTUPBACK);
	vertices.emplace_back(LEFTUPBACK);

	// Down
	vertices.emplace_back(LEFTDOWNFRONT);
	vertices.emplace_back(RIGHTDOWNFRONT);
	vertices.emplace_back(RIGHTDOWNBACK);

	vertices.emplace_back(LEFTDOWNFRONT);
	vertices.emplace_back(RIGHTDOWNBACK);
	vertices.emplace_back(LEFTDOWNBACK);

	// Left
	vertices.emplace_back(LEFTUPBACK);
	vertices.emplace_back(LEFTUPFRONT);
	vertices.emplace_back(LEFTDOWNFRONT);

	vertices.emplace_back(LEFTUPBACK);
	vertices.emplace_back(LEFTDOWNFRONT);
	vertices.emplace_back(LEFTDOWNBACK);

	// Right													 
	vertices.emplace_back(RIGHTUPFRONT);
	vertices.emplace_back(RIGHTUPBACK);
	vertices.emplace_back(RIGHTDOWNBACK);

	vertices.emplace_back(RIGHTUPFRONT);
	vertices.emplace_back(RIGHTDOWNBACK);
	vertices.emplace_back(RIGHTDOWNFRONT);

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
		nullptr,
		IID_PPV_ARGS(&m_pVertexBuffer)));

	ThrowIfFailed(_device->CreateCommittedResource(
		&UploadheapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pVertexUploadBuffer)));

	D3D12_SUBRESOURCE_DATA vertexData{};
	vertexData.pData = vertices.data();
	vertexData.RowPitch = vertexBufferSize;
	vertexData.SlicePitch = vertexData.RowPitch;
	UpdateSubresources<1>(_commandList.Get(),
		m_pVertexBuffer.Get(), m_pVertexUploadBuffer.Get(), 0, 0, 1, &vertexData);

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_pVertexBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	_commandList->ResourceBarrier(1, &barrier);

	m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = vertexBufferSize;
	m_vertexBufferView.StrideInBytes = sizeof(tVertex);
}
