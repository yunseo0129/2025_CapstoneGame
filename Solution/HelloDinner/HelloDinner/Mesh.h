#pragma once
#include "stdafx.h"

// Index Buffer ¹Ì»ç¿ë
class CMesh abstract
{
public:
	CMesh() = default;
	virtual ~CMesh() = default;

	virtual void Render(const ComPtr<ID3D12GraphicsCommandList>& commandList) const;
	virtual void ReleaseUploadBuffer();
	virtual void SetHeapProperties(D3D12_HEAP_PROPERTIES& _heapProps, D3D12_HEAP_TYPE _type);
	virtual void SetResourceDesc(D3D12_RESOURCE_DESC& _resourceDesc, UINT64 _width);

protected:
	UINT						m_iVertices;
	ComPtr<ID3D12Resource>		m_pVertexBuffer;
	ComPtr<ID3D12Resource>		m_pVertexUploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW	m_vertexBufferView;
};






