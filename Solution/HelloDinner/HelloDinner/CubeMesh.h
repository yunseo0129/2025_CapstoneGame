#pragma once
#include "Mesh.h"

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