#pragma once

#include "Base.h"

/* 1. 이 애니메이션(대기, 공격 etc)이 컨트롤해야하는 뼈(Channel)의 정보를 가진다.  */
/* 2. */
class CAnimation final : public CBase
{
private:
	CAnimation();
	CAnimation(const CAnimation& Prototype);
	virtual ~CAnimation() = default;

public:
	HRESULT Initialize(const class CModel* pModel);
	// 매 프레임 호출함
	// 
	// 컨트롤 해야 하는 뼈들을 업데이트 해줌  
	_bool Update_TransformationMatrices(const vector<class CBone*>& Bones, _bool isLoop, _float fTimeDelta);

	_bool Update_TransformationMatrices(const vector<class CBone*>& Bones, _float fBlendTime, _float fTimeDelta);

	void  Change_Anim(const vector<class CBone*>& Bones, vector<_uint> before);

	void Anim_Init();
	vector<_uint> Get_VecBones();
	vector<class CChannel*> Get_Channels() { return m_Channels; }

private:
	// 애니메이션 이름
	_char					m_szName[MAX_PATH] = {};
	// 컨트롤해야 하는 채널의 수
	_uint					m_iNumChannels = {};
	// 컨트롤 해야 하는 채널 백터
	vector<class CChannel*> m_Channels;

	// 이 애니메이션의 재생시간
	_float					m_fDuration = {};
	// 뭔지 몰겠는데 일단 fbx에서 받아와야하는건 맞음
	_float					m_fTickPerSecond = {};
	// 현재 재생구간
	_float					m_fCurrentTrackPosition = {};
	// 채널 별 현재 진행하고 있는 키프레임인거같음
	vector<_uint>			m_CurrentKeyFrameIndices;

	class CGameInstance* m_pGameInstance = { nullptr };

public:
	static CAnimation* Create(const class CModel* pModel);
	virtual CAnimation* Clone();
	virtual void Free() override;
};