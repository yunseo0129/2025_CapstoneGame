#pragma once
#include "VIBuffer.h"

class CVIBuffer_Cube : public CVIBuffer
{
private:
    CVIBuffer_Cube(EngineContext* _pContext);
    CVIBuffer_Cube(const CVIBuffer_Cube& Prototype);
    virtual ~CVIBuffer_Cube() = default;

public:
    virtual HRESULT Initialize_Prototype(ID3D12GraphicsCommandList* _pCommandList) override;

    static CVIBuffer_Cube* Create(EngineContext* _pContext);
    virtual CComponent* Clone(void* pArg) override;
    virtual void Free() override;
};