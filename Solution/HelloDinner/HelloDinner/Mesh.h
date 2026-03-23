#pragma once

#include "VIBuffer.h"
#include "Model.h"

//		VIBuffer 상속
class CMesh final : public CVIBuffer
{
private:
	CMesh(ID3D12Device* pDevice);
	virtual ~CMesh() = default;

public:
	// 멤버변수 외에 바이너리해야 될 것 있음
	virtual HRESULT Initialize_Prototype(CModel::TYPE eModelType, class CModel* pModel, _fmatrix PreTransformMatrix, ID3D12GraphicsCommandList* cmdList);
	virtual HRESULT Initialize(void* pArg);

public:
	// 애님모델과 논애님모델은 정점버퍼를 따로 사용한다
	// 바이너리화 해야할 것 있음
	HRESULT Ready_VertexBuffer_For_NonAnim(ID3D12GraphicsCommandList* cmdList, _fmatrix PreTransformMatrix);
	HRESULT Ready_VertexBuffer_For_Anim(class CModel* pModel);

private:
	// 이 매쉬의 이름
	_char						m_szName[MAX_PATH] = "";


public:
	static CMesh* Create(ID3D12Device* pDevice, EngineContext* pContext, CModel::TYPE eModelType, class CModel* pModel, _fmatrix PreTransformMatrix);
	virtual CComponent* Clone(void* pArg) override;
	virtual void Free() override;

};