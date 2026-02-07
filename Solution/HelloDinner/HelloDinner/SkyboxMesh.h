#pragma once
#include "Mesh.h"

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