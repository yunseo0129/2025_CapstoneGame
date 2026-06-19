#pragma once
#include "VIBuffer.h"

// UI 정점 구조체 (InputLayout: m_LayoutUI = POSITION(12) + TEXCOORD(8) = stride 20)
typedef struct
{
    XMFLOAT3 vPosition;   // 12 bytes
    XMFLOAT2 vTexcoord;   //  8 bytes
} VTXUI;

// 화면용 단위 사각형 (0,0)~(1,1). 두 개의 삼각형(인덱스 6).
//  - CUIObject 가 world 행렬로 위치/크기/NDC 변환을 담당한다.
//  - UV 는 좌상단(0,0) ~ 우하단(1,1).
class CVIBuffer_Rect final: public CVIBuffer
{
private:
    CVIBuffer_Rect(EngineContext* _pContext);
    CVIBuffer_Rect(const CVIBuffer_Rect& Prototype);
    virtual ~CVIBuffer_Rect() = default;

public:
    virtual HRESULT Initialize_Prototype(ID3D12GraphicsCommandList* _pCommandList) override;
    virtual HRESULT Initialize(void* pArg) override;

public:
    static CVIBuffer_Rect* Create(EngineContext* _pContext);
    virtual CComponent* Clone(void* pArg) override;
    virtual void Free() override;
};