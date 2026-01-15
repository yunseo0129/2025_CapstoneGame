#pragma once
#include "stdafx.h"

class CBinary_Converter
{
public:
	CBinary_Converter();
	~CBinary_Converter();

public:
	_uint Get_NumMeshes() const {
		return m_iNumMeshes;
	}

	HRESULT Convert_to_Binary(MODEL_TYPE eModelType, const _char* pModelFilePath, const _tchar* pComponentTag);

	HRESULT Save_Data_NonAnim(const _tchar* pComponentTag);
	HRESULT Save_Data_Anim(const _tchar* pComponentTag);

private:
	string ConvertToString(wstring strName) {
		string ConvertName(strName.begin(), strName.end());
		return ConvertName;
	}

	wstring ConvertToWString(string strName) {
		wstring ConvertName(strName.begin(), strName.end());
		return ConvertName;
	}

private:
	Assimp::Importer			m_Importer;
	const aiScene* m_pAIScene = { nullptr };

	_uint m_iNumMeshes = 0;
	vector<class CBinSave_Mesh*>		m_Meshes;
	MODEL_INFO					m_tSaveInfo{};
	MODEL_INFO					m_tLoadInfo{};
	MODEL_TYPE					m_eType{ TYPE_END };
};