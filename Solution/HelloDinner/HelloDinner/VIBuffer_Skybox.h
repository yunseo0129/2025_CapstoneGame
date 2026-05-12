#pragma once
#include "VIBuffer.h"

class CVIBuffer_Skybox : public CVIBuffer
{
private:
    CVIBuffer_Skybox(EngineContext* _pContext);
    CVIBuffer_Skybox(const CVIBuffer_Skybox& Prototype);
    virtual ~CVIBuffer_Skybox() = default;

public:
    virtual HRESULT Initialize_Prototype(ID3D12GraphicsCommandList* _commandList) override;

    static CVIBuffer_Skybox* Create(EngineContext* _pContext);
    virtual CComponent* Clone(void* pArg) override;
    virtual void Free() override;
};