#pragma once

#include "Component.h"

//		컴포넌트 상속
class CModel final : public CComponent
{
public:
	enum TYPE { TYPE_NONANIM, TYPE_ANIM, TYPE_END };
	
	// 머티리얼 로드 모드
	enum MATERIAL_LOAD_MODE {
		MATLOAD_FROM_BINARY,     // 바이너리 경로에서 텍스처 로드 (캐릭터용)
		MATLOAD_SKIP_TEXTURE     // 바이너리는 읽되 텍스처 생성 스킵 (맵용, 외부에서 Set)
	};
private:
	CModel(ID3D12Device* pDevice, EngineContext* pContext);
	CModel(const CModel& Prototype);
	virtual ~CModel() = default;

public:
	// Material 슬롯에 텍스쳐를 세팅하는 함수
	HRESULT Set_MaterialTexture(_uint iMaterialIndex, TextureType eType, class CTexture* pTexture);
	// 매쉬 개수를 반환해줌
	_uint Get_NumMeshes() const {
		return m_iNumMeshes;
	}
	_uint Get_NumMaterials() const {
		return m_iNumMaterials;
	}
	_bool Get_Blend() const { return m_isBlend; }
	void  Off_Blend();
	// 가지고있는 모든 뼈 순회해서 이름에 맞는 뼈 인덱스를 찾아줌
	_uint Get_BoneIndex(const _char* pBoneName) const;

	// 현재 애니메이션 번호와 루프유무를 설정해줌
	void SetUp_Animation(_uint iAnimIndex, _bool isLoop) {
		m_iCurrentAnimIndex = iAnimIndex;
		m_isLoop = isLoop;
	}

	void Change_Animation(_uint iAnimIndex, _float fLinearDurationTime, _bool isLoop);
	// 현 애니메이션속 함수에 뼈 다 넘겨서 뼈들 트랜스폼 바꿔주고 랜더하기 위한 최종 컴바인드 행렬 만듦
	// 매 프레임 불러준다
	_bool Play_Animation(_float fTimeDelta);
	// 보간할때 사용하는 애니메이션
	_bool Blend_Animation(_float fTimeDelta);
	const _float4x4* Get_BoneMatrix(const _char* pBoneName) const;

	// 랜더 때마다 호출 셰이더에 바인딩해줌
	HRESULT Bind_BoneMatrices(class CShader* pShader, const _char* pConstantName, _uint iMeshIndex);

public:
// 인자값으로 넘어온 매쉬번호에 맞는 매쉬를 그려줌 (상위 클래스의 랜더에서 매쉬개수만큼 부를거임)
	virtual HRESULT Render(ID3D12GraphicsCommandList* _commandList, _uint iMeshIndex);

private:
	// 애님과 논애님을 구별하기 위함
	TYPE						m_eModelType = { TYPE_END };
	MATERIAL_LOAD_MODE m_eMatLoadMode = { MATLOAD_FROM_BINARY };

private:
	// 매쉬의 총 갯수를 저장
	_uint						m_iNumMeshes = { 0 };
	// 매쉬정보들을 저장하는 벡터
	vector<class CMesh*>		m_Meshes;
	// 뼈 정보들을 저장하는 벡터
	vector<class CBone*>		m_Bones;
	// 애니메이션의 총 갯수를 저장
	_uint						m_iNumAnimations = {};
	// 애니메이션들을 저장하는 벡터
	vector<class CAnimation*>	m_Animations;

private:
	// 현재 재생되고있는 애니메이션 번호를 저장
	_uint						m_iCurrentAnimIndex = {};
	// 현재 재생되고있는 애니메이션의 루프 유무를 저장
	_bool						m_isLoop = { false };
	// 재생되고 있는 논루프 애니메이션이 끝났는지를 저장
	_bool						m_isFinished = { false };
	// 보간 중일때 true
	_bool						m_isBlend = { false };
	// 보간 시간
	_float						m_fBlendTime = 0;

private:
	// 메테리얼 총 갯수
	_uint						m_iNumMaterials = {};
	// 메테리얼들을 저장하는 벡터 -> 벡벡벡말고 메테리얼 클래스 하나만들어서 그냥 벡터로 만들것
	vector<class CMaterial*>		m_Materials;

private:
	// 로컬 매트릭스처럼 사용될 미리 준비한 매트릭스임 회전, 크기 정보같은 초기값들을 담음
	_float4x4					m_PreTransformMatrix = {};

private:
	HRESULT Ready_Meshes();
	HRESULT Ready_Materials(const wchar_t* pModelFilePath);
	HRESULT Ready_Bones();
	HRESULT Ready_Animations();

private:
	HRESULT Bind_Material(_uint iMeshIndex, TextureType eType, _uint iTextureIndex, ID3D12GraphicsCommandList* _commandList);

private:
	ID3D12Device* m_pDevice = { nullptr };

public:
	virtual HRESULT Initialize_Prototype(TYPE eModelType, const wchar_t* pModelFilePath, _fmatrix PreTransformMatrix, MATERIAL_LOAD_MODE eMatMode);
	virtual HRESULT Initialize(void* pArg) override;

	static CModel* Create(ID3D12Device* pDevice, EngineContext* pContext, TYPE eModelType, const wchar_t* pModelFilePath, _fmatrix PreTransformMatrix, MATERIAL_LOAD_MODE eMatMode = MATLOAD_FROM_BINARY);
	virtual CComponent* Clone(void* pArg) override;
	virtual void Free() override;
};