#include "Channel.h"
#include "Model.h"
#include "Bone.h"
#include "GameInstance.h"

CChannel::CChannel() : m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);
}

HRESULT CChannel::Initialize(const CModel* pModel)
{
	m_pGameInstance->Read_File(m_szName);
	m_iBoneIndex = pModel->Get_BoneIndex(m_szName);

	// 미사용 데이터임
	m_pGameInstance->Read_File(m_iNumScaleFrameKeys);
	m_pGameInstance->Read_File(m_iNumRotationFrameKeys);
	m_pGameInstance->Read_File(m_iNumPositionFrameKeys);

	m_pGameInstance->Read_File(m_iNumKeyFrames);
	m_pGameInstance->Read_File(m_iBoneIndex);

	_float3			vScale, vPosition;
	_float4			vRotation;

	for (size_t i = 0; i < m_iNumKeyFrames; i++)
	{
		KEYFRAME		KeyFrame = {};

		m_pGameInstance->Read_File(KeyFrame);

		m_KeyFrames.push_back(KeyFrame);
	}

	return S_OK;
}

void CChannel::Update_TransformationMatrix(_float fCurrentTrackPosition, _uint* pKeyFrameIndex, const vector<class CBone*>& Bones)
{
	if (0.f == fCurrentTrackPosition)
		*pKeyFrameIndex = 0;

	KEYFRAME		LastKeyFrame = m_KeyFrames.back();

	_float3			vScale;
	_float4			vRotation;
	_float3			vPosition;

	if (fCurrentTrackPosition >= LastKeyFrame.fKeyFramePosition)
	{
		vScale = LastKeyFrame.vScale;
		vRotation = LastKeyFrame.vRotation;
		vPosition = LastKeyFrame.vPosition;
	}

	else
	{
		while (fCurrentTrackPosition >= m_KeyFrames[*pKeyFrameIndex + 1].fKeyFramePosition)
			++*pKeyFrameIndex;

		_float			fRatio = (fCurrentTrackPosition - m_KeyFrames[*pKeyFrameIndex].fKeyFramePosition) /
			(m_KeyFrames[*pKeyFrameIndex + 1].fKeyFramePosition - m_KeyFrames[*pKeyFrameIndex].fKeyFramePosition);


		_float3			vLeftScale, vRightScale;
		_float4			vLeftRotation, vRightRotation;
		_float3			vLeftPosition, vRightPosition;

		vLeftScale = m_KeyFrames[*pKeyFrameIndex].vScale;
		vLeftRotation = m_KeyFrames[*pKeyFrameIndex].vRotation;
		vLeftPosition = m_KeyFrames[*pKeyFrameIndex].vPosition;

		vRightScale = m_KeyFrames[*pKeyFrameIndex + 1].vScale;
		vRightRotation = m_KeyFrames[*pKeyFrameIndex + 1].vRotation;
		vRightPosition = m_KeyFrames[*pKeyFrameIndex + 1].vPosition;

		XMStoreFloat3(&vScale, XMVectorLerp(XMLoadFloat3(&vLeftScale), XMLoadFloat3(&vRightScale), fRatio));
		XMStoreFloat4(&vRotation, XMQuaternionSlerp(XMLoadFloat4(&vLeftRotation), XMLoadFloat4(&vRightRotation), fRatio));
		XMStoreFloat3(&vPosition, XMVectorLerp(XMLoadFloat3(&vLeftPosition), XMLoadFloat3(&vRightPosition), fRatio));
	}

	Bones[m_iBoneIndex]->Set_TransformationMatrix(XMMatrixAffineTransformation(XMLoadFloat3(&vScale), XMVectorSet(0.f, 0.f, 0.f, 1.f), XMLoadFloat4(&vRotation), XMVectorSetW(XMLoadFloat3(&vPosition), 1.f)));
}

void CChannel::Update_TransformationMatrix(_float fCurrentTrackPosition, _float fBlendTime, const vector<class CBone*>& Bones)
{
	_float			fRatio = (fCurrentTrackPosition / fBlendTime);

	_float3			vScale;
	_float4			vRotation;
	_float3			vPosition;

	_float3			vLeftScale, vRightScale;
	_float4			vLeftRotation, vRightRotation;
	_float3			vLeftPosition, vRightPosition;

	vLeftScale = m_BeforeKeyFrame.vScale;
	vLeftRotation = m_BeforeKeyFrame.vRotation;
	vLeftPosition = m_BeforeKeyFrame.vPosition;

	vRightScale = m_KeyFrames[0].vScale;
	vRightRotation = m_KeyFrames[0].vRotation;
	vRightPosition = m_KeyFrames[0].vPosition;

	XMStoreFloat3(&vScale, XMVectorLerp(XMLoadFloat3(&vLeftScale), XMLoadFloat3(&vRightScale), fRatio));
	XMStoreFloat4(&vRotation, XMQuaternionSlerp(XMLoadFloat4(&vLeftRotation), XMLoadFloat4(&vRightRotation), fRatio));
	XMStoreFloat3(&vPosition, XMVectorLerp(XMLoadFloat3(&vLeftPosition), XMLoadFloat3(&vRightPosition), fRatio));

	Bones[m_iBoneIndex]->Set_TransformationMatrix(XMMatrixAffineTransformation(XMLoadFloat3(&vScale), XMVectorSet(0.f, 0.f, 0.f, 1.f), XMLoadFloat4(&vRotation), XMVectorSetW(XMLoadFloat3(&vPosition), 1.f)));
}

void CChannel::Capture_KeyFrame(const vector<class CBone*>& Bones)
{
	_matrix TransformMat = Bones[m_iBoneIndex]->Get_TransformationMatrix();
	_vector Scale, Rotate, Position;
	XMMatrixDecompose(&Scale, &Rotate, &Position, TransformMat);
	XMStoreFloat3(&m_BeforeKeyFrame.vScale, Scale);
	XMStoreFloat4(&m_BeforeKeyFrame.vRotation, Rotate);
	XMStoreFloat3(&m_BeforeKeyFrame.vPosition, Position);
	m_BeforeKeyFrame.fKeyFramePosition = 0;
}

CChannel* CChannel::Create(const CModel* pModel)
{
	CChannel* pInstance = new CChannel();

	if (FAILED(pInstance->Initialize(pModel)))
	{
		MSG_BOX("Failed to Created : CChannel");
		Safe_Release(pInstance);
	}
	return pInstance;
}
void CChannel::Free()
{
	__super::Free();
	Safe_Release(m_pGameInstance);
}
