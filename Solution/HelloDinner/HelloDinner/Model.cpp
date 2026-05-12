#include "Model.h"
#include "Mesh.h"
#include "GameInstance.h"
#include "Texture.h"
#include "Material.h"
#include "Bone.h"
#include "Animation.h"

CModel::CModel(EngineContext* pContext)
	: CComponent{ pContext }
{
}

CModel::CModel(const CModel& Prototype)
	: CComponent{ Prototype.m_pContext }
	, m_eModelType{ Prototype.m_eModelType }
	, m_eMatLoadMode{ Prototype.m_eMatLoadMode }
	, m_iNumMeshes{ Prototype.m_iNumMeshes }
	, m_Meshes{ Prototype.m_Meshes }
	, m_iNumMaterials{ Prototype.m_iNumMaterials }
	, m_Materials{ Prototype.m_Materials }
	, m_PreTransformMatrix{ Prototype.m_PreTransformMatrix }
{
	// Bone은 인스턴스마다 애니메이션 상태가 다르므로 깊은 복사

	for (auto& pBone : Prototype.m_Bones)
		m_Bones.push_back(pBone->Clone());

	// Animation도 깊은 복사
	for (auto& pAnim : Prototype.m_Animations)
		m_Animations.push_back(pAnim->Clone());

	// 애님 모델이면 본 매트릭스 업로드 버퍼 생성
	if (TYPE_ANIM == m_eModelType)
		Create_BoneBuffer();
}

HRESULT CModel::Set_MaterialTexture(_uint iMaterialIndex, TextureType eType, CTexture* pTexture)
{
	if (iMaterialIndex >= m_iNumMaterials || nullptr == pTexture)
		return E_FAIL;

	return m_Materials[iMaterialIndex]->Add_Texture(eType, pTexture);
}

HRESULT CModel::Initialize_Prototype(TYPE eModelType, const wchar_t* pModelFilePath, _fmatrix PreTransformMatrix, MATERIAL_LOAD_MODE eMatMode)
{
	m_eModelType = eModelType;
	m_eMatLoadMode = eMatMode;

	XMStoreFloat4x4(&m_PreTransformMatrix, PreTransformMatrix);

	m_pGameInstance->Open_File(pModelFilePath);
	
	if (FAILED(Ready_Bones()))
		return E_FAIL;
	
	if (FAILED(Ready_Meshes()))
		return E_FAIL;

	if (FAILED(Ready_Materials(pModelFilePath, eMatMode)))
		return E_FAIL;

	if (FAILED(Ready_Animations()))
		return E_FAIL;

	m_pGameInstance->Close_File();

	return S_OK;
}

HRESULT CModel::Initialize(void* pArg)
{
	return S_OK;
}

HRESULT CModel::Render(ID3D12GraphicsCommandList* _commandList, _uint iMeshIndex, bool IsShadow)
{
	if (iMeshIndex >= m_iNumMeshes)
		return E_FAIL;
	if (!IsShadow)
	{
	// 이 메쉬가 참조하는 머티리얼의 Diffuse 텍스처 바인딩
	_uint iMaterialIndex = m_Meshes[iMeshIndex]->Get_MaterialIndex();

		if (iMaterialIndex < m_iNumMaterials)
		{
			// 텍스처가 실제로 있는지 확인
			CTexture* pTex = m_Materials[iMaterialIndex]->Get_Texture((TextureType)TextureType_DIFFUSE, 0);
			if (pTex == nullptr)
			{
				char szLog[256];
				sprintf_s(szLog, "[Model::Render] NO TEXTURE! MeshIdx=%u, MatIdx=%u, this=0x%p\n",
					iMeshIndex, iMaterialIndex, this);
				OutputDebugStringA(szLog);
			}
			else {
				Bind_Material(iMaterialIndex, (TextureType)TextureType_DIFFUSE, 0, _commandList);
			}

			CTexture* pTexNormal = m_Materials[iMaterialIndex]->Get_Texture((TextureType)TextureType_NORMALS, 0);
			if (pTexNormal == nullptr)
			{
			}
			else {
				Bind_Material(iMaterialIndex, (TextureType)TextureType_NORMALS, 0, _commandList);
			}
		}
	}
	// 2. 메쉬 렌더 (IASet + DrawIndexedInstanced)
	m_Meshes[iMeshIndex]->Render(_commandList);


	return S_OK;
}

