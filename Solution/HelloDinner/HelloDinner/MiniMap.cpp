#include "MiniMap.h"
#include "GameInstance.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"

namespace
{
    inline _float Clamp01(_float v)
    {
        return (v < 0.f) ? 0.f : (v > 1.f ? 1.f : v);
    }
    inline _float4 Lerp4(const _float4& a, const _float4& b, _float t)
    {
        return _float4(
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t);
    }
}

CMiniMap::CMiniMap(EngineContext* _pContext)
    : CUIObject(_pContext)
{
}

CMiniMap::CMiniMap(const CMiniMap& Prototype)
    : CUIObject(Prototype)
    , m_pVIBufferCom(Prototype.m_pVIBufferCom)
    , m_pTriangleCom(Prototype.m_pTriangleCom)
    , m_pBGTextureCom(Prototype.m_pBGTextureCom)
{
    Safe_AddRef(m_pVIBufferCom);
    Safe_AddRef(m_pTriangleCom);
    Safe_AddRef(m_pBGTextureCom);
}

HRESULT CMiniMap::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CMiniMap::Initialize(void* pArg)
{
    if (nullptr == pArg)
        return E_FAIL;

    MINIMAP_DESC* pDesc = static_cast<MINIMAP_DESC*>(pArg);
    m_strBGTextureProtoTag = pDesc->strBGTextureProtoTag;
    m_iBGTextureLevelIndex = pDesc->iBGTextureLevelIndex;
    m_fViewRange = pDesc->fViewRange;
    m_iMapLevelIndex = pDesc->iMapLevelIndex;
    m_strMapLayerTag = pDesc->strMapLayerTag;
    m_fHeightMin = pDesc->fHeightMin;
    m_fHeightMax = pDesc->fHeightMax;
    m_vColorLow = pDesc->vColorLow;
    m_vColorHigh = pDesc->vColorHigh;
    m_fBlipScale = pDesc->fBlipScale;
    m_fBlipMinPx = pDesc->fBlipMinPx;
    m_fMarkerSize = pDesc->fMarkerSize;
    m_vMarkerColor = pDesc->vMarkerColor;

    // 베이스(CUIObject)가 위치/크기/배경색(m_vColor)/화면크기 셋업
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    return S_OK;
}

void CMiniMap::Render(ID3D12GraphicsCommandList* _commandList)
{
    if (nullptr == m_pVIBufferCom)
        return;

    // UI 파이프라인 (알파 블렌딩 / 깊이 X) — 배경/조각/마커 공통
    m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::UI);

    // -----------------------------------------------------------------
    // 1) 배경 (베이스가 갱신해 둔 m_matNDCWorld 사용)
    // -----------------------------------------------------------------
    Bind_NDCWorld(_commandList);
    if (nullptr != m_pBGTextureCom)
    {
        Bind_UIColor(_commandList, true);
        if (FAILED(m_pBGTextureCom->Bind_ShaderResource(_commandList, RootParameterIndex::TEXTURE_Diffuse)))
        {
            MSG_BOX("Failed to Bind BG Texture in CMiniMap");
            return;
        }
    }
    else
    {
        Bind_UIColor(_commandList, false);
    }
    m_pVIBufferCom->Render(_commandList);

    // -----------------------------------------------------------------
    // 2) 카메라(=내 플레이어 시점)에서 중심 위치 + 수평 방향축 구하기
    //    뷰 행렬의 역 = 카메라 월드. RIGHT(_11,_13) 로 수평축(피치에 강건).
    // -----------------------------------------------------------------
    XMFLOAT4X4 viewF = m_pGameInstance->Get_CurrentCameraView();
    XMMATRIX   camWorld = XMMatrixInverse(nullptr, XMLoadFloat4x4(&viewF));
    XMFLOAT4X4 camW;
    XMStoreFloat4x4(&camW, camWorld);

    const _float camX = camW._41;
    const _float camZ = camW._43;

    // 수평 RIGHT 축 (정규화). 좌우가 반대면 (rx,rz) 부호를 뒤집을 것.
    _float rx = camW._11;
    _float rz = camW._13;
    _float rlen = sqrtf(rx * rx + rz * rz);
    if (rlen < 1e-5f) { rx = 1.f; rz = 0.f; }
    else { rx /= rlen; rz /= rlen; }

    // FORWARD 축 = RIGHT 를 좌회전. 상하가 반대면 (fx,fz) 부호를 뒤집을 것.
    const _float fx = -rz;
    const _float fz = rx;

    // 미니맵 중심 픽셀 / 픽셀-월드 배율 (정사각 가정: m_fW 사용)
    const _float cxPx = m_fX + m_fW * 0.5f;
    const _float cyPx = m_fY + m_fH * 0.5f;
    const _float scale = (m_fW * 0.5f) / m_fViewRange;
    const _float fRangeSq = m_fViewRange * m_fViewRange;

    // -----------------------------------------------------------------
    // 3) 맵 조각 높이맵
    // -----------------------------------------------------------------
    list<CGameObject*> MapObjs = m_pGameInstance->Get_List(m_iMapLevelIndex, m_strMapLayerTag);
    const _float hRange = (m_fHeightMax - m_fHeightMin);

    for (auto& pObj : MapObjs)
    {
        if (nullptr == pObj)
            continue;

        _float3 c; _float r;
        if (!pObj->Get_WorldBoundingSphere(c, r))
            continue;

        const _float dx = c.x - camX;
        const _float dz = c.z - camZ;

        // 보여줄 반경 밖이면 스킵
        if (dx * dx + dz * dz > fRangeSq)
            continue;

        // 상대벡터를 right/forward 축에 투영 -> 미니맵 로컬 (회전 적용)
        const _float lr = dx * rx + dz * rz; // 오른쪽(+)
        const _float lf = dx * fx + dz * fz; // 앞쪽(+) -> 화면 위(-Y)

        const _float bx = cxPx + lr * scale;
        const _float by = cyPx - lf * scale;

        // 높이 -> 색
        _float t = (hRange > 0.f) ? (c.y - m_fHeightMin) / hRange : 0.f;
        t = Clamp01(t);
        const _float4 col = Lerp4(m_vColorLow, m_vColorHigh, t);

        // blip 크기 (최소치 보장)
        _float sz = r * scale * 2.f * m_fBlipScale;
        if (sz < m_fBlipMinPx) sz = m_fBlipMinPx;

        _float4x4 matBlip = Make_PixelRectNDC(bx - sz * 0.5f, by - sz * 0.5f, sz, sz);
        Draw_Solid(_commandList, m_pVIBufferCom, matBlip, col);
    }

    // -----------------------------------------------------------------
    // 4) 내 플레이어 마커 (중앙 고정, 회전식이라 항상 위를 향하는 삼각형)
    // -----------------------------------------------------------------
    CVIBuffer* pMarkerBuffer = (nullptr != m_pTriangleCom) ? m_pTriangleCom : m_pVIBufferCom;
    _float4x4 matMarker = Make_PixelRectNDC(
        cxPx - m_fMarkerSize * 0.5f, cyPx - m_fMarkerSize * 0.5f,
        m_fMarkerSize, m_fMarkerSize);
    Draw_Solid(_commandList, pMarkerBuffer, matMarker, m_vMarkerColor);
}

