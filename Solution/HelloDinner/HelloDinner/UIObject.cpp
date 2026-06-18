#include "UIObject.h"
#include "Transform.h"
#include "GameInstance.h"

CUIObject::CUIObject(EngineContext* _pContext)
    : CGameObject(_pContext)
{
    XMStoreFloat4x4(&m_matNDCWorld, XMMatrixIdentity());
}

CUIObject::CUIObject(const CUIObject& Prototype)
    : CGameObject(Prototype)
    , m_fX(Prototype.m_fX)
    , m_fY(Prototype.m_fY)
    , m_fW(Prototype.m_fW)
    , m_fH(Prototype.m_fH)
    , m_fDepth(Prototype.m_fDepth)
    , m_bVisible(Prototype.m_bVisible)
    , m_vColor(Prototype.m_vColor)
{
    XMStoreFloat4x4(&m_matNDCWorld, XMMatrixIdentity());
}

HRESULT CUIObject::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CUIObject::Initialize(void* pArg)
{
    // Transform 은 UI 에서 직접 쓰지 않지만, 베이스 호환을 위해 생성
    if (nullptr == m_pTransformCom)
        m_pTransformCom = CTransform::Create(m_pContext);

    if (nullptr != pArg)
    {
        UIOBJECT_DESC* pDesc = static_cast<UIOBJECT_DESC*>(pArg);
        m_fX = pDesc->fX;
        m_fY = pDesc->fY;
        m_fW = pDesc->fSizeX;
        m_fH = pDesc->fSizeY;
        m_fDepth = pDesc->fDepth;
        m_vColor = pDesc->vColor;
    }

    // 화면 크기 캐시 (고정 윈도우)
    m_fViewportW = (_float)Client::g_iWinSizeX;
    m_fViewportH = (_float)Client::g_iWinSizeY;

    return S_OK;
}

void CUIObject::Priority_Update(_float fTimeDelta)
{
}

void CUIObject::Update(_float fTimeDelta)
{
}

void CUIObject::Late_Update(_float fTimeDelta)
{
    if (!m_bVisible)
        return;

    // 매 프레임 위치/크기 기반으로 NDC world 갱신 후 UI 큐 등록
    Compute_NDCWorld();
    m_pGameInstance->Add_RenderObject(CRenderer::RG_UI, this);
}

// =====================================================================
//  픽셀 사각형 -> NDC 사각형 변환 행렬
//   단위정점 u,v(0~1) 에 대해:
//     x_pixel = u*W + X      -> ndc_x = 2*x_pixel/VW - 1
//     y_pixel = v*H + Y      -> ndc_y = 1 - 2*y_pixel/VH
//   전개:
//     ndc_x = u*(2W/VW) + (2X/VW - 1)
//     ndc_y = v*(-2H/VH) + (1 - 2Y/VH)
//   row-major (행벡터 곱):
//     row0 (u계수): [ 2W/VW,      0,    0, 0 ]
//     row1 (v계수): [ 0,     -2H/VH,    0, 0 ]
//     row2 (z)    : [ 0,          0,    1, 0 ]
//     row3 (상수) : [ 2X/VW-1, 1-2Y/VH, 0, 1 ]
// =====================================================================
void CUIObject::Compute_NDCWorld()
{
    if (m_fViewportW <= 0.f) m_fViewportW = (_float)Client::g_iWinSizeX;
    if (m_fViewportH <= 0.f) m_fViewportH = (_float)Client::g_iWinSizeY;

    const _float sx = 2.f * m_fW / m_fViewportW;
    const _float sy = -2.f * m_fH / m_fViewportH;
    const _float tx = 2.f * m_fX / m_fViewportW - 1.f;
    const _float ty = 1.f - 2.f * m_fY / m_fViewportH;

    _float4x4 m;
    m._11 = sx;  m._12 = 0.f; m._13 = 0.f; m._14 = 0.f;
    m._21 = 0.f; m._22 = sy;  m._23 = 0.f; m._24 = 0.f;
    m._31 = 0.f; m._32 = 0.f; m._33 = 1.f; m._34 = 0.f;
    m._41 = tx;  m._42 = ty;  m._43 = 0.f; m._44 = 1.f;

    m_matNDCWorld = m;
}

void CUIObject::Bind_NDCWorld(ID3D12GraphicsCommandList* _commandList)
{
    // b1(GameObject 슬롯)에 16 float (NDC world) 전달
    _commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &m_matNDCWorld, 0);
}

void CUIObject::Bind_UIColor(ID3D12GraphicsCommandList* _commandList, _bool bUseTexture)
{
    // b4(UIColor 슬롯): [color rgba(4)] + [param xyzw(4)] = 8 float
    _float fParams[8];
    fParams[0] = m_vColor.x;   // r
    fParams[1] = m_vColor.y;   // g
    fParams[2] = m_vColor.z;   // b
    fParams[3] = m_vColor.w;   // a
    fParams[4] = bUseTexture ? 1.f : 0.f; // useTexture
    fParams[5] = 0.f;
    fParams[6] = 0.f;
    fParams[7] = 0.f;
    _commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::UIColor, 8, fParams, 0);
}

void CUIObject::Free()
{
    Safe_Release(m_pTransformCom);
    __super::Free();
}