#pragma once
#include "UIObject.h"

/*
    CUI_Texture
    --------------------------------------------------------------------
    텍스처 한 장을 화면에 그리는 가장 기본적인 UI 패널.
    (배경 패널, 아이콘, 스코어보드 배경 등 범용으로 사용)

    사용:
      CUI_Texture::UI_TEXTURE_DESC desc;
      desc.fX = 100; desc.fY = 100; desc.fSizeX = 300; desc.fSizeY = 200;
      desc.strTextureProtoTag = L"Prototype_Component_Texture_PanelBG";
      desc.iTextureLevelIndex = LEVEL_GAMEPLAY;
      Add_GameObject_ToLayer(..., L"Prototype_GameObject_UI_Texture", ..., &desc);
*/
class CUI_Texture final: public CUIObject
{
public:
    struct UI_TEXTURE_DESC: public CUIObject::UIOBJECT_DESC
    {
        _wstring strTextureProtoTag = L"";   // 사용할 텍스처 프로토타입 태그
        _uint    iTextureLevelIndex = 0;     // 그 텍스처가 등록된 레벨 인덱스
    };

private:
    CUI_Texture(EngineContext* _pContext);
    CUI_Texture(const CUI_Texture& Prototype);
    virtual ~CUI_Texture() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* pArg) override;
    virtual void    Render(ID3D12GraphicsCommandList* _commandList) override;

private:
    HRESULT Ready_Components(const _wstring& strTextureProtoTag, _uint iTextureLevelIndex);

private:
    class CVIBuffer* m_pVIBufferCom = nullptr;
    class CTexture* m_pTextureCom = nullptr;

    _wstring m_strTextureProtoTag = L"";
    _uint    m_iTextureLevelIndex = 0;

public:
    static CUI_Texture* Create(EngineContext* _pContext);
    virtual CGameObject* Clone(void* pArg) override;
    virtual void Free() override;
};