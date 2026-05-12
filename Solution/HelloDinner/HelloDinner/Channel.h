#pragma once

#include "Base.h"

/* 0. 이 뼈가 소속된 애니메이션을 표현하기위해서 뼈는 어떤 상태(KeyFrame)(행렬)들을 가지는가?! 에 대한 정보를 가진다. */
/* 1. 현재 재생시간에 맞는 뼈의 상태행렬을 만들어낸다.  */
/* 2. 같은 이름을 가진 채널이 다수 존재할 수 있다?! */
//		CBase 상속
class CChannel final : public CBase
{
private:
	CChannel();
	virtual ~CChannel() = default;

public:
	HRESULT Initialize(const class CModel* pModel);
	// 매 프레임 불림
	// 채널의 인덱스번호와 같은 뼈를 현 재생 중인 애님의 해당 뼈의 매트릭스를 키프레임으로 보간해줌
	void Update_TransformationMatrix(_float fCurrentTrackPosition, _uint* pKeyFrameIndex, const vector<class CBone*>& Bones);
	void Update_TransformationMatrix(_float fCurrentTrackPosition, _float fBlendTime, const vector<class CBone*>& Bones);
	void Channel_Init() { m_BeforeKeyFrame = {}; }
	_uint Get_BoneIndex() const { return m_iBoneIndex; }
	void Capture_KeyFrame(const vector<class CBone*>& Bones);

private:
	// 이 뼈(채널)의 이름
	_char						m_szName[MAX_PATH] = {};
	// 키프레임 수
	_uint						m_iNumKeyFrames = {};
	// 키프레임을 들고있는 벡터
	vector<KEYFRAME>			m_KeyFrames;
	// 이 뼈의 인덱스 번호 정보
	_uint						m_iBoneIndex = {};
	// 보간할 이전 프레임
	KEYFRAME					m_BeforeKeyFrame = {};
	// 아직 미사용중
	_uint						m_iNumScaleFrameKeys = {};
	_uint						m_iNumRotationFrameKeys = {};
	_uint						m_iNumPositionFrameKeys = {};

	class CGameInstance*		m_pGameInstance = { nullptr };

public:
	static CChannel* Create(const class CModel* pModel);
	virtual void Free() override;
};