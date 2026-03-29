#include "VIBuffer_Cube.h"

CVIBuffer_Cube::CVIBuffer_Cube(EngineContext* _pContext)
    : CVIBuffer(_pContext)
{
}

CVIBuffer_Cube::CVIBuffer_Cube(const CVIBuffer_Cube& Prototype)
    : CVIBuffer(Prototype.m_pContext)
{
}

HRESULT CVIBuffer_Cube::Initialize_Prototype(ID3D12GraphicsCommandList* _pCommandList)
{
    // 1. 정점 데이터 정의
    vector<VTXMESH> vertices;

    const XMFLOAT3 LEFTDOWNFRONT = { -1.f, -1.f, -1.f };
    const XMFLOAT3 LEFTDOWNBACK = { -1.f, -1.f, +1.f };
    const XMFLOAT3 LEFTUPFRONT = { -1.f, +1.f, -1.f };
    const XMFLOAT3 LEFTUPBACK = { -1.f, +1.f, +1.f };
    const XMFLOAT3 RIGHTDOWNFRONT = { +1.f, -1.f, -1.f };
    const XMFLOAT3 RIGHTDOWNBACK = { +1.f, -1.f, +1.f };
    const XMFLOAT3 RIGHTUPFRONT = { +1.f, +1.f, -1.f };
    const XMFLOAT3 RIGHTUPBACK = { +1.f, +1.f, +1.f };

    // --- 상단 (Upper) 4개 정점 ---
    vertices.emplace_back(LEFTUPBACK, XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f));
    vertices.emplace_back(RIGHTUPBACK, XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f));
    vertices.emplace_back(RIGHTUPFRONT, XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f));
    vertices.emplace_back(LEFTUPFRONT, XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f));

    // --- 하단 (Down) 4개 정점 ---    
    vertices.emplace_back(LEFTDOWNBACK, XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f));
    vertices.emplace_back(RIGHTDOWNBACK, XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f));
    vertices.emplace_back(RIGHTDOWNFRONT, XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f));
    vertices.emplace_back(LEFTDOWNFRONT, XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f));

    

    m_iVertexStride = sizeof(VTXMESH);
    m_iVertices = vertices.size();
    _uint vertexBufferSize = m_iVertexStride * m_iVertices;

    // 부모의 헬퍼 함수를 이용해 Vertex Buffer 생성
    if (FAILED(Create_Buffer(_pCommandList, &m_pVertexBuffer, &m_pVertexUploadBuffer,
        vertexBufferSize, vertices.data(), false)))
        return E_FAIL;

    // 뷰 설정
    m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress ();
    m_vertexBufferView.StrideInBytes = m_iVertexStride;
    m_vertexBufferView.SizeInBytes = vertexBufferSize;

    // 2. 인덱스 데이터 정의
    vector<_uint> indices;
    indices.push_back(0); indices.push_back(1); indices.push_back(2);
    indices.push_back(0); indices.push_back(2); indices.push_back(3);

    indices.push_back(3); indices.push_back(2); indices.push_back(6);
    indices.push_back(3); indices.push_back(6); indices.push_back(7);

    indices.push_back(7); indices.push_back(6); indices.push_back(5);
    indices.push_back(7); indices.push_back(5); indices.push_back(4);

    indices.push_back(1); indices.push_back(0); indices.push_back(4);
    indices.push_back(1); indices.push_back(4); indices.push_back(5);

    indices.push_back(0); indices.push_back(3); indices.push_back(7);
    indices.push_back(0); indices.push_back(7); indices.push_back(4);

    indices.push_back(2); indices.push_back(1); indices.push_back(5);
    indices.push_back(2); indices.push_back(5); indices.push_back(6);

    m_iIndices = indices.size();
    _uint indexBufferSize = sizeof(_uint) * m_iIndices;

    // 부모의 헬퍼 함수 재활용 (Index Buffer도 Buffer이므로 동일함)
    if (FAILED(Create_Buffer(_pCommandList, &m_pIndexBuffer, &m_pIndexUploadBuffer,
        indexBufferSize, indices.data(), true)))
        return E_FAIL;

    // 뷰 설정
    m_indexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    m_indexBufferView.SizeInBytes = indexBufferSize;

    return S_OK;
}


CVIBuffer_Cube* CVIBuffer_Cube::Create(EngineContext* _pContext)
{
    CVIBuffer_Cube* pInstance = new CVIBuffer_Cube(_pContext);
    if (FAILED(pInstance->Initialize_Prototype(_pContext->cmdList)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Create : CVIBuffer_Cube");
    }
    return pInstance;
}

CComponent* CVIBuffer_Cube::Clone(void* pArg)
{
    CVIBuffer_Cube* pInstance = new CVIBuffer_Cube(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Clone : CVIBuffer_Cube");
    }
    return pInstance;
}

void CVIBuffer_Cube::Free()
{
    __super::Free();
}