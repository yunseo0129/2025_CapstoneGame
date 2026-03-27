#include "Animation.h"
#include "Channel.h"
#include "GameInstance.h"


CAnimation::CAnimation() : m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);
}

CAnimation::CAnimation(const CAnimation& Prototype)
	: m_iNumChannels{ Prototype.m_iNumChannels }
	, m_Channels{ Prototype.m_Channels }
	, m_fDuration{ Prototype.m_fDuration }
	, m_fTickPerSecond{ Prototype.m_fTickPerSecond }
	, m_fCurrentTrackPosition{ Prototype.m_fCurrentTrackPosition }
	, m_CurrentKeyFrameIndices{ Prototype.m_CurrentKeyFrameIndices }
	, m_pGameInstance{ Prototype.m_pGameInstance }
{
	Safe_AddRef(m_pGameInstance);

	for (auto& pChannel : m_Channels)
		Safe_AddRef(pChannel);

	strcpy_s(m_szName, Prototype.m_szName);
}

HRESULT CAnimation::Initialize(const CModel* pModel)
{
	m_pGameInstance->Read_File(m_szName);
	m_pGameInstance->Read_File(m_fDuration);
	m_pGameInstance->Read_File(m_fTickPerSecond);
	m_pGameInstance->Read_File(m_iNumChannels);

	m_CurrentKeyFrameIndices.resize(m_iNumChannels);

	for (size_t i = 0; i < m_iNumChannels; i++)
	{
		CChannel* pChannel = CChannel::Create(pModel);
		if (nullptr == pChannel)
			return E_FAIL;

		m_Channels.push_back(pChannel);
	}
	return S_OK;
}

_bool CAnimation::Update_TransformationMatrices(const vector<class CBone*>& Bones, _bool isLoop, _float fTimeDelta)
{
	m_fCurrentTrackPosition += m_fTickPerSecond * fTimeDelta;

	if (true == isLoop && m_fCurrentTrackPosition >= m_fDuration)
	{
		m_fCurrentTrackPosition = 0.f;
	}

	else if (m_fCurrentTrackPosition >= m_fDuration)
		return true;

	for (size_t i = 0; i < m_iNumChannels; i++)
	{
		m_Channels[i]->Update_TransformationMatrix(m_fCurrentTrackPosition, &m_CurrentKeyFrameIndices[i], Bones);
	}

	return false;
}

_bool CAnimation::Update_TransformationMatrices(const vector<class CBone*>& Bones, _float fBlendTime, _float fTimeDelta)
{
	m_fCurrentTrackPosition += m_fTickPerSecond * fTimeDelta;

	if (m_fCurrentTrackPosition >= fBlendTime)
		return true;

	for (size_t i = 0; i < m_iNumChannels; i++)
	{
		m_Channels[i]->Update_TransformationMatrix(m_fCurrentTrackPosition, fBlendTime, Bones);
	}

	return false;
}

void CAnimation::Change_Anim(const vector<class CBone*>& Bones, vector<_uint> before)
{
	for (size_t i = 0; i < before.size(); ++i)
	{
		for (size_t j = 0; j < m_Channels.size(); ++j)
		{
			if (before[i] == m_Channels[j]->Get_BoneIndex())
			{
				m_Channels[i]->Capture_KeyFrame(Bones);
			}
		}
	}
}

void CAnimation::Anim_Init()
{
	m_fCurrentTrackPosition = 0;
	for (size_t i = 0; i < m_iNumChannels; i++)
	{
		m_CurrentKeyFrameIndices[i] = 0;
		m_Channels[i]->Channel_Init();
	}
}

vector<_uint> CAnimation::Get_VecBones()
{
	vector<_uint> result = {};
	for (_uint i = 0; i < m_iNumChannels; ++i)
	{
		result.push_back(m_Channels[i]->Get_BoneIndex());
	}
	return result;
}

CAnimation* CAnimation::Create(const CModel* pModel)
{
	CAnimation* pInstance = new CAnimation();

	if (FAILED(pInstance->Initialize(pModel)))
	{
		MSG_BOX("Failed to Created : CAnimation");
		Safe_Release(pInstance);
	}
	return pInstance;
}

CAnimation* CAnimation::Clone()
{
	return new CAnimation(*this);
}

void CAnimation::Free()
{
	__super::Free();

	for (auto& pChannel : m_Channels)
		Safe_Release(pChannel);

	m_Channels.clear();
	Safe_Release(m_pGameInstance);
}

