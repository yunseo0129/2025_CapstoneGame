#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <winnt.h>
#include <winerror.h>
#include <filesystem>
#include <DirectXMath.h>

#include "assimp\scene.h"
#include "assimp\Importer.hpp"
#include "assimp\postprocess.h"


using namespace std;
using namespace DirectX;

enum MODEL_TYPE { TYPE_NONANIM, TYPE_ANIM, TYPE_END };

typedef	struct
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
}MESH_INFO;

typedef struct
{
	char		mTexPath[256];
}MAX_CHAR;

typedef	struct
{
	vector<MAX_CHAR>		mTexture[25];	//fbx의 텍스쳐 정보
}TEXTURE_INFO;

typedef struct NODE_INFO
{
	char					mName[256];
	XMFLOAT4X4				mTransformation{};
	XMFLOAT4X4				mCombindTransformationMatrix{};
	int						miParentBoneIndex{ -1 };
	unsigned int			miNumChildren{};
	vector<NODE_INFO>		mChildren;
};

typedef struct
{
	XMFLOAT3		vScale;
	XMFLOAT4		vRotation;
	XMFLOAT3		vPosition;
	float			fKeyFramePosition;
}KEYFRAME;

typedef struct
{
	_char					mszName[256];
	_uint					miNumScaleFrameKeys;
	_uint					miNumRotationFrameKeys;
	_uint					miNumPositionFrameKeys;
	_uint					miNumKeyFrame{};
	vector<KEYFRAME>		mKeyFrame;
	_uint					miBoneIndex{};

}CHANNEL_INFO;

typedef struct
{
	_char					mszName[256];
	_uint					miNumChannels{};
	vector<CHANNEL_INFO>	mChannels;
	_float					mDuration{};
	_float					mfTickPerSecond{};

}ANIMATION_INFO;

typedef struct
{
	unsigned int				mMeshCnt;		//메쉬의 개수
	vector<MESH_INFO>			mMeshes;		//메쉬의 정보

	unsigned int				mNumMaterials;	//fbx의 메테리얼 개수
	vector<TEXTURE_INFO>		mvecTexture;	//텍스쳐의 정보
	NODE_INFO					mRootNode;
	vector<NODE_INFO>			mNodes;

	_uint						miNumAnimation{};
	vector<ANIMATION_INFO>		mAnim;
}MODEL_INFO;

typedef		bool						_bool;

typedef		signed char					_byte;
typedef		unsigned char				_ubyte;
typedef		char						_char;


typedef		wchar_t						_tchar;
typedef		wstring						_wstring;

typedef		signed short				_short;
typedef		unsigned short				_ushort;

typedef		signed int					_int;
typedef		unsigned int				_uint;

typedef		signed long					_long;
typedef		unsigned long				_ulong;

typedef		float						_float;
typedef		double						_double;

typedef		XMUINT2						_uint2;
typedef		XMFLOAT2					_float2;
typedef		XMFLOAT3					_float3;
typedef		XMFLOAT4					_float4;
typedef		XMVECTOR					_vector;
typedef		FXMVECTOR					_fvector;
typedef		GXMVECTOR					_gvector;
typedef		HXMVECTOR					_hvector;
typedef		CXMVECTOR					_cvector;

typedef		XMFLOAT4X4					_float4x4;
typedef		XMMATRIX					_matrix;
typedef		FXMMATRIX					_fmatrix;