HRESULT CModel::Create_BoneBuffer()
{
	_uint ncbElementBytes = ((sizeof(CB_BONE_MATRICES) + 255) & ~255);

	D3D12_HEAP_PROPERTIES d3dHeapPropertiesDesc;
	::ZeroMemory(&d3dHeapPropertiesDesc, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapPropertiesDesc.Type = D3D12_HEAP_TYPE_UPLOAD;
	d3dHeapPropertiesDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapPropertiesDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapPropertiesDesc.CreationNodeMask = 1;
	d3dHeapPropertiesDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC d3dResourceDesc;
	::ZeroMemory(&d3dResourceDesc, sizeof(D3D12_RESOURCE_DESC));
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	d3dResourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	d3dResourceDesc.Width = ncbElementBytes;
	d3dResourceDesc.Height = 1;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	d3dResourceDesc.SampleDesc.Count = 1;
	d3dResourceDesc.SampleDesc.Quality = 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	for (_int i = 0; i < FRAME_COUNT; ++i)
	{
		HRESULT hResult = m_pContext->device->CreateCommittedResource(
			&d3dHeapPropertiesDesc, D3D12_HEAP_FLAG_NONE,
			&d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
			NULL, __uuidof(ID3D12Resource), (void**)&m_pBoneBuffers[i]);

		if (FAILED(hResult))
			return E_FAIL;

		m_pBoneBuffers[i]->Map(0, NULL, (void**)&m_pCbMappedBones[i]);
	}

	return S_OK;
}

HRESULT CModel::Ready_Bones()
{
	if (m_eModelType == TYPE_NONANIM)
		return S_OK;

	size_t NumBone;
	m_pGameInstance->Read_File(NumBone);

	_char BoneName[MAX_PATH];
	_float4x4 BoneTransformMatrix;
	_float4x4 CombindTransformationMatrix;
	_int BoneParentIndex;

	for (size_t i = 0; i < NumBone; ++i)
	{
		m_pGameInstance->Read_File(BoneName);
		m_pGameInstance->Read_File(BoneTransformMatrix);
		m_pGameInstance->Read_File(CombindTransformationMatrix); // 추가함(활용안함) -> 애니메이션 재생 안할때 기본상태값 조정에 사용해야함
		m_pGameInstance->Read_File(BoneParentIndex);

		CBone* pBone = CBone::Create(BoneName, BoneTransformMatrix, BoneParentIndex, CombindTransformationMatrix);

		if (nullptr == pBone)
			return E_FAIL;


		m_Bones.push_back(pBone);
	}

	return S_OK;
}

HRESULT CModel::Ready_Animations()
{
	if (m_eModelType == TYPE_NONANIM)
		return S_OK;

	m_pGameInstance->Read_File(m_iNumAnimations);

	for (size_t i = 0; i < m_iNumAnimations; i++)
	{
		CAnimation* pAnimation = CAnimation::Create(this);
		if (nullptr == pAnimation)
			return E_FAIL;

		m_Animations.push_back(pAnimation);
	}

	return S_OK;
}

HRESULT CModel::Ready_Meshes()
{
	m_pGameInstance->Read_File(m_iNumMeshes);

	
	for (size_t i = 0; i < m_iNumMeshes; i++)
	{
		CMesh* pMesh = CMesh::Create(m_pContext, m_eModelType, this, XMLoadFloat4x4(&m_PreTransformMatrix));
		if (nullptr == pMesh)
			return E_FAIL;

		m_Meshes.push_back(pMesh);
	}
	

	return S_OK;
}

HRESULT CModel::Ready_Materials(const wchar_t* pModelFilePath, MATERIAL_LOAD_MODE eMatMode)
{
	m_pGameInstance->Read_File(m_iNumMaterials);

	m_Materials.resize(m_iNumMaterials);

	for (size_t i = 0; i < m_iNumMaterials; i++)
	{
		m_Materials[i] = CMaterial::Create();

		for (size_t j = 0; j < 25; j++)
		{
			// 각 슬롯마다 MAX_PATH바이트 경로를 읽음
			_char szPath[MAX_PATH] = "";
			m_pGameInstance->Read_File(szPath);

			// "Not_Data"이면 텍스처 없음 → 스킵
			if (strcmp(szPath, "Not_Data") == 0)
				continue;

			// 미리 dds 파일로 만들어 뒀다면 dds 파일로 읽어오기
			if (eMatMode == MATLOAD_DDS_FILE)
				continue;

			// char → wchar_t 변환
			_tchar szPerfectPath[MAX_PATH] = TEXT("");
			MultiByteToWideChar(CP_ACP, 0, szPath, -1, szPerfectPath, MAX_PATH);

			CTexture* pTexture = CTexture::Create(m_pContext, szPerfectPath);
			if (pTexture != nullptr)
			{
				m_Materials[i]->Add_Texture((TextureType)j, pTexture);
			}
		}
	}
	return S_OK;
}

HRESULT CModel::Bind_Material(_uint iMeshIndex, TextureType eType, _uint iTextureIndex, ID3D12GraphicsCommandList* _commandList)
{
	if (iMeshIndex >= m_iNumMaterials)
		return E_FAIL;

	// RootParameterIndex 추가 후 변경
	RootParameterIndex rootParameterIndex = RootParameterIndex::TEXTURE_Diffuse;

	if (eType == TextureType_NORMALS)
		rootParameterIndex = RootParameterIndex::TEXTURE_Normal;

	m_Materials[iMeshIndex]->Bind_ShaderResource(_commandList, eType, rootParameterIndex, iTextureIndex);
	return S_OK;
}

CModel* CModel::Create(EngineContext* pContext, TYPE eModelType, const wchar_t* pModelFilePath, _fmatrix PreTransformMatrix, MATERIAL_LOAD_MODE eMatMode)
{
	CModel* pInstance = new CModel(pContext);

	if (FAILED(pInstance->Initialize_Prototype(eModelType, pModelFilePath, PreTransformMatrix, eMatMode)))
	{
		MSG_BOX("Failed to Created : CModel");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CComponent* CModel::Clone(void* pArg)
{
	CModel* pInstance = new CModel(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CModel");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CModel::Free()
{

	for ( auto& pMesh : m_Meshes ) {
		if ( pMesh ) {
			Safe_Release ( pMesh );
		}
	}

	for (auto& pMaterial : m_Materials)
		Safe_Release(pMaterial);

	for (auto& pBone : m_Bones)
		Safe_Release(pBone);

	for (auto& pAnim : m_Animations)
		Safe_Release(pAnim);

	for (int i = 0; i < FRAME_COUNT; ++i) {

		if (m_pCbMappedBones[i] != nullptr) {
			m_pCbMappedBones[i] = nullptr;
		}
	}

	__super::Free();
}

void CModel::Off_Blend()
{
	m_isBlend = false;
	m_Animations[m_iCurrentAnimIndex]->Anim_Init();
}

_uint CModel::Get_BoneIndex(const _char* pBoneName) const
{
	_uint	iBoneIndex = { 0 };

	auto	iter = find_if(m_Bones.begin(), m_Bones.end(), [&](CBone* pBone)->_bool
		{
			if (false == strcmp(pBone->Get_Name(), pBoneName))
				return true;

			++iBoneIndex;

			return false;
		});

	if (iter == m_Bones.end())
		MSG_BOX("없어");


	return iBoneIndex;
}

void CModel::Change_Animation(_uint iAnimIndex, _float fLinearDurationTime, _bool isLoop)
{
	vector<_uint> before = m_Animations[m_iCurrentAnimIndex]->Get_VecBones();
	m_Animations[m_iCurrentAnimIndex]->Anim_Init();
	m_Animations[iAnimIndex]->Change_Anim(m_Bones, before);
	m_iCurrentAnimIndex = iAnimIndex;
	m_isLoop = isLoop;
	m_fBlendTime = fLinearDurationTime;
	m_isBlend = true;
}

_bool CModel::Play_Animation(_float fTimeDelta)
{
	/* 표현하고자 하는 애니메이션에 따른, 뼈의 상태(TransformationMatrix)를 갱신해준다.  */
	m_isFinished = m_Animations[m_iCurrentAnimIndex]->Update_TransformationMatrices(m_Bones, m_isLoop, fTimeDelta);

	/* 뼈의 바뀐 TransformationMatrix를 활용하여 렌덜이하기위한 최종 뼈의 CombinedTransformationMatrix를 만들어낸다. */
	for (auto& pBone : m_Bones)
	{
		pBone->Update_CombinedTransformationMatrix(m_Bones, XMLoadFloat4x4(&m_PreTransformMatrix));
	}

	return m_isFinished;
}

_bool CModel::Blend_Animation(_float fTimeDelta)
{
	/* 표현하고자 하는 애니메이션에 따른, 뼈의 상태(TransformationMatrix)를 갱신해준다.  */
	m_isFinished = m_Animations[m_iCurrentAnimIndex]->Update_TransformationMatrices(m_Bones, m_fBlendTime, fTimeDelta);

	/* 뼈의 바뀐 TransformationMatrix를 활용하여 렌덜이하기위한 최종 뼈의 CombinedTransformationMatrix를 만들어낸다. */
	for (auto& pBone : m_Bones)
	{
		pBone->Update_CombinedTransformationMatrix(m_Bones, XMLoadFloat4x4(&m_PreTransformMatrix));
	}

	return m_isFinished;
}

const _float4x4* CModel::Get_BoneMatrix(const _char* pBoneName) const
{
	auto   iter = find_if(m_Bones.begin(), m_Bones.end(), [&](CBone* pBone)->_bool
		{
			if (false == strcmp(pBone->Get_Name(), pBoneName))
				return true;

			return false;
		});

	if (iter == m_Bones.end())
		return nullptr;

	return (*iter)->Get_CombinedTransformationFloat4x4();
}

HRESULT CModel::Bind_BoneMatrices(ID3D12GraphicsCommandList* _cmdList, _uint iMeshIndex)
{
	if (iMeshIndex >= m_iNumMeshes)
		return E_FAIL;

	_int iFrameIndex = m_pGameInstance->GetCurrentFrameIndex();

	// 1. 메쉬의 본 매트릭스 계산 (OffsetMatrix * CombinedMatrix) → 매핑된 버퍼에 직접 기록
	m_Meshes[iMeshIndex]->SetUp_BoneMatrices(m_Bones, m_pCbMappedBones[iFrameIndex]->m_BoneMatrices);

	// 2. CBV 바인딩
	_cmdList->SetGraphicsRootConstantBufferView(
		RootParameterIndex::BoneMatrix,
		m_pBoneBuffers[iFrameIndex]->GetGPUVirtualAddress());

	return S_OK;
}

HRESULT CModel::Ready_MapMaterial(const wchar_t* pModelFilePath, int _nMaterial, TextureType _eType )
{
	CTexture* pTexture = CTexture::Create(m_pContext, pModelFilePath);
	if (pTexture != nullptr)
	{
		m_Materials[_nMaterial]->Add_Texture(_eType, pTexture);
	}
	else {
		return E_FAIL;
	}
	return S_OK;
}
