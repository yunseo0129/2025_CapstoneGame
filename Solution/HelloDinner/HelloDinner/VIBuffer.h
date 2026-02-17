#pragma once
#include "Component.h"

// CMesh 클래스
// 인덱스버퍼를 사용하지 않는 매쉬 사용
class CVIBuffer abstract : public CComponent
{
protected:
    // 부모 생성자: Device와 Context(CommandList)를 받아옴
    CVIBuffer(const ComPtr<ID3D12Device>& _device);
    // 복사 생성자: 프로토타입의 데이터를 복사 (얕은 복사로 리소스 공유)
    CVIBuffer(const CVIBuffer& Prototype);
    virtual ~CVIBuffer() = default;

public:
    HRESULT Render(ID3D12GraphicsCommandList* _commandList);

    void LogBufferViews ( const D3D12_VERTEX_BUFFER_VIEW& vbv , const D3D12_INDEX_BUFFER_VIEW& ibv );

    void ReleaseUploadBuffer(); // 렌더링 시작 후 업로드 버퍼 해제

    virtual void SetHeapProperties(D3D12_HEAP_PROPERTIES& _heapProps, D3D12_HEAP_TYPE _type);
    virtual void SetResourceDesc(D3D12_RESOURCE_DESC& _resourceDesc, UINT64 _width);

protected: // 자식 클래스들이 공통으로 쓸 변수
    ComPtr<ID3D12Device>                m_pDevice;

	// Vertex Buffer 관련 멤버 변수
    ComPtr<ID3D12Resource>      m_pVertexBuffer = nullptr;
    ComPtr<ID3D12Resource>      m_pVertexUploadBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW    m_vertexBufferView;

    _uint                       m_iVertices = 0;
    _uint                       m_iVertexStride = 0;
    D3D12_PRIMITIVE_TOPOLOGY    m_ePrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// Index Buffer 관련 멤버 변수
    ComPtr<ID3D12Resource>		m_pIndexBuffer = nullptr;
    ComPtr<ID3D12Resource>		m_pIndexUploadBuffer = nullptr;
    D3D12_INDEX_BUFFER_VIEW		m_indexBufferView;

    _uint						m_iIndices = 0;
    DXGI_FORMAT                 m_eIndexFormat = DXGI_FORMAT_R32_UINT; // 보통 32비트 인덱스


protected:
    // DX12 버퍼 생성 헬퍼 함수
    HRESULT Create_Buffer(ID3D12GraphicsCommandList* _pCommandList, ID3D12Resource** _ppDefaultBuffer, ID3D12Resource** _ppUploadBuffer,
        _uint _iBufferSize, const void* _pData, bool _isIndex);

public:
    virtual HRESULT Initialize_Prototype(ID3D12GraphicsCommandList* _pCommandList);
    virtual HRESULT Initialize(void* pArg) override;

    virtual CComponent* Clone(void* pArg) = 0;
    virtual void Free() override;
};