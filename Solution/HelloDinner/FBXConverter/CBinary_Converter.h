#pragma once
#include "stdafx.h"

class CBinary_Converter
{
public:
	CBinary_Converter();
	~CBinary_Converter();

public:
	enum PATH_STATE { DIRECTORY, FILEPATH, FILENAME, COMPONETTAG };

public:
	HRESULT			Convert(MODEL_TYPE eType, const _tchar* _path);

private:
	HRESULT			Find_Directories(const _tchar* _path, vector<const _tchar*>& Directories);
	HRESULT			ModifyFileSuffixAndCheckExistence(const _char* szFullPath, const _char* newSuffix, _char* outFullPath);
	_char*			GetFileExtension(_char* file_name);

	HRESULT			Convert_to_Binary(MODEL_TYPE eModelType, const _char* pModelFilePath, const _tchar* pComponentTag);

	HRESULT			Save_Data_NonAnim(const _tchar* pComponentTag);
	HRESULT			Save_Data_Anim(const _tchar* pComponentTag);

	HRESULT			Ready_Bone(const aiNode* pAINode, _int iParentBoneIndex);
	_uint			Get_BoneIndex(const _char* pBoneName);

	HRESULT			Ready_Meshes();
	HRESULT			Save_Mesh_Info(const aiMesh* pAIMesh);
	HRESULT			Save_Mesh_Info_Anim(const aiMesh* pAIMesh);

	HRESULT			Ready_Materials(const _char* pModelFilePath, _bool jys = false);

	HRESULT			Ready_Animation();
	ANIMATION_INFO	Create_Animation(const aiAnimation* pAIAnimation);
	CHANNEL_INFO	Create_Channel(const aiNodeAnim* pAIChannel);
	NODE_INFO		Create_Node(const aiNode* pAINode, _int iParentBoneIndex);

private:
	string			ConvertToString(wstring strName) {
		string ConvertName(strName.begin(), strName.end());
		return ConvertName;
	}

	wstring			ConvertToWString(string strName) {
		wstring ConvertName(strName.begin(), strName.end());
		return ConvertName;
	}

private:
	Assimp::Importer				m_Importer;

	vector<class CBinSave_Mesh*>	m_Meshes;
	MODEL_INFO						m_tSaveInfo{};
	MODEL_INFO						m_tLoadInfo{};
	MODEL_TYPE						m_eType{ TYPE_END };
	_uint							m_iNumMeshes = 0;

	const aiScene*					m_pAIScene = { nullptr };
};