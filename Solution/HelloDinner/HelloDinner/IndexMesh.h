#pragma once

#include "Mesh.h"

// Index Buffer »ç¿ë
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