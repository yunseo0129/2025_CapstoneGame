#pragma once
#include "IndexMesh.h"

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