// =====================================================================
//  임의 픽셀 사각형 -> NDC world (CUIObject::Compute_NDCWorld 와 동일 수식)
// =====================================================================
_float4x4 CMiniMap::Make_PixelRectNDC(_float fX, _float fY, _float fW, _float fH) const
{
    const _float vw = (m_fViewportW > 0.f) ? m_fViewportW : (_float)Client::g_iWinSizeX;
    const _float vh = (m_fViewportH > 0.f) ? m_fViewportH : (_float)Client::g_iWinSizeY;

    const _float sx = 2.f * fW / vw;
    const _float sy = -2.f * fH / vh;
    const _float tx = 2.f * fX / vw - 1.f;
    const _float ty = 1.f - 2.f * fY / vh;

    _float4x4 m;
    m._11 = sx;  m._12 = 0.f; m._13 = 0.f; m._14 = 0.f;
    m._21 = 0.f; m._22 = sy;  m._23 = 0.f; m._24 = 0.f;
    m._31 = 0.f; m._32 = 0.f; m._33 = 1.f; m._34 = 0.f;
    m._41 = tx;  m._42 = ty;  m._43 = 0.f; m._44 = 1.f;
    return m;
}

void CMiniMap::Draw_Solid(ID3D12GraphicsCommandList* _commandList, CVIBuffer* pBuffer,
    const _float4x4& matNDC, const _float4& vColor)
{
    if (nullptr == pBuffer)
        return;

    // b1 : 위치/크기 (NDC world)
    _commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::GameObject, 16, &matNDC, 0);

    // b4 : 색 rgba(4) + param(4). useTexture=0(단색)
    _float fParams[8];
    fParams[0] = vColor.x;
    fParams[1] = vColor.y;
    fParams[2] = vColor.z;
    fParams[3] = vColor.w;
    fParams[4] = 0.f; // useTexture = false
    fParams[5] = 0.f;
    fParams[6] = 0.f;
    fParams[7] = 0.f;
    _commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::UIColor, 8, fParams, 0);

    pBuffer->Render(_commandList);
}

HRESULT CMiniMap::Ready_Components()
{
    // 공용 단위 사각형 버퍼 (Loader 에서 LEVEL_STATIC 에 등록되어 있음)
    if (FAILED(Add_Component(LEVEL_STATIC, L"Prototype_Component_VIBuffer_Rect",
        TEXT("Com_VIBuffer"), reinterpret_cast<CComponent**>(&m_pVIBufferCom))))
    {
        MSG_BOX("Failed to Add Component : VIBuffer_Rect in CMiniMap");
        return E_FAIL;
    }

    // 단위 삼각형 버퍼 (플레이어 방향 마커용. Loader 에서 LEVEL_STATIC 에 등록)
    if (FAILED(Add_Component(LEVEL_STATIC, L"Prototype_Component_VIBuffer_Triangle",
        TEXT("Com_Triangle"), reinterpret_cast<CComponent**>(&m_pTriangleCom))))
    {
        MSG_BOX("Failed to Add Component : VIBuffer_Triangle in CMiniMap");
        return E_FAIL;
    }

    // 배경 텍스처 (지정된 경우에만)
    if (!m_strBGTextureProtoTag.empty())
    {
        if (FAILED(Add_Component(m_iBGTextureLevelIndex, m_strBGTextureProtoTag,
            TEXT("Com_BGTexture"), reinterpret_cast<CComponent**>(&m_pBGTextureCom))))
        {
            MSG_BOX("Failed to Add Component : BG Texture in CMiniMap");
            return E_FAIL;
        }
    }

    return S_OK;
}

CMiniMap* CMiniMap::Create(EngineContext* _pContext)
{
    CMiniMap* pInstance = new CMiniMap(_pContext);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Create : CMiniMap");
    }
    return pInstance;
}

CGameObject* CMiniMap::Clone(void* pArg)
{
    CMiniMap* pInstance = new CMiniMap(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Clone : CMiniMap");
    }
    return pInstance;
}

void CMiniMap::Free()
{
    Safe_Release(m_pVIBufferCom);
    Safe_Release(m_pTriangleCom);
    Safe_Release(m_pBGTextureCom);
    __super::Free();
}