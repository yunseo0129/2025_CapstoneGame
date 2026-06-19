#include "UI_Text.h"
#include "GameInstance.h"
#include "Font_Manager.h"

CUI_Text::CUI_Text(EngineContext* _pContext)
    : CUIObject(_pContext)
{
}

CUI_Text::CUI_Text(const CUI_Text& Prototype)
    : CUIObject(Prototype)
    , m_strText(Prototype.m_strText)
    , m_strFontTag(Prototype.m_strFontTag)
    , m_fTextScale(Prototype.m_fTextScale)
    , m_bCentered(Prototype.m_bCentered)
{
}

HRESULT CUI_Text::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CUI_Text::Initialize(void* pArg)
{
    if (nullptr == pArg)
        return E_FAIL;

    UI_TEXT_DESC* pDesc = static_cast<UI_TEXT_DESC*>(pArg);
    m_strText = pDesc->strText;
    m_strFontTag = pDesc->strFontTag;
    m_fTextScale = pDesc->fTextScale;
    m_bCentered = pDesc->bCentered;

    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    return S_OK;
}

void CUI_Text::Late_Update(_float fTimeDelta)
{
    if (!m_bVisible)
        return;

    // 텍스트는 RG_TEXT 큐에 등록 (SpriteBatch 패스에서 그림)
    m_pGameInstance->Add_RenderObject(CRenderer::RG_TEXT, this);
}

void CUI_Text::Render(ID3D12GraphicsCommandList* _commandList)
{
    if (m_bCentered)
        m_pGameInstance->Draw_Text_Centered(m_strFontTag, m_strText,
            _float2(m_fX, m_fY), m_vColor, m_fTextScale, m_fDepth);
    else
        m_pGameInstance->Draw_Text(m_strFontTag, m_strText,
            _float2(m_fX, m_fY), m_vColor, m_fTextScale, m_fDepth);
}

CUI_Text* CUI_Text::Create(EngineContext* _pContext)
{
    CUI_Text* pInstance = new CUI_Text(_pContext);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Create : CUI_Text");
    }
    return pInstance;
}

CGameObject* CUI_Text::Clone(void* pArg)
{
    CUI_Text* pInstance = new CUI_Text(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Clone : CUI_Text");
    }
    return pInstance;
}

void CUI_Text::Free()
{
    __super::Free();
}