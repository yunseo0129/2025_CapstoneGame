#include "CBinary_Converter.h"

CBinary_Converter::CBinary_Converter()
{
}

CBinary_Converter::~CBinary_Converter()
{
}

HRESULT CBinary_Converter::Convert_to_Binary(MODEL_TYPE eModelType, const _char* pModelFilePath, const _tchar* pComponentTag)
{
	_uint		iFlag = { aiProcess_ConvertToLeftHanded | aiProcessPreset_TargetRealtime_Fast };

	m_eType = eModelType;

	if (m_eType == TYPE_NONANIM)
		iFlag |= aiProcess_PreTransformVertices;

	m_pAIScene = m_Importer.ReadFile(pModelFilePath, iFlag);

	if (0 == m_pAIScene)
	{
		cout << "파일 읽기 실패" << endl;
		return E_FAIL;
	}
	
	// 애니메이션 있는 모델의 경우 뼈도 저장해야함
	if (m_eType == TYPE_ANIM)
	{
		if (FAILED(Ready_Bone(m_pAIScene->mRootNode, -1)))
		{
			cout << "뼈 정보 저장 실패" << endl;
			return E_FAIL;
		}
	}

	if (FAILED(Ready_Meshes()))
	{
		cout << "메쉬 정보 저장 실패" << endl;
		return E_FAIL;
	}

	if (FAILED(Ready_Materials(pModelFilePath, true)))
	{
		cout << "텍스쳐 정보 저장 실패" << endl;
		return E_FAIL;
	}

	if (m_eType == TYPE_ANIM)
	{
		if (FAILED(Ready_Animation()))
		{
			cout << "애니메이션 정보 저장 실패" << endl;
			return E_FAIL;
		}
	}

	std::filesystem::path filePath = pModelFilePath;
	string strDirectory = filePath.parent_path().string();
	wstring comName = ConvertToWString(strDirectory) + L"/" + pComponentTag + L".txt";

	HRESULT hr = S_OK;

	if (m_eType == TYPE_ANIM)
		hr = Save_Data_Anim(comName.c_str());
	else if (m_eType == TYPE_NONANIM)
		hr = Save_Data_NonAnim(comName.c_str());
	
	if (FAILED(hr))
	{
		cout << "파일 쓰기 실패" << endl;
		return E_FAIL;
	}

	m_tSaveInfo = {};

	return S_OK;
}

HRESULT CBinary_Converter::Save_Data_NonAnim(const _tchar* pComponentTag)
{
	return S_OK;
}

HRESULT CBinary_Converter::Save_Data_Anim(const _tchar* pComponentTag)
{
	return S_OK;
}

HRESULT CBinary_Converter::Ready_Bone(const aiNode* pAINode, _int iParentBoneIndex)
{
	return S_OK;
}

HRESULT CBinary_Converter::Ready_Meshes()
{
	return S_OK;
}

HRESULT CBinary_Converter::Ready_Materials(const _char* pModelFilePath, _bool jys)
{
	return S_OK;
}

HRESULT CBinary_Converter::Ready_Animation()
{
	return S_OK;
}
