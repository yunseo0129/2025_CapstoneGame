#include "VIBuffer_Rect.h"

CVIBuffer_Rect::CVIBuffer_Rect(EngineContext* _pContext)
    : CVIBuffer(_pContext)
{
}

CVIBuffer_Rect::CVIBuffer_Rect(const CVIBuffer_Rect& Prototype)
    : CVIBuffer(Prototype)
{
}

HRESULT CVIBuffer_Rect::Initialize_Prototype(ID3D12GraphicsCommandList* _pCommandList)
{
    // ---- 1) 정점: 단위 사각형 (0,0)~(1,1) ----
    //  (0,0) 좌상 ── (1,0) 우상
    //     │             │
    //  (0,1) 좌하 ── (1,1) 우하
    //  UV 는 위치와 동일하게(좌상 0,0 / 우하 1,1)
    vector<VTXUI> vertices(4);
    vertices[0].vPosition = XMFLOAT3(0.f, 0.f, 0.f); vertices[0].vTexcoord = XMFLOAT2(0.f, 0.f); // 좌상
    vertices[1].vPosition = XMFLOAT3(1.f, 0.f, 0.f); vertices[1].vTexcoord = XMFLOAT2(1.f, 0.f); // 우상
    vertices[2].vPosition = XMFLOAT3(1.f, 1.f, 0.f); vertices[2].vTexcoord = XMFLOAT2(1.f, 1.f); // 우하
    vertices[3].vPosition = XMFLOAT3(0.f, 1.f, 0.f); vertices[3].vTexcoord = XMFLOAT2(0.f, 1.f); // 좌하

    m_iVertexStride = sizeof(VTXUI);   // stride 20
    m_iVertices = (_uint)vertices.size();
    _uint vertexBufferSize = m_iVertexStride * m_iVertices;

    if (FAILED(Create_Buffer(_pCommandList, &m_pVertexBuffer, &m_pVertexUploadBuffer,
        vertexBufferSize, vertices.data(), false)))
        return E_FAIL;

    m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = m_iVertexStride;
    m_vertexBufferView.SizeInBytes = vertexBufferSize;

    // ---- 2) 인덱스: 두 삼각형 ----
    vector<_uint> indices = {0, 1, 2, 0, 2, 3};

    m_iIndices = (_uint)indices.size();
    _uint indexBufferSize = sizeof(_uint) * m_iIndices;

    if (FAILED(Create_Buffer(_pCommandList, &m_pIndexBuffer, &m_pIndexUploadBuffer,
        indexBufferSize, indices.data(), true)))
        return E_FAIL;

    m_indexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    m_indexBufferView.SizeInBytes = indexBufferSize;

    return S_OK;
}

HRESULT CVIBuffer_Rect::Initialize(void* pArg)
{
    return S_OK;
}

CVIBuffer_Rect* CVIBuffer_Rect::Create(EngineContext* _pContext)
{
    CVIBuffer_Rect* pInstance = new CVIBuffer_Rect(_pContext);
    if (FAILED(pInstance->Initialize_Prototype(_pContext->cmdList)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Create : CVIBuffer_Rect");
    }
    return pInstance;
}

CComponent* CVIBuffer_Rect::Clone(void* pArg)
{
    CVIBuffer_Rect* pInstance = new CVIBuffer_Rect(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Clone : CVIBuffer_Rect");
    }
    return pInstance;
}

void CVIBuffer_Rect::Free()
{
    __super::Free();
}