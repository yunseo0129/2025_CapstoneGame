#pragma once
#include "UIObject.h"

/*
    CUI_Panel
    --------------------------------------------------------------------
    텍스처 없이 "단색 사각형"만 그리는 UI 패널.
    (스코어보드 배경/행, 상점 오버레이/슬롯 등 레이아웃 블록용)

    - 색상/알파는 UIOBJECT_DESC::vColor 로 지정.
    - 텍스처 리소스가 필요 없으므로 즉시 렌더링 확인 가능.
    - 프로토타입은 VIBuffer_Rect 만 사용.

    사용:
      CUI_Panel::UI_PANEL_DESC desc;
      desc.fX = 40; desc.fY = 40; desc.fSizeX = 360; desc.fSizeY = 220;
      desc.vColor = _float4(0.1f, 0.12f, 0.16f, 0.85f); // 반투명 어두운 패널
      desc.fDepth = 0.6f;
      Add_GameObject_ToLayer(LEVEL_STATIC, L"Prototype_GameObject_UI_Panel",
          LEVEL_GAMEPLAY, L"Layer_UI", &desc);
*/
class CUI_Panel final: public CUIObject
{
public:
    struct UI_PANEL_DESC: public CUIObject::UIOBJECT_DESC
    {
        // 추가 필드 없음 (색상/위치/크기는 베이스 DESC 사용)
    };

private:
    CUI_Panel(EngineContext* _pContext);
    CUI_Panel(const CUI_Panel& Prototype);
    virtual ~CUI_Panel() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* pArg) override;
    virtual void    Render(ID3D12GraphicsCommandList* _commandList) override;

private:
    HRESULT Ready_Components();

private:
    class CVIBuffer* m_pVIBufferCom = nullptr;

public:
    static CUI_Panel* Create(EngineContext* _pContext);
    virtual CGameObject* Clone(void* pArg) override;
    virtual void Free() override;
};