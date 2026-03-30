#pragma once
#include "Base.h"
#include "Graphic_Device.h"

class CLight final : public CBase
{
public:
	struct LIGHT_DESC
	{
		enum TYPE { TYPE_POINT, TYPE_DIRECTIONAL, TYPE_END };

		TYPE			eType;
		XMFLOAT4		vDirection;
		XMFLOAT4		vPosition;
		float			fRange;

		XMFLOAT4		vDiffuse;
		XMFLOAT4		vAmbient;
		XMFLOAT4		vSpecular;
	};

private:
	CLight(EngineContext* pContext);
	virtual ~CLight() = default;

public:
	HRESULT Initialize(const LIGHT_DESC& LightDesc);

	void Bind_LightBuffer(ID3D12GraphicsCommandList* pCmdList, RootParameterIndex _eIndex);

private:
	HRESULT Create_LightBuffer();

private:
	EngineContext* m_pContext = { nullptr };
	class CGameInstance* m_pGameInstance = { nullptr };

	static const _int FRAME_COUNT = CGraphic_Device::SWAP_CHAIN_BUFFER_COUNT;
	ComPtr<ID3D12Resource> m_pLightbuffer[FRAME_COUNT];
	CB_LIGHT*			m_pCbMappedLight[FRAME_COUNT]{};

	LIGHT_DESC m_LightDesc;

public:
	static CLight* Create(EngineContext* pContext, const LIGHT_DESC& LightDesc);
	virtual void Free() override;
};