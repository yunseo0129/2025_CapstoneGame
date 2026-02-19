#include "VIBuffer.h"

// engine_context까지는 필요없을 듯
CVIBuffer::CVIBuffer(const ComPtr<ID3D12Device>& _device)
    : CComponent(nullptr)
    , m_pDevice(_device)
{
    m_ePrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

CVIBuffer::CVIBuffer(const CVIBuffer& Prototype)
    : CComponent(Prototype)
    , m_pDevice(Prototype.m_pDevice)
    , m_pVertexBuffer(Prototype.m_pVertexBuffer)             // 리소스 공유
    , m_vertexBufferView(Prototype.m_vertexBufferView)       // 뷰 정보 복사
    , m_iVertices(Prototype.m_iVertices)
    , m_iVertexStride(Prototype.m_iVertexStride)
    , m_ePrimitiveTopology(Prototype.m_ePrimitiveTopology)
    , m_pIndexBuffer(Prototype.m_pIndexBuffer)               // 리소스 공유
    , m_indexBufferView(Prototype.m_indexBufferView)         // 뷰 정보 복사
    , m_iIndices(Prototype.m_iIndices)
    , m_eIndexFormat(Prototype.m_eIndexFormat)
{
    // UploadBuffer는 복사할 필요 없음 (이미 GPU에 올라갔으므로)
    // commendlist도 업로드 버퍼 생성 시에만 필요하므로 복사하지 않음
    // Device와 Buffer는 ComPtr로 관리 중이라서 자동 addref 됨.
}

HRESULT CVIBuffer::Initialize_Prototype(ID3D12GraphicsCommandList* _pCommandList)
{
    // 기본 구현은 없음. 자식에서 구현.
    // Clone을 생성할 떄 실행
    // 이때 업로드힙, 디폴트힙 버퍼 생성까지 모두 처리해야함.

    // 1. 정점 정보 생성
    // 2. 인덱스 정보 생성
    // 3. Create_Buffer 호출 (Vertex, Index 두번 호출)
    // 4. 뷰 정보 설정
    return S_OK;
}

HRESULT CVIBuffer::Initialize(void* pArg)
{
    return S_OK;
}

HRESULT CVIBuffer::Render(ID3D12GraphicsCommandList* _commandList)
{
    // commendlist는 매 프레임마다 다를 수 있으므로 인자로 받음.
    // 공통 파이프라인 설정

    _commandList->IASetPrimitiveTopology(m_ePrimitiveTopology);
    _commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    _commandList->IASetIndexBuffer(&m_indexBufferView);
    _commandList->DrawIndexedInstanced(m_iIndices, 1, 0, 0, 0);

	// 자식에서 새로 만들 필요X - 공통으로 사용할 수 있도록 구현
    return S_OK;
}

void CVIBuffer::LogBufferViews ( const D3D12_VERTEX_BUFFER_VIEW& vbv , const D3D12_INDEX_BUFFER_VIEW& ibv )
{
    char msg[256];
    sprintf_s ( msg , "VBV: GPU=0x%llx, Size=%u, Stride=%u\n" ,
        static_cast< unsigned long long >( vbv.BufferLocation ) ,
        vbv.SizeInBytes , vbv.StrideInBytes );
    OutputDebugStringA ( msg );

    sprintf_s ( msg , "IBV: GPU=0x%llx, Size=%u, Format=%u\n" ,
        static_cast< unsigned long long >( ibv.BufferLocation ) ,
        ibv.SizeInBytes , static_cast< unsigned >( ibv.Format ) );
    OutputDebugStringA ( msg );
}

void CVIBuffer::ReleaseUploadBuffer()
{
    if (m_pVertexUploadBuffer) m_pVertexUploadBuffer.Reset();
    if (m_pIndexUploadBuffer) m_pIndexUploadBuffer.Reset();
}

void CVIBuffer::SetHeapProperties(D3D12_HEAP_PROPERTIES& _heapProps, D3D12_HEAP_TYPE _type)
{
    _heapProps.Type = _type;
    _heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    _heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    _heapProps.CreationNodeMask = 1;
    _heapProps.VisibleNodeMask = 1;
}

void CVIBuffer::SetResourceDesc(D3D12_RESOURCE_DESC& _resourceDesc, UINT64 _width)
{
    _resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    _resourceDesc.Alignment = 0;
    _resourceDesc.Width = _width;
    _resourceDesc.Height = 1;
    _resourceDesc.DepthOrArraySize = 1;
    _resourceDesc.MipLevels = 1;
    _resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    _resourceDesc.SampleDesc.Count = 1;
    _resourceDesc.SampleDesc.Quality = 0;
    _resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    _resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
}


HRESULT CVIBuffer::Create_Buffer(ID3D12GraphicsCommandList* _pCommandList, ID3D12Resource** _ppDefaultBuffer, ID3D12Resource** _ppUploadBuffer, _uint _iBufferSize, const void* _pData, bool _isIndex)
{
    D3D12_HEAP_PROPERTIES DefaultheapProps;
    SetHeapProperties(DefaultheapProps, D3D12_HEAP_TYPE_DEFAULT);

    D3D12_HEAP_PROPERTIES UploadheapProps;
    SetHeapProperties(UploadheapProps, D3D12_HEAP_TYPE_UPLOAD);

    D3D12_RESOURCE_DESC resourceDesc;
    SetResourceDesc(resourceDesc, _iBufferSize);


    // 1. Default Heap (GPU 전용) 생성
    ThrowIfFailed(m_pDevice->CreateCommittedResource(
        &DefaultheapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        NULL,
        IID_PPV_ARGS(_ppDefaultBuffer)));

    // 2. Upload Heap (CPU -> GPU 전달용) 생성
    ThrowIfFailed(m_pDevice->CreateCommittedResource(
        &UploadheapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        NULL,
        IID_PPV_ARGS(_ppUploadBuffer)));

    // 3. 데이터 복사
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = _pData;
    subResourceData.RowPitch = _iBufferSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    UpdateSubresources(_pCommandList, *_ppDefaultBuffer, *_ppUploadBuffer, 0, 0, 1, &subResourceData);

        // 4. 상태 전이 (Copy Dest -> Generic Read)

    if ( _isIndex == false ) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition (
            *_ppDefaultBuffer ,
            D3D12_RESOURCE_STATE_COPY_DEST ,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
        );
        _pCommandList->ResourceBarrier ( 1 , &barrier );
    }
    else if ( _isIndex == true ) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition (
            *_ppDefaultBuffer ,
            D3D12_RESOURCE_STATE_COPY_DEST ,
            D3D12_RESOURCE_STATE_INDEX_BUFFER
        );
        _pCommandList->ResourceBarrier ( 1 , &barrier );
    }
    
    return S_OK;
}

void CVIBuffer::Free()
{
	m_pVertexBuffer.Reset();
	m_pIndexBuffer.Reset();
	m_pDevice.Reset();
    __super::Free();
}