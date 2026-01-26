#pragma once
#include "stdafx.h"

// Index Buffer 미사용
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

// Index Buffer 사용
class CIndexMesh abstract : public CMesh
{
public:
	CIndexMesh() = default;
	~CIndexMesh() = default;

	virtual void Render(const ComPtr<ID3D12GraphicsCommandList>& _commandList) const override;
	virtual void ReleaseUploadBuffer() override;

protected:
	UINT						m_iIndices;
	ComPtr<ID3D12Resource>		m_pIndexBuffer;
	ComPtr<ID3D12Resource>		m_pIndexUploadBuffer;
	D3D12_INDEX_BUFFER_VIEW		m_indexBufferView;
};

class CCubeMesh : public CMesh
{
private:
	struct tVertex
	{
		XMFLOAT3 m_xmf3Position;
		XMFLOAT2 m_xmf2Uv;
	};

public:
	CCubeMesh(const ComPtr<ID3D12Device>& _device, const ComPtr<ID3D12GraphicsCommandList>& _commandList);
	~CCubeMesh() = default;
};

class CCubeIndexMesh : public CIndexMesh
{
private:
	struct tVertex
	{
		XMFLOAT3 m_xmf3Position;
		XMFLOAT4 m_xmf4Colors;
	};

public:
	CCubeIndexMesh(const ComPtr<ID3D12Device>& _device, const ComPtr<ID3D12GraphicsCommandList>& _commandList);
	~CCubeIndexMesh() = default;
};

class CSkyboxMesh : public CMesh
{
private:
	struct tVertex
	{
		XMFLOAT3 m_xmf3Position;
	};

public:
	CSkyboxMesh(const ComPtr<ID3D12Device>& _device, const ComPtr<ID3D12GraphicsCommandList>& _commandList);
	~CSkyboxMesh() override = default;
};