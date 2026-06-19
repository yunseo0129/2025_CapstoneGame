#pragma once
#include "UIObject.h"

/*
    CUI_Text
    --------------------------------------------------------------------
    한 줄 텍스트를 그리는 UI 오브젝트.

    - 패널(CUI_Panel)과 달리 VIBuffer/PSO 를 쓰지 않고,
      CFont_Manager(SpriteBatch)를 통해 그린다.
    - Render() 는 Font_Manager::Draw_Text 만 호출한다.
      (SpriteBatch Begin/End 는 Renderer 의 텍스트 패스가 감싼다)
    - 따라서 Late_Update 에서 RG_UI 가 아니라 "RG_TEXT" 큐에 등록한다.

    좌표: CUIObject 의 (m_fX, m_fY) = 텍스트 위치(픽셀).
          크기(m_fW,m_fH)는 텍스트엔 의미 없지만 베이스 호환으로 유지.

    사용:
      CUI_Text::UI_TEXT_DESC d;
      d.fX = 200; d.fY = 60;
      d.strText = L"ROUND 6/10";
      d.strFontTag = L"Font_Default";
      d.vColor = _float4(1,1,1,1);
      d.fTextScale = 1.0f;
      d.bCentered = true;
      Add_GameObject_ToLayer(LEVEL_STATIC, L"Prototype_GameObject_UI_Text", LV, layer, &d);
*/
class CUI_Text final: public CUIObject
{
public:
    struct UI_TEXT_DESC: public CUIObject::UIOBJECT_DESC
    {
        _wstring strText = L"";
        _wstring strFontTag = L"Font_Default";
        _float   fTextScale = 1.f;
        _bool    bCentered = false;   // true 면 (fX,fY)가 텍스트 중심
    };

private:
    CUI_Text(EngineContext* _pContext);
    CUI_Text(const CUI_Text& Prototype);
    virtual ~CUI_Text() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* pArg) override;
    virtual void    Late_Update(_float fTimeDelta) override;   // RG_TEXT 등록
    virtual void    Render(ID3D12GraphicsCommandList* _commandList) override;

public:
    // 런타임에 내용 변경 (스코어/KDA 갱신용)
    void Set_Text(const _wstring& strText) { m_strText = strText; }
    const _wstring& Get_Text() const { return m_strText; }

private:
    _wstring m_strText = L"";
    _wstring m_strFontTag = L"Font_Default";
    _float   m_fTextScale = 1.f;
    _bool    m_bCentered = false;

public:
    static CUI_Text* Create(EngineContext* _pContext);
    virtual CGameObject* Clone(void* pArg) override;
    virtual void Free() override;
};