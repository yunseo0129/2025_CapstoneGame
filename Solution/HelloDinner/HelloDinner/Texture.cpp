#include "Texture.h"
#include "../HelloDinner/Common/DDSTextureLoader12.h"

CTexture::CTexture(const ComPtr<ID3D12Device>& _device,
	const ComPtr<ID3D12GraphicsCommandList>& _commandList,
	const wstring& _fileName, UINT _rootParameterIndex) :
	m_iRootParameterIndex{ _rootParameterIndex }
{
	LoadTexture(_device, _commandList, _fileName);
	CreateSrvDescriptorHeap(_device);
	CreateShaderResourceView(_device);
}

void CTexture::UpdateShaderVariable(const ComPtr<ID3D12GraphicsCommandList>& _commandList) const
{
	ID3D12DescriptorHeap* ppHeaps[] = { m_pSrvDescriptorHeap.Get() };
	_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	CD3DX12_GPU_DESCRIPTOR_HANDLE descriptorHandle{ m_pSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart() };
	_commandList->SetGraphicsRootDescriptorTable(m_iRootParameterIndex, descriptorHandle);
}

void CTexture::ReleaseUploadBuffer()
{
	m_pTextureUploadBuffer.Reset();
}

void CTexture::LoadTexture(const ComPtr<ID3D12Device>& _device,
	const ComPtr<ID3D12GraphicsCommandList>& _commandList,
	const wstring& _fileName)
{
	unique_ptr<uint8_t[]> ddsData;
	vector<D3D12_SUBRESOURCE_DATA> subresources;
	DDS_ALPHA_MODE ddsAlphaMode{ DDS_ALPHA_MODE_UNKNOWN };
	ThrowIfFailed(DirectX::LoadDDSTextureFromFileEx(_device.Get(), _fileName.c_str(), 0,
		D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_DEFAULT, m_pTexture.GetAddressOf(), ddsData, subresources, &ddsAlphaMode));

	UINT nSubresources{ (UINT)subresources.size() };
	const UINT64 TextureSize{ GetRequiredIntermediateSize(m_pTexture.Get(), 0, nSubresources) };

	D3D12_HEAP_PROPERTIES heapUploadProps;
	heapUploadProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapUploadProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapUploadProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapUploadProps.CreationNodeMask = 1;
	heapUploadProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = TextureSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ThrowIfFailed(_device->CreateCommittedResource(
		&heapUploadProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pTextureUploadBuffer)
	));

	UpdateSubresources(_commandList.Get(), m_pTexture.Get(), m_pTextureUploadBuffer.Get(), 0, 0, nSubresources, subresources.data());

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_pTexture.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
	_commandList->ResourceBarrier(1, &barrier);
}

void CTexture::CreateSrvDescriptorHeap(const ComPtr<ID3D12Device>& _device)
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(_device->CreateDescriptorHeap(
		&srvHeapDesc, IID_PPV_ARGS(&m_pSrvDescriptorHeap)));
}

void CTexture::CreateShaderResourceView(const ComPtr<ID3D12Device>& _device)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle{
		m_pSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart() };

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_pTexture->GetDesc().Format;

	switch (m_iRootParameterIndex)
	{
	case RootParameter::Texture:
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = m_pTexture->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		break;
	case RootParameter::TextureCube:
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = m_pTexture->GetDesc().MipLevels;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		break;
	default:
		break;
	}
	_device->CreateShaderResourceView(m_pTexture.Get(), &srvDesc, descriptorHandle);
}
