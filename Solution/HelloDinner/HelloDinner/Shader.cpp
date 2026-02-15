#include "Shader.h"

void CShader::UpdateShaderVariable(ID3D12GraphicsCommandList* _commandList)
{
	_commandList->SetPipelineState(m_pPipelineState.Get());
}




