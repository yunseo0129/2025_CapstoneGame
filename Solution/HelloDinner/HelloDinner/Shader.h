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



