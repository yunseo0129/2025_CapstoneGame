#pragma once
#include "Shader.h"

class CSkyboxShader : public CShader
{
public:
	CSkyboxShader(const ComPtr<ID3D12Device>& _device, const ComPtr<ID3D12RootSignature>& _rootSignature);
	~CSkyboxShader() override = default;
};