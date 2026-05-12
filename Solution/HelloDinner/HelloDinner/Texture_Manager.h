#pragma once

// Normal 텍스쳐 같은 NULL일 수 있는 텍스쳐의 기본값을 가지고 있는 클래스
#include "Base.h"

class CTexture_Manager final : public CBase
{
private:
	CTexture_Manager(EngineContext* _context);
	virtual ~CTexture_Manager() = default;

public:
	HRESULT Initialize();

	CD3DX12_CPU_DESCRIPTOR_HANDLE Get_CPUHandle();
	CD3DX12_GPU_DESCRIPTOR_HANDLE Get_GPUHandle(_uint _iIndex);

	_uint Get_CurrentIndex() const { return m_iCurrentIndex; }
	void Offset_DescriptorHandle(_uint _iOffset);

	void Bind_GlobalHeap(ID3D12GraphicsCommandList* _pCmdList);

	void initialize_DefaultTexture(ID3D12GraphicsCommandList* _pCmdList);
	class CTexture* Get_DefaultNormalTexture() const { return m_pDefaultNormalTexture; }

private:
	EngineContext* m_pContext = { nullptr };
	ComPtr<ID3D12DescriptorHeap> m_pSrvHeap;

	_uint m_iDescriptorSize = 0;
	_uint m_iMaxDescriptors = 10000;
	_uint m_iCurrentIndex = 0;

	class CTexture* m_pDefaultNormalTexture = { nullptr };

public:
	static CTexture_Manager* Create(EngineContext* _context);
	virtual void Free() override;
};
