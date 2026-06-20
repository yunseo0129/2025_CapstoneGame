#include "UI_Crosshair.h"
#include "GameInstance.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"

CUI_Crosshair::CUI_Crosshair(EngineContext* _pContext)
    : CUIObject(_pContext)
{
}

CUI_Crosshair::CUI_Crosshair(const CUI_Crosshair& Prototype)
    : CUIObject(Prototype)
    , m_pVIBufferCom(Prototype.m_pVIBufferCom)
    , m_pTextureCom(Prototype.m_pTextureCom)
{
    Safe_AddRef(m_pVIBufferCom);
    Safe_AddRef(m_pTextureCom);
}

HRESULT CUI_Crosshair::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CUI_Crosshair::Initialize(void* pArg)
{
    if (nullptr == pArg)
        return E_FAIL;

    CROSSHAIR_DESC* pDesc = static_cast<CROSSHAIR_DESC*>(pArg);
    m_strTextureProtoTag = pDesc->strTextureProtoTag;
    m_iTextureLevelIndex = pDesc->iTextureLevelIndex;
    m_fExpandScale = pDesc->fExpandScale;
    m_fRecoverTime = (pDesc->fRecoverTime > 0.0001f) ? pDesc->fRecoverTime : 0.25f;

    // 베이스(CUIObject)가 위치/크기/색상/화면크기 셋업
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    // 기본 크기 보관 (반동 애니메이션의 기준값)
    m_fBaseW = m_fW;
    m_fBaseH = m_fH;

    // 시작은 기본 크기로 화면 중앙 정렬
    Apply_Centered_Size(m_fBaseW, m_fBaseH);

    if (FAILED(Ready_Components(m_strTextureProtoTag, m_iTextureLevelIndex)))
        return E_FAIL;

    return S_OK;
}

void CUI_Crosshair::Update(_float fTimeDelta)
{
    // 반동 진행도 m_fKick 를 0(기본 크기)을 향해 선형 감소.
    //  On_Fire 에서 1로 튀므로, 발사 직후엔 가장 크고 점점 복귀한다.
    if (m_fKick > 0.f)
    {
        m_fKick -= fTimeDelta / m_fRecoverTime;
        if (m_fKick < 0.f)
            m_fKick = 0.f;
    }

    // 현재 배율: 1 ~ fExpandScale 사이를 m_fKick 가 보간.
    //  m_fKick=1 → fExpandScale, m_fKick=0 → 1.0
    const _float fCurScale = 1.f + (m_fExpandScale - 1.f) * m_fKick;

    Apply_Centered_Size(m_fBaseW * fCurScale, m_fBaseH * fCurScale);
}

void CUI_Crosshair::Render(ID3D12GraphicsCommandList* _commandList)
{
    // 1) NDC world (위치/크기) -> b1
    Bind_NDCWorld(_commandList);

    // 2) UI PSO
    m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::UI);

    // 3) 색상 틴트(텍스처 모드) -> b4
    Bind_UIColor(_commandList, true);

    // 4) 텍스처(t0) 바인딩
    if (m_pTextureCom != nullptr)
    {
        if (FAILED(m_pTextureCom->Bind_ShaderResource(_commandList, RootParameterIndex::TEXTURE_Diffuse)))
        {
            MSG_BOX("Failed to Bind Texture in CUI_Crosshair");
            return;
        }
    }

    // 5) 사각형 그리기
    if (m_pVIBufferCom != nullptr)
        m_pVIBufferCom->Render(_commandList);
}

void CUI_Crosshair::On_Fire()
{
    // 발사 성공 → 반동 진행도를 최대(1)로 리셋.
    //  이미 커져 있는 상태에서 다시 쏴도 즉시 최대로 튀어 연사감이 살아난다.
    m_fKick = 1.f;
}

void CUI_Crosshair::Apply_Centered_Size(_float fW, _float fH)
{
    // 크기를 바꾸되, 중심이 화면 중앙에 고정되도록 좌상단 좌표를 보정.
    //  (CUIObject 의 (m_fX, m_fY)는 좌상단 픽셀 위치이므로 중심-크기/2)
    m_fW = fW;
    m_fH = fH;
    m_fX = (m_fViewportW - fW) * 0.5f;
    m_fY = (m_fViewportH - fH) * 0.5f;
}

HRESULT CUI_Crosshair::Ready_Components(const _wstring& strTextureProtoTag, _uint iTextureLevelIndex)
{
    // 공용 단위 사각형 버퍼
    if (FAILED(Add_Component(LEVEL_STATIC, L"Prototype_Component_VIBuffer_Rect",
        TEXT("Com_VIBuffer"), reinterpret_cast<CComponent**>(&m_pVIBufferCom))))
    {
        MSG_BOX("Failed to Add Component : VIBuffer_Rect in CUI_Crosshair");
        return E_FAIL;
    }

    // 원+점 크로스헤어 텍스처
    if (!strTextureProtoTag.empty())
    {
        if (FAILED(Add_Component(iTextureLevelIndex, strTextureProtoTag,
            TEXT("Com_Texture"), reinterpret_cast<CComponent**>(&m_pTextureCom))))
        {
            MSG_BOX("Failed to Add Component : Texture in CUI_Crosshair");
            return E_FAIL;
        }
    }

    return S_OK;
}

CUI_Crosshair* CUI_Crosshair::Create(EngineContext* _pContext)
{
    CUI_Crosshair* pInstance = new CUI_Crosshair(_pContext);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Create : CUI_Crosshair");
    }
    return pInstance;
}

CGameObject* CUI_Crosshair::Clone(void* pArg)
{
    CUI_Crosshair* pInstance = new CUI_Crosshair(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Clone : CUI_Crosshair");
    }
    return pInstance;
}

void CUI_Crosshair::Free()
{
    Safe_Release(m_pVIBufferCom);
    Safe_Release(m_pTextureCom);
    __super::Free();
}