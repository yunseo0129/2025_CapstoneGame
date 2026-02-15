#pragma once

#include "Component.h"

//		컴포넌트 상속
class CModel final : public CComponent
{
public:
	enum TYPE { TYPE_NONANIM, TYPE_ANIM, TYPE_END };
private:
	CModel(ID3D12Device* pDevice, EngineContext* pContext);
	CModel(const CModel& Prototype);
	virtual ~CModel() = default;

public:
	// 매쉬 개수를 반환해줌
	_uint Get_NumMeshes() const {
		return m_iNumMeshes;
	}

public:
// 인자값으로 넘어온 매쉬번호에 맞는 매쉬를 그려줌 (상위 클래스의 랜더에서 매쉬개수만큼 부를거임)
	virtual HRESULT Render(_uint iMeshIndex);

private:
	// 애님과 논애님을 구별하기 위함
	TYPE						m_eModelType = { TYPE_END };

private:
	// 매쉬의 총 갯수를 저장
	_uint						m_iNumMeshes = { 0 };
	// 매쉬정보들을 저장하는 벡터
	vector<class CMesh*>		m_Meshes;

private:
	// 로컬 매트릭스처럼 사용될 미리 준비한 매트릭스임 회전, 크기 정보같은 초기값들을 담음
	_float4x4					m_PreTransformMatrix = {};

private:
	HRESULT Ready_Meshes();

private:
	ID3D12Device* m_pDevice = { nullptr };

public:
	virtual HRESULT Initialize_Prototype(TYPE eModelType, const wchar_t* pModelFilePath, _fmatrix PreTransformMatrix);
	virtual HRESULT Initialize(void* pArg) override;

	static CModel* Create(ID3D12Device* pDevice, EngineContext* pContext, TYPE eModelType, const wchar_t* pModelFilePath, _fmatrix PreTransformMatrix);
	virtual CComponent* Clone(void* pArg) override;
	virtual void Free() override;
};