#pragma once

#include "Component.h"
#include "Graphic_Device.h"

//		컴포넌트 상속
class CModel final : public CComponent
{
public:
	enum TYPE { TYPE_NONANIM, TYPE_ANIM, TYPE_END };
	
	// 머티리얼 로드 모드
	enum MATERIAL_LOAD_MODE {
		MATLOAD_FROM_BINARY,     // 바이너리 경로에서 텍스처 로드 (캐릭터용)
		MATLOAD_DDS_FILE		// 바이너리는 읽되 텍스처 파일을 dds로 읽음
	};
private:
	CModel(EngineContext* pContext);
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
    _bool Get_UpperBlend() const { return m_isUpperBlend; }
    _bool Get_LowerBlend() const { return m_isLowerBlend; }
	void  Off_Blend();
	// 가지고있는 모든 뼈 순회해서 이름에 맞는 뼈 인덱스를 찾아줌
	_uint Get_BoneIndex(const _char* pBoneName) const;

	// 현재 애니메이션 번호와 루프유무를 설정해줌
	void SetUp_Animation(_uint iAnimIndex, _bool isLoop) {
		m_iCurrentAnimIndex = iAnimIndex;
		m_isLoop = isLoop;
	}
    void SetUp_Animation(_uint iAnimIndex, _bool isLoop, _bool _isUpper) {
        if (_isUpper)
        {
            m_iCurrentUpperAnimIndex = iAnimIndex;
            m_isUpperLoop = isLoop;
        }
        else
        {
            m_iCurrentLowerAnimIndex = iAnimIndex;
            m_isLowerLoop = isLoop;
        }
    }

	void Change_Animation(_uint iAnimIndex, _float fLinearDurationTime, _bool isLoop);
	// 현 애니메이션속 함수에 뼈 다 넘겨서 뼈들 트랜스폼 바꿔주고 랜더하기 위한 최종 컴바인드 행렬 만듦
	// 매 프레임 불러준다
	_bool Play_Animation(_float fTimeDelta);
	// 보간할때 사용하는 애니메이션
	_bool Blend_Animation(_float fTimeDelta);

    // 상하체 보간용
    void Change_Animation(_uint iAnimIndex, _float fLinearDurationTime, _bool isLoop, bool _isUpper);
    // 상하체 보간용
    _bool Play_Animation(_float fTimeDelta, bool _isUpper);
    // 상하체 보간용
    _bool Blend_Animation(_float fTimeDelta, bool _isUpper);
    // 상하체 보간용
    void Set_SpineBoneIndex(_uint _index)
    {
        m_iSpineBoneIndex = _index;
    }
    bool Is_UpperBone(_uint boneIndex);
    void Merge_UpperLower();

	const _float4x4* Get_BoneMatrix(const _char* pBoneName) const;

	// 랜더 때마다 호출 셰이더에 바인딩해줌
	HRESULT Bind_BoneMatrices(ID3D12GraphicsCommandList* _cmdList, _uint iMeshIndex);

	HRESULT Ready_MapMaterial(const wchar_t* pModelFilePath, int _nMaterial, TextureType _eType);
public:
// 인자값으로 넘어온 매쉬번호에 맞는 매쉬를 그려줌 (상위 클래스의 랜더에서 매쉬개수만큼 부를거임)
	virtual HRESULT Render(ID3D12GraphicsCommandList* _commandList, _uint iMeshIndex, bool IsShadow = false);

    HRESULT Render_Instanced(ID3D12GraphicsCommandList* cmdList, _uint iMeshIndex, _uint instanceCount, const D3D12_VERTEX_BUFFER_VIEW& instanceVBV, bool IsShadow = false);
private:
	// 애님과 논애님을 구별하기 위함
	TYPE						m_eModelType = { TYPE_END };
	MATERIAL_LOAD_MODE m_eMatLoadMode = { MATLOAD_FROM_BINARY };

private:
	// 매쉬의 총 갯수를 저장
	_uint						m_iNumMeshes = { 0 };
	// 매쉬정보들을 저장하는 벡터
	vector<class CMesh*>		m_Meshes;
	// 뼈 정보들을 저장하는 벡터 (베이스)
	vector<class CBone*>		m_Bones;
    // 상체시작 뼈 번호
    _uint                       m_iSpineBoneIndex;
    // 뼈 정보들을 저장하는 벡터 (상체)
    vector<class CBone*>        m_BonesUpper;
    // 뼈 정보들을 저장하는 벡터 (하체)
    vector<class CBone*>        m_BonesLower;
	// 애니메이션의 총 갯수를 저장
	_uint						m_iNumAnimations = {};
	// 애니메이션들을 저장하는 벡터
	vector<class CAnimation*>	m_Animations;

private:
	// 현재 재생되고있는 애니메이션 번호를 저장
	_uint						m_iCurrentAnimIndex = {};
    _uint                       m_iCurrentUpperAnimIndex = {};
    _uint                       m_iCurrentLowerAnimIndex = {};
	// 현재 재생되고있는 애니메이션의 루프 유무를 저장
	_bool						m_isLoop = { false };
    _bool                       m_isUpperLoop = { false };
    _bool                       m_isLowerLoop = { false };
	// 재생되고 있는 논루프 애니메이션이 끝났는지를 저장
	_bool						m_isFinished = { false };
    _bool						m_isUpperFinished = { false };
    _bool						m_isLowerFinished = { false };
	// 보간 중일때 true
	_bool						m_isBlend = { false };
    _bool                       m_isUpperBlend = { false };
    _bool                       m_isLowerBlend = { false };
	// 보간 시간
	_float						m_fBlendTime = 0;
    _float                      m_fUpperBlendTime = 0;
    _float                      m_fLowerBlendTime = 0;

private:
	// 메테리얼 총 갯수
	_uint						m_iNumMaterials = {};
	// 메테리얼들을 저장하는 벡터 -> 벡벡벡말고 메테리얼 클래스 하나만들어서 그냥 벡터로 만들것
	vector<class CMaterial*>		m_Materials;

private:
	// 로컬 매트릭스처럼 사용될 미리 준비한 매트릭스임 회전, 크기 정보같은 초기값들을 담음
	_float4x4					m_PreTransformMatrix = {};

private:

	// 본 매트릭스 Constant Buffer (프레임별)
	static const _int FRAME_COUNT = CGraphic_Device::SWAP_CHAIN_BUFFER_COUNT;
	ComPtr<ID3D12Resource>	m_pBoneBuffers[FRAME_COUNT];
	CB_BONE_MATRICES* m_pCbMappedBones[FRAME_COUNT] = {};

	HRESULT Create_BoneBuffer();

private:
	HRESULT Ready_Meshes();
	HRESULT Ready_Materials(const wchar_t* pModelFilePath, MATERIAL_LOAD_MODE eMatMode);
	HRESULT Ready_Bones();
	HRESULT Ready_Animations();

private:
	HRESULT Bind_Material(_uint iMeshIndex, TextureType eType, _uint iTextureIndex, ID3D12GraphicsCommandList* _commandList);


public:
	virtual HRESULT Initialize_Prototype(TYPE eModelType, const wchar_t* pModelFilePath, _fmatrix PreTransformMatrix, MATERIAL_LOAD_MODE eMatMode);
	virtual HRESULT Initialize(void* pArg) override;

	static CModel* Create(EngineContext* pContext, TYPE eModelType, const wchar_t* pModelFilePath, _fmatrix PreTransformMatrix, MATERIAL_LOAD_MODE eMatMode = MATLOAD_FROM_BINARY);
	virtual CComponent* Clone(void* pArg) override;
	virtual void Free() override;
};