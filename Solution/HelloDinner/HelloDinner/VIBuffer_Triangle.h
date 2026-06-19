#pragma once
#include "VIBuffer.h"

/*
    CVIBuffer_Triangle
    --------------------------------------------------------------------
    위를 가리키는 단위 삼각형. (미니맵 플레이어 방향 마커용)
     - 정점 0~1 좌표계, CUIObject/CMiniMap 의 NDC 변환과 호환
       (InputLayout: m_LayoutUI = POSITION(12) + TEXCOORD(8), stride 20)
     - v0 = 위 꼭짓점(0.5, 0), v1 = 우하(1, 1), v2 = 좌하(0, 1)
     - 인덱스 {0,1,2}. UI PSO(Cull Back / CW=front)와 와인딩 일치.
*/
class CVIBuffer_Triangle final: public CVIBuffer
{
private:
    CVIBuffer_Triangle(EngineContext* _pContext);
    CVIBuffer_Triangle(const CVIBuffer_Triangle& Prototype);
    virtual ~CVIBuffer_Triangle() = default;

public:
    virtual HRESULT Initialize_Prototype(ID3D12GraphicsCommandList* _pCommandList) override;
    virtual HRESULT Initialize(void* pArg) override;

public:
    static CVIBuffer_Triangle* Create(EngineContext* _pContext);
    virtual CComponent* Clone(void* pArg) override;
    virtual void Free() override;
};