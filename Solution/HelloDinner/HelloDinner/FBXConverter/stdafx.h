#pragma once

#define NOMINMAX
#include "Mytypedef.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <Windows.h>
#include <winnt.h>
#include <winerror.h>
#include <filesystem>

#include "assimp\scene.h"
#include "assimp\Importer.hpp"
#include "assimp\postprocess.h"
#include <assimp/cimport.h>

using namespace std;

typedef		wstring						_wstring;

enum MODEL_TYPE { TYPE_NONANIM, TYPE_ANIM, TYPE_END };


template<typename T>
void	Safe_Delete_Array(T& Pointer)
{
	if (nullptr != Pointer)
	{
		delete[] Pointer;
		Pointer = nullptr;
	}
}

struct MESH_INFO
{
	unsigned int			mMeshMaterials;	//메쉬에 적용된 메테리얼 개수
	char					szMeshName[256];		//메쉬의 이름
	unsigned int			mNumVtxCnt;		//메쉬의 정점개수
	unsigned int			mNumTriCnt;		//메쉬의 삼각형 개수

	vector<XMFLOAT3>		mvPosition;	//메쉬의 정점의 위치
	vector<XMFLOAT3>		mvNormal;	//메쉬의 정점의 노말
	vector<XMFLOAT2>		mvTexCoord;	//메쉬의 정점의 텍스쿠드
	vector<XMFLOAT3>		mvTangent;	//메쉬의 정점의 탄젠트 스페이스
	vector<XMUINT4>			mvBlendIndices;
	vector<XMFLOAT4>		mvBlendWeights;

	vector<unsigned int	>		mvcecIdx;	//메쉬의 인덱스 정보

	_uint						miNumBones{};
	vector<_uint>				mBoneIndices;
	vector<XMFLOAT4X4>			mOffsetMatrix;
};

struct MAX_CHAR
{
	char		mTexPath[256];
};

struct TEXTURE_INFO
{
	vector<MAX_CHAR>		mTexture[25];	//fbx의 텍스쳐 정보
};

struct NODE_INFO
{
	char					mName[256];
	XMFLOAT4X4				mTransformation{};
	XMFLOAT4X4				mCombindTransformationMatrix{};
	int						miParentBoneIndex{ -1 };
	unsigned int			miNumChildren{};
	vector<NODE_INFO>		mChildren;
};

struct KEYFRAME
{
	XMFLOAT3		vScale;
	XMFLOAT4		vRotation;
	XMFLOAT3		vPosition;
	float			fKeyFramePosition;
};

struct CHANNEL_INFO
{
	_char					mszName[256];
	_uint					miNumScaleFrameKeys;
	_uint					miNumRotationFrameKeys;
	_uint					miNumPositionFrameKeys;
	_uint					miNumKeyFrame{};
	vector<KEYFRAME>		mKeyFrame;
	_uint					miBoneIndex{};

};

struct ANIMATION_INFO
{
	_char					mszName[256];
	_uint					miNumChannels{};
	vector<CHANNEL_INFO>	mChannels;
	_float					mDuration{};
	_float					mfTickPerSecond{};

};

struct MODEL_INFO
{
	unsigned int				mMeshCnt;		//메쉬의 개수
	vector<MESH_INFO>			mMeshes;		//메쉬의 정보

	unsigned int				mNumMaterials;	//fbx의 메테리얼 개수
	vector<TEXTURE_INFO>		mvecTexture;	//텍스쳐의 정보
	NODE_INFO					mRootNode;
	vector<NODE_INFO>			mNodes;

	_uint						miNumAnimation{};
	vector<ANIMATION_INFO>		mAnim;
};