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
	_uint Get_MaterialIndex() const { return m_iMaterialIndex; }

public:
	// 멤버변수 외에 바이너리해야 될 것 있음
	virtual HRESULT Initialize_Prototype(CModel::TYPE eModelType, class CModel* pModel, _fmatrix PreTransformMatrix, ID3D12GraphicsCommandList* cmdList);
	virtual HRESULT Initialize(void* pArg);

public:
	// 애님모델과 논애님모델은 정점버퍼를 따로 사용한다
	// 바이너리화 해야할 것 있음
	HRESULT Ready_VertexBuffer_For_NonAnim(ID3D12GraphicsCommandList* cmdList, _fmatrix PreTransformMatrix);
	HRESULT Ready_VertexBuffer_For_Anim(ID3D12GraphicsCommandList* cmdList, class CModel* pModel);

	

public:
	void SetUp_BoneMatrices(const vector<CBone*>& Bones, _float4x4* pBoneMatrices);
	const _float4x4* Get_BoneMatrices() const { return m_BoneMatrices; }
	_uint Get_NumBones() const { return m_iNumBones; }

private:
	// 이 매쉬의 이름
	_char						m_szName[MAX_PATH] = "";
	// 이 매쉬의 뼈 개수
	_uint						m_iNumBones = {};
	// 이 매쉬에 영향을 주는 뼈들의 모델에 있는 뼈 인덱스 번호들
	vector<_uint>				m_BoneIndices;
	// 뼈들의 오프셋 매트릭스
	vector<_float4x4>			m_BoneOffsetMatrices;
	// 이 매쉬의 뼈 행렬 전부
	_float4x4					m_BoneMatrices[512];

public:
	static CMesh* Create(ID3D12Device* pDevice, EngineContext* pContext, CModel::TYPE eModelType, class CModel* pModel, _fmatrix PreTransformMatrix);
	virtual CComponent* Clone(void* pArg) override;
	virtual void Free() override;

};