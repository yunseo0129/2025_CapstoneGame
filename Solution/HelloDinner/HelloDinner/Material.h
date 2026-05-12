#pragma once
#include "Base.h"
#include "Texture.h" // 앞서 만드신 CTexture 헤더 포함

class CMaterial : public CBase
{
private:
	CMaterial();
	CMaterial(const CMaterial& Prototype);
	virtual ~CMaterial() = default;

public:
	HRESULT Add_Texture(TextureType eType, class CTexture* pTexture);

	class CTexture* Get_Texture(TextureType eType, _uint iTextureIndex = 0);

	HRESULT Bind_ShaderResource(ID3D12GraphicsCommandList* pCmdList, TextureType eType, RootParameterIndex iRootParameterIndex, _uint iTextureIndex = 0);

private:
	vector<vector<class CTexture*>> m_Textures;

public:
	static CMaterial* Create();
	virtual CMaterial* Clone();
	virtual void Free() override;
};