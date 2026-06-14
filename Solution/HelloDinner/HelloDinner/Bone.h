#pragma once

#include "Base.h"

/* assimp에서는 뼈의 정보를 세가지 타입으로 제공한다. */
/* aiNode, aiBone, aiNodeAnim */
//			CBase 상속
class CBone final : public CBase
{
private:
	CBone();
	virtual ~CBone() = default;

public:
	// 이 뼈의 이름을 반환함
	const _char* Get_Name() const {
		return m_szName;
	}
	// 이 뼈의 컴바인드된 트랜스폼 행렬을 반환함
	_matrix Get_CombinedTransformationMatrix() const {
		return XMLoadFloat4x4(&m_CombindTransformationMatrix);
	}
	const _float4x4* Get_CombinedTransformationFloat4x4() const {
		return &m_CombindTransformationMatrix;
	}
	_matrix Get_TransformationMatrix() const {
		return XMLoadFloat4x4(&m_TransformationMatrix);
	}
	// 트랜스폼 행렬에 인자행렬을 넣어줌
	void Set_TransformationMatrix(_fmatrix TransformationMatrix) {
		XMStoreFloat4x4(&m_TransformationMatrix, TransformationMatrix);
	}

    _int Get_ParentBoneIndex() {
        return m_iParentBoneIndex;
    }

public:
	HRESULT Initialize(_char* Name, _float4x4 TransformMatrix, _int ParentIndex, _float4x4 CombindTransformationMatrix);
	// 매 프레임 불림
	// 루트본은 프리트랜스폼, 나머지는 자신의 부모 컴바인드 행렬을 받아와 컴바인드 행렬을 만든다
	void Update_CombinedTransformationMatrix(const vector<CBone*>& Bones, _fmatrix PreTransformMatrix);

private:
	// 이 뼈의 이름
	_char				m_szName[MAX_PATH] = {};

	/* 부모를 기준으로 표현한 나만의 상태 행렬.  */
	_float4x4			m_TransformationMatrix = {};

	/* m_TransformationMatrix * Parent`s m_CombindTransformationMatrix  */
	_float4x4			m_CombindTransformationMatrix = {};

	// 내 부모 뼈 인덱스 번호
	_int				m_iParentBoneIndex = { -1 };

public:
	static CBone* Create(_char* Name, _float4x4 TransformMatrix, _int ParentIndex, _float4x4 CombindTransformationMatrix);
	virtual CBone* Clone();
	virtual void Free() override;
};