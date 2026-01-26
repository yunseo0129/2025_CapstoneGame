#pragma once
#include "stdafx.h"

class CShader abstract
{
public:
	CShader() = default;
	virtual ~CShader() = default;

	void UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& _commandList);

protected:
	ComPtr<ID3D12PipelineState> m_pPipelineState;
};

class CObjectShader : public CShader
{
public:
	CObjectShader(const ComPtr<ID3D12Device>& _device, const ComPtr<ID3D12RootSignature>& _rootSignature);
	~CObjectShader() override = default;
};

class CSkyboxShader : public CShader
{
public:
	CSkyboxShader(const ComPtr<ID3D12Device>& _device, const ComPtr<ID3D12RootSignature>& _rootSignature);
	~CSkyboxShader() override = default;
};