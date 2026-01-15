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
		cout << "FBX파일 읽기 실패" << endl;
		return E_FAIL;
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
		cout << "저장 실패" << endl;
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
