#include "IndexMesh.h"


void CIndexMesh::Render(const ComPtr<ID3D12GraphicsCommandList>& _commandList) const
{
	_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	_commandList->IASetIndexBuffer(&m_indexBufferView);
	_commandList->DrawIndexedInstanced(m_iIndices, 1, 0, 0, 0);
}

void CIndexMesh::ReleaseUploadBuffer()
{
	CMesh::ReleaseUploadBuffer();
	if (m_pIndexUploadBuffer) m_pIndexUploadBuffer.Reset();
}