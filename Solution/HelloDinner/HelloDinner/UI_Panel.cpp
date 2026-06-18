#include "UI_Panel.h"
#include "GameInstance.h"
#include "VIBuffer_Rect.h"

CUI_Panel::CUI_Panel(EngineContext* _pContext)
    : CUIObject(_pContext)
{
}

CUI_Panel::CUI_Panel(const CUI_Panel& Prototype)
    : CUIObject(Prototype)
    , m_pVIBufferCom(Prototype.m_pVIBufferCom)
{
    Safe_AddRef(m_pVIBufferCom);
}

HRESULT CUI_Panel::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CUI_Panel::Initialize(void* pArg)
{
    if (nullptr == pArg)
        return E_FAIL;

    // 베이스가 위치/크기/색상/화면크기 셋업
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    return S_OK;
}

void CUI_Panel::Render(ID3D12GraphicsCommandList* _commandList)
{
    // 1) NDC world (위치/크기) -> b1
    Bind_NDCWorld(_commandList);

    // 2) UI PSO
    m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::UI);

    // 3) 색상(단색 모드: useTexture=false) -> b4
    Bind_UIColor(_commandList, false);

    // 4) 사각형 그리기
    if (m_pVIBufferCom != nullptr)
        m_pVIBufferCom->Render(_commandList);
}

HRESULT CUI_Panel::Ready_Components()
{
    // 공용 단위 사각형 버퍼 (LEVEL_STATIC 에 프로토타입 등록되어 있어야 함)
    if (FAILED(Add_Component(LEVEL_STATIC, L"Prototype_Component_VIBuffer_Rect",
        TEXT("Com_VIBuffer"), reinterpret_cast<CComponent**>(&m_pVIBufferCom))))
    {
        MSG_BOX("Failed to Add Component : VIBuffer_Rect in CUI_Panel");
        return E_FAIL;
    }

    return S_OK;
}

CUI_Panel* CUI_Panel::Create(EngineContext* _pContext)
{
    CUI_Panel* pInstance = new CUI_Panel(_pContext);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Create : CUI_Panel");
    }
    return pInstance;
}

CGameObject* CUI_Panel::Clone(void* pArg)
{
    CUI_Panel* pInstance = new CUI_Panel(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Clone : CUI_Panel");
    }
    return pInstance;
}

void CUI_Panel::Free()
{
    Safe_Release(m_pVIBufferCom);
    __super::Free();
}