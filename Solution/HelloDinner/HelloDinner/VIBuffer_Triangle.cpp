#include "VIBuffer_Triangle.h"
#include "VIBuffer_Rect.h"   // VTXUI 재사용 (POSITION + TEXCOORD, stride 20)

CVIBuffer_Triangle::CVIBuffer_Triangle(EngineContext* _pContext)
    : CVIBuffer(_pContext)
{
}

CVIBuffer_Triangle::CVIBuffer_Triangle(const CVIBuffer_Triangle& Prototype)
    : CVIBuffer(Prototype)
{
}

HRESULT CVIBuffer_Triangle::Initialize_Prototype(ID3D12GraphicsCommandList* _pCommandList)
{
    // ---- 1) 정점: 위를 가리키는 단위 삼각형 ----
    //        v0 (0.5, 0)  위 꼭짓점
    //         /        \
    //    v2 (0,1) ── v1 (1,1)
    vector<VTXUI> vertices(3);
    vertices[0].vPosition = XMFLOAT3(0.5f, 0.f, 0.f); vertices[0].vTexcoord = XMFLOAT2(0.5f, 0.f); // 위
    vertices[1].vPosition = XMFLOAT3(1.f, 1.f, 0.f);  vertices[1].vTexcoord = XMFLOAT2(1.f, 1.f);  // 우하
    vertices[2].vPosition = XMFLOAT3(0.f, 1.f, 0.f);  vertices[2].vTexcoord = XMFLOAT2(0.f, 1.f);  // 좌하

    m_iVertexStride = sizeof(VTXUI);
    m_iVertices = (_uint)vertices.size();
    _uint vertexBufferSize = m_iVertexStride * m_iVertices;

    if (FAILED(Create_Buffer(_pCommandList, &m_pVertexBuffer, &m_pVertexUploadBuffer,
        vertexBufferSize, vertices.data(), false)))
        return E_FAIL;

    m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = m_iVertexStride;
    m_vertexBufferView.SizeInBytes = vertexBufferSize;

    // ---- 2) 인덱스: 삼각형 1개 (CW = front) ----
    vector<_uint> indices = {0, 1, 2};

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

HRESULT CVIBuffer_Triangle::Initialize(void* pArg)
{
    return S_OK;
}

CVIBuffer_Triangle* CVIBuffer_Triangle::Create(EngineContext* _pContext)
{
    CVIBuffer_Triangle* pInstance = new CVIBuffer_Triangle(_pContext);
    if (FAILED(pInstance->Initialize_Prototype(_pContext->cmdList)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Create : CVIBuffer_Triangle");
    }
    return pInstance;
}

CComponent* CVIBuffer_Triangle::Clone(void* pArg)
{
    CVIBuffer_Triangle* pInstance = new CVIBuffer_Triangle(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Clone : CVIBuffer_Triangle");
    }
    return pInstance;
}

void CVIBuffer_Triangle::Free()
{
    __super::Free();
}