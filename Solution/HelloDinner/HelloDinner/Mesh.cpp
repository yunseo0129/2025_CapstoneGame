#include "Mesh.h"

void CMesh::Render(const ComPtr<ID3D12GraphicsCommandList>& _commandList) const
{
	_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	_commandList->DrawInstanced(m_iVertices, 1, 0, 0);
}

void CMesh::ReleaseUploadBuffer()
{
	if (m_pVertexUploadBuffer) m_pVertexUploadBuffer.Reset();
}

void CMesh::SetHeapProperties(D3D12_HEAP_PROPERTIES& _heapProps, D3D12_HEAP_TYPE _type)
{
	_heapProps.Type = _type;
	_heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	_heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	_heapProps.CreationNodeMask = 1;
	_heapProps.VisibleNodeMask = 1;
}

void CMesh::SetResourceDesc(D3D12_RESOURCE_DESC& _resourceDesc, UINT64 _width)
{
	_resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	_resourceDesc.Alignment = 0;
	_resourceDesc.Width = _width;
	_resourceDesc.Height = 1;
	_resourceDesc.DepthOrArraySize = 1;
	_resourceDesc.MipLevels = 1;
	_resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	_resourceDesc.SampleDesc.Count = 1;
	_resourceDesc.SampleDesc.Quality = 0;
	_resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	_resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
}


