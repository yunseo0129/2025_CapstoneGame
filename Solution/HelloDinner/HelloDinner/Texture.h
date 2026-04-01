#pragma once

#include "Component.h"

class CTexture final : public CComponent
{
private:
	CTexture(EngineContext* _pContext);
	CTexture(const CTexture& Prototype);
	virtual ~CTexture() = default;

public:
	// 셰이더 바인딩 함수
	virtual HRESULT Bind_ShaderResource(ID3D12GraphicsCommandList* pCommandList, RootParameterIndex iRootParameterIndex, _uint iTextureIndex = 0);

	virtual void Release_UploadBuffer();


	// DESC 정보
	_uint Get_Width() const { return m_iWidth; }
	_uint Get_Height() const { return m_iHeight; }
	DXGI_FORMAT Get_Format() const { return m_FormatDesc; }
	_uint Get_ArraySize() const { return m_iArraySize; }
	_uint Get_MipLevels() const { return m_iMipLevels; }
	TEXTURE_TYPE Get_TextureType() const { return m_eType; }

protected:

	_uint									m_iNumTextures = { 0 };
	_uint									m_iSRVIndex = { 0 }; // 글로벌 SRV힙에서의 시작 인덱스 (Manager에서 할당받은 위치)

	// 여러 장의 텍스처(애니메이션 등)를 지원하기 위해 vector로 관리 
	vector<ComPtr<ID3D12Resource>>			m_Textures;
	vector<ComPtr<ID3D12Resource>>			m_UploadBuffers; // 업로드용 임시 버퍼

	// [메타데이터 정보]
	DXGI_FORMAT					m_FormatDesc = {};
	_uint                       m_iWidth = 0;
	_uint                       m_iHeight = 0;
	_uint                       m_iArraySize = 1;  // 기본 1, 애니메이션이면 N
	_uint                       m_iMipLevels = 1;
	TEXTURE_TYPE				m_eType = TEXTURE_TYPE::TEX_2D;

private:
	// 내부 유틸리티 함수
	virtual HRESULT Load_DDSTexture(ID3D12GraphicsCommandList* pCommandList, const wstring& pFilePath, _uint iIndex);
	HRESULT Load_WICTexture(ID3D12GraphicsCommandList* pCommandList, const wstring& _pFilePath, _uint _iIndex);
	HRESULT SetResourceDesc(ID3D12Resource* _pTexture, TEXTURE_TYPE _iTextureType );

	virtual HRESULT CreateShaderResourceView(CD3DX12_CPU_DESCRIPTOR_HANDLE _descriptorHandle, _uint _iIndex, TEXTURE_TYPE _iTextureType  );

	virtual void SetHeapProperties(D3D12_HEAP_PROPERTIES& _heapProps, D3D12_HEAP_TYPE _type);
	virtual void SetResourceDesc(D3D12_RESOURCE_DESC& _resourceDesc, UINT64 _width);


public:
	virtual HRESULT Initialize_Prototype(ID3D12GraphicsCommandList* _cmdList, const _tchar* pTextureFilePath, _uint iNumTexture, TEXTURE_TYPE  _iTextureType );
	virtual HRESULT Initialize(void* pArg) override;


	static CTexture* Create(EngineContext* _pContext, const _tchar*  eFilePath, _uint iNumTextures = 1, TEXTURE_TYPE _iTextureType = TEXTURE_TYPE::TEX_2D);
	virtual CComponent* Clone(void* pArg);
	virtual void Free() override;
};