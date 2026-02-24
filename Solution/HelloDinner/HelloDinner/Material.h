#pragma once
#include "Base.h"
#include "Texture.h" // 앞서 만드신 CTexture 헤더 포함

class CMaterial : public CBase
{
private:
	CMaterial(ID3D12Device* pDevice);
	CMaterial(const CMaterial& Prototype);
	virtual ~CMaterial() = default;

public:
	HRESULT Add_Texture(aiTextureType eType, class CTexture* pTexture);

	class CTexture* Get_Texture(aiTextureType eType, _uint iTextureIndex = 0);

	HRESULT Bind_ShaderResource(ID3D12GraphicsCommandList* pCmdList, aiTextureType eType, RootParameterIndex iRootParameterIndex, _uint iTextureIndex = 0);

private:
	vector<vector<class CTexture*>> m_Textures;

	ID3D12Device* m_pDevice = { nullptr };

public:
	static CMaterial* Create(ID3D12Device* pDevice);
	virtual CMaterial* Clone();
	virtual void Free() override;
};