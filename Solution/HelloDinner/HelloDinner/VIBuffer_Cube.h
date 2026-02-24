#pragma once
#include "VIBuffer.h"

class CVIBuffer_Cube : public CVIBuffer
{
private:
    CVIBuffer_Cube(ID3D12Device* _device);
    CVIBuffer_Cube(const CVIBuffer_Cube& Prototype);
    virtual ~CVIBuffer_Cube() = default;

public:
    virtual HRESULT Initialize_Prototype(ID3D12GraphicsCommandList* _pCommandList) override;

    static CBase* Create(ID3D12Device* _device, ID3D12GraphicsCommandList* _commandList);
    virtual CComponent* Clone(void* pArg) override;
    virtual void Free() override;
};