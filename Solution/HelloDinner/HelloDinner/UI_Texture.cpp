#include "UI_Texture.h"
#include "GameInstance.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"

CUI_Texture::CUI_Texture(EngineContext* _pContext)
    : CUIObject(_pContext)
{
}

CUI_Texture::CUI_Texture(const CUI_Texture& Prototype)
    : CUIObject(Prototype)
    , m_pVIBufferCom(Prototype.m_pVIBufferCom)
    , m_pTextureCom(Prototype.m_pTextureCom)
{
    Safe_AddRef(m_pVIBufferCom);
    Safe_AddRef(m_pTextureCom);
}

HRESULT CUI_Texture::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CUI_Texture::Initialize(void* pArg)
{
    if (nullptr == pArg)
        return E_FAIL;

    UI_TEXTURE_DESC* pDesc = static_cast<UI_TEXTURE_DESC*>(pArg);
    m_strTextureProtoTag = pDesc->strTextureProtoTag;
    m_iTextureLevelIndex = pDesc->iTextureLevelIndex;

    // 베이스(CUIObject)가 위치/크기/화면크기 셋업
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    if (FAILED(Ready_Components(m_strTextureProtoTag, m_iTextureLevelIndex)))
        return E_FAIL;

    return S_OK;
}

void CUI_Texture::Render(ID3D12GraphicsCommandList* _commandList)
{
    // 1) NDC world (위치/크기) 바인딩  -> b1
    Bind_NDCWorld(_commandList);

    // 2) UI PSO
    m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::UI);

    // 3) 색상 틴트(텍스처 모드: useTexture=true) -> b4
    Bind_UIColor(_commandList, true);

    // 4) 텍스처(t0) 바인딩
    if (m_pTextureCom != nullptr)
    {
        if (FAILED(m_pTextureCom->Bind_ShaderResource(_commandList, RootParameterIndex::TEXTURE_Diffuse)))
        {
            MSG_BOX("Failed to Bind Texture in CUI_Texture");
            return;
        }
    }

    // 5) 사각형 그리기
    if (m_pVIBufferCom != nullptr)
        m_pVIBufferCom->Render(_commandList);
}

HRESULT CUI_Texture::Ready_Components(const _wstring& strTextureProtoTag, _uint iTextureLevelIndex)
{
    // 공용 단위 사각형 버퍼 (LEVEL_STATIC 에 프로토타입 등록 권장)
    if (FAILED(Add_Component(LEVEL_STATIC, L"Prototype_Component_VIBuffer_Rect",
        TEXT("Com_VIBuffer"), reinterpret_cast<CComponent**>(&m_pVIBufferCom))))
    {
        MSG_BOX("Failed to Add Component : VIBuffer_Rect in CUI_Texture");
        return E_FAIL;
    }

    // 텍스처 (호출 측이 지정한 프로토타입 태그)
    if (!strTextureProtoTag.empty())
    {
        if (FAILED(Add_Component(iTextureLevelIndex, strTextureProtoTag,
            TEXT("Com_Texture"), reinterpret_cast<CComponent**>(&m_pTextureCom))))
        {
            MSG_BOX("Failed to Add Component : Texture in CUI_Texture");
            return E_FAIL;
        }
    }

    return S_OK;
}

CUI_Texture* CUI_Texture::Create(EngineContext* _pContext)
{
    CUI_Texture* pInstance = new CUI_Texture(_pContext);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Create : CUI_Texture");
    }
    return pInstance;
}

CGameObject* CUI_Texture::Clone(void* pArg)
{
    CUI_Texture* pInstance = new CUI_Texture(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Clone : CUI_Texture");
    }
    return pInstance;
}

void CUI_Texture::Free()
{
    Safe_Release(m_pVIBufferCom);
    Safe_Release(m_pTextureCom);
    __super::Free();
}