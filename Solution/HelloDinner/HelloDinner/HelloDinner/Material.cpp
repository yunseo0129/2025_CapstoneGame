#include "Material.h"

CMaterial::CMaterial()
{
}

CMaterial::CMaterial(const CMaterial& Prototype)
	: m_Textures(Prototype.m_Textures)
{
}

HRESULT CMaterial::Add_Texture(TextureType eType, CTexture* pTexture)
{
	if (pTexture == nullptr)
		return E_FAIL;

	if (eType >= m_Textures.size())
		return E_FAIL;

	m_Textures[eType].push_back(pTexture);

	return S_OK;
}

CTexture* CMaterial::Get_Texture(TextureType eType, _uint iTextureIndex)
{
	if (eType >= m_Textures.size())
		return nullptr;

	if (iTextureIndex >= m_Textures[eType].size())
		return nullptr;

	return m_Textures[eType][iTextureIndex];
}

HRESULT CMaterial::Bind_ShaderResource(ID3D12GraphicsCommandList* pCmdList, TextureType eType, RootParameterIndex iRootParameterIndex, _uint iTextureIndex)
{
	CTexture* pTexture = Get_Texture(eType, iTextureIndex);
	if (pTexture == nullptr)
		return E_FAIL;

	// CTexture 클래스의 바인딩 함수 호출
	return pTexture->Bind_ShaderResource(pCmdList, iRootParameterIndex, iTextureIndex);
}

CMaterial* CMaterial::Create()
{
	CMaterial* pInstance = new CMaterial();

	// 초기화 (벡터 사이즈 할당)
	pInstance->m_Textures.resize(AI_TEXTURE_TYPE_MAX);

	return pInstance;
}

CMaterial* CMaterial::Clone()
{
	return new CMaterial(*this);
}

void CMaterial::Free()
{
	// 들고 있던 텍스처 메모리 해제
	for (auto& TextureList : m_Textures)
	{
		for (auto& pTexture : TextureList)
		{
			Safe_Release(pTexture);
		}
	}
	__super::Free();
}