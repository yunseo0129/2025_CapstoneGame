#include "Texture_Manager.h"
#include "Texture.h"

CTexture_Manager::CTexture_Manager(EngineContext* _context)
: m_pContext{ _context }
{
}

HRESULT CTexture_Manager::Initialize()
{
	// 1. SRV 힙 생성
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = m_iMaxDescriptors;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	if (FAILED(m_pContext->device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_pSrvHeap))))
		return E_FAIL;

	m_iDescriptorSize = m_pContext->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_iCurrentIndex = 0;

	return S_OK;
}


// CPU 핸들 계산
CD3DX12_CPU_DESCRIPTOR_HANDLE CTexture_Manager::Get_CPUHandle()
{
	// 현재 인덱스에 해당하는 CPU 핸들 계산
	CD3DX12_CPU_DESCRIPTOR_HANDLE hHandle(m_pSrvHeap->GetCPUDescriptorHandleForHeapStart());

	hHandle.Offset(m_iCurrentIndex, m_iDescriptorSize);

	return hHandle;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CTexture_Manager::Get_GPUHandle(_uint _iIndex)
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE hHandle(m_pSrvHeap->GetGPUDescriptorHandleForHeapStart());

	hHandle.Offset(_iIndex, m_iDescriptorSize);

	return hHandle;
}

void CTexture_Manager::Offset_DescriptorHandle(_uint _iOffset)
{
	m_iCurrentIndex += _iOffset;
}

void CTexture_Manager::Bind_GlobalHeap(ID3D12GraphicsCommandList* _pCmdList)
{
	ID3D12DescriptorHeap* ppHeaps[] = { m_pSrvHeap.Get() };
	_pCmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
}

void CTexture_Manager::initialize_DefaultTexture(ID3D12GraphicsCommandList* _pCmdList)
{
	// 기본 노말 텍스쳐 생성
	// 1x1 하늘색? 텍스쳐
	m_pDefaultNormalTexture = CTexture::Create(m_pContext, L"Resources/Textures/DefaultNormal.png", 1);

}



CTexture_Manager* CTexture_Manager::Create(EngineContext* _context)
{
	CTexture_Manager* pInstance = new CTexture_Manager(_context);
	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Create : CTexture_Manager");
		Safe_Release(pInstance);
	}
	return pInstance;
}

void CTexture_Manager::Free()
{
	__super::Free();
	m_pSrvHeap.Reset();
	Safe_Release(m_pDefaultNormalTexture);
}