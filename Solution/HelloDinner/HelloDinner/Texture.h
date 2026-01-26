#pragma once
#include "stdafx.h"

class CTexture
{
public:
	CTexture() = delete;
	CTexture(const ComPtr<ID3D12Device>& _device,
		const ComPtr<ID3D12GraphicsCommandList>& _commandList,
		const wstring& _fileName, UINT _rootParameterIndex);
	~CTexture() = default;

	void UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& _commandList) const;
	void ReleaseUploadBuffer();

private:
	void LoadTexture(const ComPtr<ID3D12Device>& _device,
		const ComPtr<ID3D12GraphicsCommandList>& _commandList,
		const wstring& _fileName);
	void CreateSrvDescriptorHeap(const ComPtr<ID3D12Device>& _device);
	void CreateShaderResourceView(const ComPtr<ID3D12Device>& _device);


private:
	ComPtr<ID3D12DescriptorHeap>	m_pSrvDescriptorHeap;
	ComPtr<ID3D12Resource>			m_pTexture;
	ComPtr<ID3D12Resource>			m_pTextureUploadBuffer;
	UINT							m_iRootParameterIndex;
};


