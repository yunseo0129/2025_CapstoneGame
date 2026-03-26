#include "CBinary_Converter.h"

CBinary_Converter::CBinary_Converter()
{
}

CBinary_Converter::~CBinary_Converter()
{
	for (auto p : m_Meshes)
		delete p; 
	m_Meshes.clear();

	m_Importer.FreeScene();
}

HRESULT CBinary_Converter::Convert(MODEL_TYPE eType, const _tchar* _path)
{
	vector<const _tchar*> Directories;
	Find_Directories(_path, Directories);

	for (_uint i = 0; i < Directories.size(); ++i)
	{
		WIN32_FIND_DATAA	FileFinde;

		_tchar	path[256] = L"";
		_tchar	pathFile[256] = L"";
		wcscpy_s(path, 256, Directories[i]);
		wcscpy_s(pathFile, 256, Directories[i]);
		wcscat_s(pathFile, 256, L"*.*");

		wstring strPath = pathFile;

		HANDLE hFind = ::FindFirstFileA(ConvertToString(strPath).c_str(), &FileFinde);
		if (hFind == INVALID_HANDLE_VALUE)
		{
			cout << "경로 오류" << endl;
			return E_FAIL;
		}

		do
		{
			if (!(FileFinde.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				_tchar pPath[256] = L"";
				wcscpy_s(pPath, 256, path);
				string strFileName = FileFinde.cFileName;

				wcscat_s(pPath, 256, ConvertToWString(strFileName).c_str());
				_char temp[256] = "";

				size_t convertedSize;
				errno_t err = wcstombs_s(&convertedSize, temp, sizeof(temp), pPath, _TRUNCATE);
				if (err != 0)
				{
					cout << "wcstombs_s 실패" << endl;
					return E_FAIL;
				}
				
				if (!strcmp(".fbx", GetFileExtension(temp)) || !strcmp(".FBX", GetFileExtension(temp)))
				{
					_tchar szFilePath[256] = L"";
					_tchar* szFileName = new _tchar[256];
					wcscpy_s(szFilePath, pPath);
					err = _wsplitpath_s(szFilePath, nullptr, 0, nullptr, 0, szFileName, 256, nullptr, 0);
					if (err != 0)
					{
						cout << "_wsplitpath_s 실패" << endl;
						return E_FAIL;
					}
					const _wstring strFilePath = pPath;
					_tchar strProtoTag[256] = L"Prototype_Component_";
					wcscat_s(strProtoTag, 256, szFileName);
					Convert_to_Binary(eType, ConvertToString(strFilePath).c_str(), strProtoTag);
					Safe_Delete_Array(szFileName);
				}
			}
			else if (FileFinde.cFileName[0] != '.')
			{
				string strFileName = FileFinde.cFileName;
				_tchar path2[256] = L"";
				wcscpy_s(path2, 256, path);
				wcscat_s(path2, 256, ConvertToWString(strFileName).c_str());
				wcscat_s(path2, 256, L"/");

				Convert(eType, path2);
			}

		} while (::FindNextFileA(hFind, &FileFinde));
		::FindClose(hFind);
	}

	return S_OK;
}

HRESULT CBinary_Converter::Find_Directories(const _tchar* _path, vector<const _tchar*>& Directories)
{
	WIN32_FIND_DATAA	FileFind;

	_tchar	path[256] = L"";
	wcscpy_s(path, 256, _path);

	_tchar	pathFile[256] = L"";
	wcscpy_s(pathFile, 256, path);
	wcscat_s(pathFile, 256, L"*.*");

	wstring strPath = pathFile;

	HANDLE hFind = ::FindFirstFileA(ConvertToString(strPath).c_str(), &FileFind);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do 
		{
			if ((FileFind.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				if (FileFind.cFileName[0] != '.')
				{
					_tchar* pPath = new _tchar[256];
					wcscpy_s(pPath, 256, path);
					string strFileName = FileFind.cFileName;
					wcscat_s(pPath, 256, ConvertToWString(strFileName).c_str());
					wcscat_s(pPath, 256, L"/");
					Directories.push_back(pPath);
					Find_Directories(pPath, Directories);
				}
			}

		} while (::FindNextFileA(hFind, &FileFind));
		::FindClose(hFind);
	}

	return S_OK;
}

_char* CBinary_Converter::GetFileExtension(_char* file_name)
{
	std::filesystem::path p(file_name);
	std::string ext = p.extension().string();

	_char* result = new _char[ext.size() + 1];
	strcpy_s(result, ext.size() + 1, ext.c_str());
	return result;
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
	_tchar FilePath[256] = L"../";
	_tchar Ext[256] = L".txt";

	wcscat_s(FilePath, pComponentTag);
	wcscat_s(FilePath, Ext);
	ofstream FileStream(pComponentTag, ios_base::out | ios_base::binary);

	if (!FileStream.is_open())
		return E_FAIL;

	FileStream.write((char*)&m_tSaveInfo.mMeshCnt, sizeof(_uint));

	for (MESH_INFO Mesh : m_tSaveInfo.mMeshes)
	{
		FileStream.write((char*)&Mesh.mMeshMaterials, sizeof(_uint));
		FileStream.write((char*)&Mesh.szMeshName, sizeof(_char) * MAX_PATH);
		FileStream.write((char*)&Mesh.mNumVtxCnt, sizeof(_uint));
		FileStream.write((char*)&Mesh.mNumTriCnt, sizeof(_uint));

		for (_float3 Pos : Mesh.mvPosition)
		{
			FileStream.write((char*)&Pos, sizeof(_float3));
		}

		for (_float3 Nor : Mesh.mvNormal)
		{
			FileStream.write((char*)&Nor, sizeof(_float3));
		}

		for (_float2 Tex : Mesh.mvTexCoord)
		{
			FileStream.write((char*)&Tex, sizeof(_float2));
		}

		for (_float3 Tan : Mesh.mvTangent)
		{
			FileStream.write((char*)&Tan, sizeof(_float3));
		}

		for (_uint Idx : Mesh.mvcecIdx)
		{
			FileStream.write((char*)&Idx, sizeof(_uint));
		}
	}

	FileStream.write((char*)&m_tSaveInfo.mNumMaterials, sizeof(_uint));

	for (TEXTURE_INFO Tex : m_tSaveInfo.mvecTexture)
	{
		for (_uint i = 0; i < 25; ++i)
		{
			if (Tex.mTexture[i].size() == 0)
			{
				_char FullPath[256] = "Not_Data";
				FileStream.write((char*)&FullPath, sizeof(_char) * MAX_PATH);
			}
			else
			{
				for (MAX_CHAR Path : Tex.mTexture[i])
				{
					/*_char* FullPath = ;
					strcpy_s(FullPath, 256, Path);*/
					FileStream.write((char*)&Path.mTexPath, sizeof(_char) * MAX_PATH);
				}
			}
		}
	}

	FileStream.close();
	return S_OK;
}

HRESULT CBinary_Converter::Save_Data_Anim(const _tchar* pComponentTag)
{
	_tchar FilePath[256] = L"../";
	_tchar Ext[256] = L".txt";

	wcscat_s(FilePath, pComponentTag);
	wcscat_s(FilePath, Ext);
	ofstream FileStream(pComponentTag, ios_base::out | ios_base::binary);
	
	if (!FileStream.is_open())
		return E_FAIL;

	// ------------------------뼈 정보------------------------

	_uint NodeSize = m_tSaveInfo.mNodes.size();
	FileStream.write((char*)&NodeSize, sizeof(_int));

	for (NODE_INFO node : m_tSaveInfo.mNodes)
	{
		FileStream.write((char*)&node.mName, MAX_PATH);
		FileStream.write((char*)&node.mTransformation, sizeof(_float4x4));
		FileStream.write((char*)&node.mCombindTransformationMatrix, sizeof(_float4x4));
		FileStream.write((char*)&node.miParentBoneIndex, sizeof(_int));
	}

	//--------------------------------메쉬정보-----------------------------------------
	//Save_Bones(m_tSaveInfo.mRootNode, m_tSaveInfo.mRootNode.miParentBoneIndex, FileStream);

	FileStream.write((char*)&m_tSaveInfo.mMeshCnt, sizeof(_uint));

	for (MESH_INFO Mesh : m_tSaveInfo.mMeshes)
	{
		FileStream.write((char*)&Mesh.mMeshMaterials, sizeof(_uint));
		FileStream.write((char*)&Mesh.szMeshName, sizeof(_char) * MAX_PATH);
		FileStream.write((char*)&Mesh.mNumVtxCnt, sizeof(_uint));
		FileStream.write((char*)&Mesh.mNumTriCnt, sizeof(_uint));

		FileStream.write((char*)&Mesh.miNumBones, sizeof(_uint));


		for (_float4x4 offset : Mesh.mOffsetMatrix)
		{
			FileStream.write((char*)&offset, sizeof(_float4x4));
		}

		for (_uint idx : Mesh.mBoneIndices)
		{
			FileStream.write((char*)&idx, sizeof(_uint));
		}


		for (_float3 Pos : Mesh.mvPosition)
		{
			FileStream.write((char*)&Pos, sizeof(_float3));
		}

		for (_float3 Nor : Mesh.mvNormal)
		{
			FileStream.write((char*)&Nor, sizeof(_float3));
		}

		for (_float2 Tex : Mesh.mvTexCoord)
		{
			FileStream.write((char*)&Tex, sizeof(_float2));
		}

		for (_float3 Tan : Mesh.mvTangent)
		{
			FileStream.write((char*)&Tan, sizeof(_float3));
		}

		for (_uint Idx : Mesh.mvcecIdx)
		{
			FileStream.write((char*)&Idx, sizeof(_uint));
		}

		for (XMUINT4 BlendIdx : Mesh.mvBlendIndices)
		{
			FileStream.write((char*)&BlendIdx, sizeof(XMUINT4));
		}

		for (_float4 Weights : Mesh.mvBlendWeights)
		{
			FileStream.write((char*)&Weights, sizeof(_float4));
		}
	}
	//---------------------------메테리얼 정보 -----------------------------
	FileStream.write((char*)&m_tSaveInfo.mNumMaterials, sizeof(_uint));

	for (TEXTURE_INFO Tex : m_tSaveInfo.mvecTexture)
	{
		for (_uint i = 0; i < 25; ++i)
		{
			if (Tex.mTexture[i].size() == 0)
			{
				_char FullPath[256] = "Not_Data";
				FileStream.write((char*)&FullPath, sizeof(_char) * MAX_PATH);
			}
			else
			{
				for (MAX_CHAR Path : Tex.mTexture[i])
				{
					/*_char* FullPath = ;
					strcpy_s(FullPath, 256, Path);*/
					FileStream.write((char*)&Path.mTexPath, sizeof(_char) * MAX_PATH);
				}
			}
		}
	}
	//----------------------------애니메이션 정보--------------------------------

	FileStream.write((char*)&m_tSaveInfo.miNumAnimation, sizeof(_uint));

	for (ANIMATION_INFO Anim : m_tSaveInfo.mAnim)
	{
		FileStream.write((char*)&Anim.mszName, sizeof(_char) * MAX_PATH);
		FileStream.write((char*)&Anim.miNumChannels, sizeof(_uint));
		FileStream.write((char*)&Anim.mDuration, sizeof(_float));
		FileStream.write((char*)&Anim.mfTickPerSecond, sizeof(_float));

		for (CHANNEL_INFO Channel : Anim.mChannels)
		{
			FileStream.write((char*)&Channel.mszName, sizeof(_char) * MAX_PATH);
			FileStream.write((char*)&Channel.miNumScaleFrameKeys, sizeof(_uint));
			FileStream.write((char*)&Channel.miNumRotationFrameKeys, sizeof(_uint));
			FileStream.write((char*)&Channel.miNumPositionFrameKeys, sizeof(_uint));
			FileStream.write((char*)&Channel.miNumKeyFrame, sizeof(_uint));
			FileStream.write((char*)&Channel.miBoneIndex, sizeof(_uint));

			for (KEYFRAME KeyFrame : Channel.mKeyFrame)
			{
				FileStream.write((char*)&KeyFrame.vScale, sizeof(XMFLOAT3));
				FileStream.write((char*)&KeyFrame.vRotation, sizeof(XMFLOAT4));
				FileStream.write((char*)&KeyFrame.vPosition, sizeof(XMFLOAT3));
				FileStream.write((char*)&KeyFrame.fKeyFramePosition, sizeof(_float));
			}
		}
	}

	FileStream.close();
	return S_OK;
}

HRESULT CBinary_Converter::Save_Mesh_Info(const aiMesh* pAIMesh)
{
	MESH_INFO	tMesh{};
	tMesh.mMeshMaterials = pAIMesh->mMaterialIndex;
	//m_iMaterialIndex -> 각 메쉬가 가지고 있는 메테리얼 개수
	strcpy_s(tMesh.szMeshName, pAIMesh->mName.data);

	tMesh.mNumVtxCnt = pAIMesh->mNumVertices;
	//m_iNumVertices -> 각 메쉬가 가지고 있는 버텍스 개수

	tMesh.mNumTriCnt = pAIMesh->mNumFaces;
	//m_iNumIndices -> 각 메쉬가 가지고 있는 인덱스 개수(pAIMesh->mNumFaces * 3)

	for (size_t i = 0; i < tMesh.mNumVtxCnt; i++)
	{
		_float3 fPos, fNor, fTan{};
		_float2 fTex{};
		memcpy(&fPos, &(pAIMesh->mVertices[i]), sizeof(_float3));
		tMesh.mvPosition.push_back(fPos);
		memcpy(&fNor, &(pAIMesh->mNormals[i]), sizeof(_float3));
		tMesh.mvNormal.push_back(fNor);
		memcpy(&fTex, &pAIMesh->mTextureCoords[0][i], sizeof(_float2));
		tMesh.mvTexCoord.push_back(fTex);
		memcpy(&fTan, &pAIMesh->mTangents[i], sizeof(_float3));
		tMesh.mvTangent.push_back(fTan);
	}

	for (size_t i = 0; i < tMesh.mNumTriCnt; i++)
	{
		tMesh.mvcecIdx.push_back(pAIMesh->mFaces[i].mIndices[0]);
		tMesh.mvcecIdx.push_back(pAIMesh->mFaces[i].mIndices[1]);
		tMesh.mvcecIdx.push_back(pAIMesh->mFaces[i].mIndices[2]);
	}

	m_tSaveInfo.mMeshes.push_back(tMesh);

	return S_OK;
}

HRESULT CBinary_Converter::Save_Mesh_Info_Anim(const aiMesh* pAIMesh)
{
	MESH_INFO	tMesh{};
	tMesh.mMeshMaterials = pAIMesh->mMaterialIndex;
	//m_iMaterialIndex -> 각 메쉬가 가지고 있는 메테리얼 개수
	strcpy_s(tMesh.szMeshName, pAIMesh->mName.data);

	tMesh.mNumVtxCnt = pAIMesh->mNumVertices;
	//m_iNumVertices -> 각 메쉬가 가지고 있는 버텍스 개수

	tMesh.mNumTriCnt = pAIMesh->mNumFaces;
	//m_iNumIndices -> 각 메쉬가 가지고 있는 인덱스 개수(pAIMesh->mNumFaces * 3)

	for (size_t i = 0; i < tMesh.mNumVtxCnt; i++)
	{
		_float3 fPos, fNor, fTan{};
		_float2 fTex{};
		memcpy(&fPos, &(pAIMesh->mVertices[i]), sizeof(_float3));
		tMesh.mvPosition.push_back(fPos);
		memcpy(&fNor, &(pAIMesh->mNormals[i]), sizeof(_float3));
		tMesh.mvNormal.push_back(fNor);
		memcpy(&fTex, &pAIMesh->mTextureCoords[0][i], sizeof(_float2));
		tMesh.mvTexCoord.push_back(fTex);
		memcpy(&fTan, &pAIMesh->mTangents[i], sizeof(_float3));
		tMesh.mvTangent.push_back(fTan);
	}

	tMesh.mvBlendIndices.resize(tMesh.mNumVtxCnt);
	tMesh.mvBlendWeights.resize(tMesh.mNumVtxCnt);

	tMesh.miNumBones = pAIMesh->mNumBones;

	for (size_t i = 0; i < tMesh.miNumBones; i++)
	{
		aiBone* pAIBone = pAIMesh->mBones[i];
		_float4x4	OffsetMatrix = {};
		memcpy(&OffsetMatrix, &pAIBone->mOffsetMatrix, sizeof(_float4x4));
		XMStoreFloat4x4(&OffsetMatrix, XMMatrixTranspose(XMLoadFloat4x4(&OffsetMatrix)));

		tMesh.mOffsetMatrix.push_back(OffsetMatrix);

		tMesh.mBoneIndices.push_back(Get_BoneIndex(pAIBone->mName.data));

		for (size_t j = 0; j < pAIBone->mNumWeights; j++)
		{
			if (0 == tMesh.mvBlendWeights[pAIBone->mWeights[j].mVertexId].x)
			{
				tMesh.mvBlendIndices[pAIBone->mWeights[j].mVertexId].x = i;
				tMesh.mvBlendWeights[pAIBone->mWeights[j].mVertexId].x = pAIBone->mWeights[j].mWeight;
			}
			else if (0 == tMesh.mvBlendWeights[pAIBone->mWeights[j].mVertexId].y)
			{
				tMesh.mvBlendIndices[pAIBone->mWeights[j].mVertexId].y = i;
				tMesh.mvBlendWeights[pAIBone->mWeights[j].mVertexId].y = pAIBone->mWeights[j].mWeight;
			}
			else if (0 == tMesh.mvBlendWeights[pAIBone->mWeights[j].mVertexId].z)
			{
				tMesh.mvBlendIndices[pAIBone->mWeights[j].mVertexId].z = i;
				tMesh.mvBlendWeights[pAIBone->mWeights[j].mVertexId].z = pAIBone->mWeights[j].mWeight;
			}
			else if (0 == tMesh.mvBlendWeights[pAIBone->mWeights[j].mVertexId].w)
			{
				tMesh.mvBlendIndices[pAIBone->mWeights[j].mVertexId].w = i;
				tMesh.mvBlendWeights[pAIBone->mWeights[j].mVertexId].w = pAIBone->mWeights[j].mWeight;
			}
		}
	}

	if (0 == tMesh.miNumBones)
	{
		tMesh.miNumBones = 1;

		tMesh.mBoneIndices.push_back(Get_BoneIndex(tMesh.szMeshName));

		_float4x4		OffsetMatrix;
		XMStoreFloat4x4(&OffsetMatrix, XMMatrixIdentity());

		tMesh.mOffsetMatrix.push_back(OffsetMatrix);
	}

	for (size_t i = 0; i < tMesh.mNumTriCnt; i++)
	{
		tMesh.mvcecIdx.push_back(pAIMesh->mFaces[i].mIndices[0]);
		tMesh.mvcecIdx.push_back(pAIMesh->mFaces[i].mIndices[1]);
		tMesh.mvcecIdx.push_back(pAIMesh->mFaces[i].mIndices[2]);
	}

	m_tSaveInfo.mMeshes.push_back(tMesh);

	return S_OK;
}

_uint CBinary_Converter::Get_BoneIndex(const _char* pBoneName)
{
	_uint	iBoneIndex = { 0 };

	auto	iter = find_if(m_tSaveInfo.mNodes.begin(), m_tSaveInfo.mNodes.end(), [&](NODE_INFO tNode)->_bool
		{
			if (false == strcmp(tNode.mName, pBoneName))
				return true;

			++iBoneIndex;

			return false;
		});

	return iBoneIndex;
}

HRESULT CBinary_Converter::ModifyFileSuffixAndCheckExistence(const _char* szFullPath, const _char* newSuffix, _char* outFullPath)
{
	_char szDrive[MAX_PATH] = "";
	_char szDirectory[MAX_PATH] = "";
	_char szFileName[MAX_PATH] = "";
	_char szBaseFileName[MAX_PATH] = "";

	// 경로 분리
	_splitpath_s(szFullPath, szDrive, MAX_PATH, szDirectory, MAX_PATH, szFileName, MAX_PATH, nullptr, 0);

	// 파일명에서 뒤쪽 접미사 제거 (예: filename_Alb에서 filename만 추출)
	_char* underscorePos = strrchr(szFileName, '_'); // 마지막 '_' 위치 찾기
	if (underscorePos)
	{
		// '_' 이전까지만 복사
		size_t prefixLength = underscorePos - szFileName;
		strncpy_s(szBaseFileName, MAX_PATH, szFileName, prefixLength);
	}
	else
	{
		// '_'가 없으면
		return E_FAIL;
	}

	// 새로운 파일명으로 경로 재조합
	strcat_s(szBaseFileName, MAX_PATH, newSuffix);

	// 새로운 전체 경로 생성
	_makepath_s(outFullPath, MAX_PATH, szDrive, szDirectory, szBaseFileName, nullptr);

	// 파일 존재 여부 확인
	if (GetFileAttributesA(outFullPath) != INVALID_FILE_ATTRIBUTES)
	{
		return S_OK; // 파일이 존재함
	}
	else
	{
		return E_FAIL; // 파일이 존재하지 않음
	}
}

ANIMATION_INFO CBinary_Converter::Create_Animation(const aiAnimation* pAIAnimation)
{
	ANIMATION_INFO anim{};
	strcpy_s(anim.mszName, pAIAnimation->mName.data);

	anim.mDuration = pAIAnimation->mDuration;
	anim.mfTickPerSecond = pAIAnimation->mTicksPerSecond;

	anim.miNumChannels = pAIAnimation->mNumChannels;

	for (size_t i = 0; i < anim.miNumChannels; i++)
	{
		anim.mChannels.push_back(Create_Channel(pAIAnimation->mChannels[i]));
	}

	return anim;
}

CHANNEL_INFO CBinary_Converter::Create_Channel(const aiNodeAnim* pAIChannel)
{
	CHANNEL_INFO tChannel;
	//여기서 가져오는 이름은 모델에서 저장한 전체뼈의 들어간 이름과 같음(m_Bonse에 저장되어 있는 어떤 뼈와 이름이 같다는 뜻 
	// -> 이 함수에서 찾은 뼈와 m_Bones에 이름이 같은 뼈의 이름은 동기화 되어 있다)
	strcpy_s(tChannel.mszName, pAIChannel->mNodeName.data);

	//m_Bones에 몇번째 들어있는지를 구해옴
	tChannel.miBoneIndex = Get_BoneIndex(tChannel.mszName);

	tChannel.miNumScaleFrameKeys = pAIChannel->mNumScalingKeys;
	tChannel.miNumRotationFrameKeys = pAIChannel->mNumRotationKeys;
	tChannel.miNumPositionFrameKeys = pAIChannel->mNumPositionKeys;

	//3가지(Scale, Rotation ,Position) 키프레임의 개수를 구해와서 가장 많은 키프레임 개수를 저장
	tChannel.miNumKeyFrame = max(pAIChannel->mNumScalingKeys, pAIChannel->mNumRotationKeys);
	tChannel.miNumKeyFrame = max(tChannel.miNumKeyFrame, pAIChannel->mNumPositionKeys);


	_float3			vScale, vPosition;
	_float4			vRotation;

	//가장 큰 값을 가지는 키프레임 개수만큼 순회
	for (size_t i = 0; i < tChannel.miNumKeyFrame; i++)
	{
		KEYFRAME		KeyFrame = {};

		if (i < pAIChannel->mNumScalingKeys)
		{
			//Scale의 키프레임을 저장
			memcpy(&vScale, &pAIChannel->mScalingKeys[i].mValue, sizeof(_float3));
			KeyFrame.fKeyFramePosition = pAIChannel->mScalingKeys[i].mTime;
		}
		if (i < pAIChannel->mNumRotationKeys)
		{
			//Rotation의 키프레임을 저장(4원소 형태로 반환하는 값이 w x z y 순이므로 순서를 재정렬하기 위해 직접 대입해줌)
			vRotation.x = pAIChannel->mRotationKeys[i].mValue.x;
			vRotation.y = pAIChannel->mRotationKeys[i].mValue.y;
			vRotation.z = pAIChannel->mRotationKeys[i].mValue.z;
			vRotation.w = pAIChannel->mRotationKeys[i].mValue.w;
			KeyFrame.fKeyFramePosition = pAIChannel->mRotationKeys[i].mTime;
		}
		if (i < pAIChannel->mNumPositionKeys)
		{
			//Position의 키프레임을 저장
			memcpy(&vPosition, &pAIChannel->mPositionKeys[i].mValue, sizeof(_float3));
			KeyFrame.fKeyFramePosition = pAIChannel->mPositionKeys[i].mTime;
		}

		KeyFrame.vScale = vScale;
		KeyFrame.vRotation = vRotation;
		KeyFrame.vPosition = vPosition;

		tChannel.mKeyFrame.push_back(KeyFrame);
	}

	return tChannel;
}

NODE_INFO CBinary_Converter::Create_Node(const aiNode* pAINode, _int iParentBoneIndex)
{
	NODE_INFO	 tNode{};
	strcpy_s(tNode.mName, pAINode->mName.data);

	memcpy(&tNode.mTransformation, &pAINode->mTransformation, sizeof(_float4x4));

	XMStoreFloat4x4(&tNode.mCombindTransformationMatrix, XMMatrixIdentity());

	tNode.miParentBoneIndex = iParentBoneIndex;

	return tNode;
}

HRESULT CBinary_Converter::Ready_Bone(const aiNode* pAINode, _int iParentBoneIndex)
{
	NODE_INFO	tNode{};
	m_tSaveInfo.mNodes.push_back(Create_Node(pAINode, iParentBoneIndex));

	iParentBoneIndex = m_tSaveInfo.mNodes.size() - 1;

	for (size_t i = 0; i < pAINode->mNumChildren; ++i)
		Ready_Bone(pAINode->mChildren[i], iParentBoneIndex);

	return S_OK;
}

HRESULT CBinary_Converter::Ready_Meshes()
{
	m_tSaveInfo.mMeshCnt = m_pAIScene->mNumMeshes;

	for (size_t i = 0; i < m_tSaveInfo.mMeshCnt; i++)
	{
		if (m_eType == TYPE_NONANIM)
		{
			if (FAILED(Save_Mesh_Info(m_pAIScene->mMeshes[i])))
				return E_FAIL;
		}
		else if (m_eType == TYPE_ANIM)
		{
			if (FAILED(Save_Mesh_Info_Anim(m_pAIScene->mMeshes[i])))
				return E_FAIL;
		}
	}

	return S_OK;
}

HRESULT CBinary_Converter::Ready_Materials(const _char* pModelFilePath, _bool jys)
{
	m_tSaveInfo.mNumMaterials = m_pAIScene->mNumMaterials;

	if (jys == true)
	{
		for (size_t i = 0; i < m_tSaveInfo.mNumMaterials; i++)
		{
			aiMaterial* pAiMaterial = m_pAIScene->mMaterials[i];

			TEXTURE_INFO	tTexture{};
			_uint		iNumTextures = pAiMaterial->GetTextureCount(aiTextureType(1));

			aiString		strTexturePath;

			if (FAILED(pAiMaterial->GetTexture(aiTextureType(1), 0, &strTexturePath)))
				continue;


			_char		szDrive[MAX_PATH] = "";
			_char		szDirectory[MAX_PATH] = "";
			_char		szFileName[MAX_PATH] = "";
			_char		szExt[MAX_PATH] = "";

			_char		szFullPath[256] = "";

			_splitpath_s(pModelFilePath, szDrive, MAX_PATH, szDirectory, MAX_PATH, nullptr, 0, nullptr, 0);
			_splitpath_s(strTexturePath.data, nullptr, 0, nullptr, 0, szFileName, MAX_PATH, szExt, MAX_PATH);

			strcpy_s(szFullPath, szDrive);
			strcat_s(szFullPath, szDirectory);
			strcat_s(szFullPath, szFileName);
			strcat_s(szFullPath, szExt);

			_tchar		szPerfectPath[MAX_PATH] = TEXT("");

			MultiByteToWideChar(CP_ACP, 0, szFullPath, strlen(szFullPath), szPerfectPath, MAX_PATH);
			MAX_CHAR pPath{};
			strcpy_s(pPath.mTexPath, 256, szFullPath);
			tTexture.mTexture[1].push_back(pPath);

			// _Spm 2
			{
				const _char textype[] = "_Spm.png";
				_char typePath[MAX_PATH] = "";
				if (!FAILED(ModifyFileSuffixAndCheckExistence(szFullPath, textype, typePath)))
				{
					MAX_CHAR pPath{};
					strcpy_s(pPath.mTexPath, 256, typePath);
					tTexture.mTexture[2].push_back(pPath);
				}
			}
			// _Emm 4
			{
				const _char textype[] = "_Emm.png";
				_char typePath[MAX_PATH] = "";
				if (!FAILED(ModifyFileSuffixAndCheckExistence(szFullPath, textype, typePath)))
				{
					MAX_CHAR pPath{};
					strcpy_s(pPath.mTexPath, 256, typePath);
					tTexture.mTexture[4].push_back(pPath);
				}
			}
			// _Nrm 6
			{
				const _char textype[] = "_Nrm.png";
				_char typePath[MAX_PATH] = "";
				if (!FAILED(ModifyFileSuffixAndCheckExistence(szFullPath, textype, typePath)))
				{
					MAX_CHAR pPath{};
					strcpy_s(pPath.mTexPath, 256, typePath);
					tTexture.mTexture[6].push_back(pPath);
				}
			}
			// _Msk 8
			{
				const _char textype[] = "_Msk.png";
				_char typePath[MAX_PATH] = "";
				if (!FAILED(ModifyFileSuffixAndCheckExistence(szFullPath, textype, typePath)))
				{
					MAX_CHAR pPath{};
					strcpy_s(pPath.mTexPath, 256, typePath);
					tTexture.mTexture[8].push_back(pPath);
				}
			}
			// _Trs 9
			{
				const _char textype[] = "_Trs.png";
				_char typePath[MAX_PATH] = "";
				if (!FAILED(ModifyFileSuffixAndCheckExistence(szFullPath, textype, typePath)))
				{
					MAX_CHAR pPath{};
					strcpy_s(pPath.mTexPath, 256, typePath);
					tTexture.mTexture[9].push_back(pPath);
				}
			}
			// _Mtl 15
			{
				const _char textype[] = "_Mtl.png";
				_char typePath[MAX_PATH] = "";
				if (!FAILED(ModifyFileSuffixAndCheckExistence(szFullPath, textype, typePath)))
				{
					MAX_CHAR pPath{};
					strcpy_s(pPath.mTexPath, 256, typePath);
					tTexture.mTexture[15].push_back(pPath);
				}
			}
			// _ao  17
			{
				const _char textype[] = "_Ao.png";
				_char typePath[MAX_PATH] = "";
				if (!FAILED(ModifyFileSuffixAndCheckExistence(szFullPath, textype, typePath)))
				{
					MAX_CHAR pPath{};
					strcpy_s(pPath.mTexPath, 256, typePath);
					tTexture.mTexture[17].push_back(pPath);
				}
			}

			m_tSaveInfo.mvecTexture.push_back(tTexture);
		}
	}
	else
	{
		for (size_t i = 0; i < m_tSaveInfo.mNumMaterials; i++)
		{
			aiMaterial* pAiMaterial = m_pAIScene->mMaterials[i];

			TEXTURE_INFO	tTexture{};
			for (size_t j = 1; j < AI_TEXTURE_TYPE_MAX; j++)
			{
				_uint		iNumTextures = pAiMaterial->GetTextureCount(aiTextureType(j));

				for (size_t k = 0; k < iNumTextures; k++)
				{
					aiString		strTexturePath;

					if (FAILED(pAiMaterial->GetTexture(aiTextureType(j), k, &strTexturePath)))
						break;


					_char		szDrive[MAX_PATH] = "";
					_char		szDirectory[MAX_PATH] = "";
					_char		szFileName[MAX_PATH] = "";
					_char		szExt[MAX_PATH] = "";

					_char		szFullPath[256] = "";

					_splitpath_s(pModelFilePath, szDrive, MAX_PATH, szDirectory, MAX_PATH, nullptr, 0, nullptr, 0);
					_splitpath_s(strTexturePath.data, nullptr, 0, nullptr, 0, szFileName, MAX_PATH, szExt, MAX_PATH);

					strcpy_s(szFullPath, szDrive);
					strcat_s(szFullPath, szDirectory);
					strcat_s(szFullPath, szFileName);
					strcat_s(szFullPath, szExt);

					_tchar		szPerfectPath[MAX_PATH] = TEXT("");

					MultiByteToWideChar(CP_ACP, 0, szFullPath, strlen(szFullPath), szPerfectPath, MAX_PATH);
					MAX_CHAR pPath{};
					strcpy_s(pPath.mTexPath, 256, szFullPath);
					tTexture.mTexture[j].push_back(pPath);
				}
			}
			m_tSaveInfo.mvecTexture.push_back(tTexture);
		}
	}
	return S_OK;
}

HRESULT CBinary_Converter::Ready_Animation()
{
	m_tSaveInfo.miNumAnimation = m_pAIScene->mNumAnimations;

	for (size_t i = 0; i < m_tSaveInfo.miNumAnimation; i++)
	{

		//CAnimation* pAnimation = CAnimation::Create(m_pAIScene->mAnimations[i], this);

		m_tSaveInfo.mAnim.push_back(Create_Animation(m_pAIScene->mAnimations[i]));
	}

	return S_OK;
}
