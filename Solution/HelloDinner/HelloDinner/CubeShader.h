#pragma once
#include "Shader.h"

class CCubeShader : public CShader
{
public:
	CCubeShader(const ComPtr<ID3D12Device>& _device, const ComPtr<ID3D12RootSignature>& _rootSignature);
	~CCubeShader() override = default;
};

