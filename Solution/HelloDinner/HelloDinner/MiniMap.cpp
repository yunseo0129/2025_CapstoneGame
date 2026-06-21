#include "MiniMap.h"
#include "GameInstance.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"
#include <cmath>

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
    m_fRadarEdgePx = pDesc->fRadarEdgePx;
    m_fRingThickness = pDesc->fRingThickness;
    m_vRingColor = pDesc->vRingColor;

    // 베이스(CUIObject)가 위치/크기/배경색(m_vColor)/화면크기 셋업
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    return S_OK;
}

void CMiniMap::Update(_float fTimeDelta)
{
    __super::Update(fTimeDelta);

    if (!(m_bVisible && GetOnOff()))
        return;

    // [맵 RTT] 플레이어(카메라) 중심 + 좌우 회전 반영해서 탑다운 맵을 그리도록 활성화.
    //  - 중심 = 카메라 월드 위치(x,z)
    //  - up   = 카메라 수평 FORWARD (피치 무시) -> 보는 방향이 미니맵 위쪽이 됨
    XMFLOAT4X4 viewF = m_pGameInstance->Get_CurrentCameraView();
    XMMATRIX   camWorld = XMMatrixInverse(nullptr, XMLoadFloat4x4(&viewF));
    XMFLOAT4X4 camW;
    XMStoreFloat4x4(&camW, camWorld);

    const _float camX = camW._41;
    const _float camZ = camW._43;

    // 수평 RIGHT 축
    _float rx = camW._11;
    _float rz = camW._13;
    _float rlen = sqrtf(rx * rx + rz * rz);
    if (rlen < 1e-5f) { rx = 1.f; rz = 0.f; }
    else { rx /= rlen; rz /= rlen; }
    // 수평 FORWARD = RIGHT 좌회전
    const _float fx = -rz;
    const _float fz = rx;

    m_pGameInstance->Set_MapRTView(
        _float3(camX, 0.f, camZ),
        m_fViewRange,
        _float3(fx, 0.f, fz)); // up = 보는 방향
    m_pGameInstance->Set_MapRTActive(true);
}

void CMiniMap::Render(ID3D12GraphicsCommandList* _commandList)
{
    if (nullptr == m_pVIBufferCom)
        return;

    // UI 파이프라인 (알파 블렌딩 / 깊이 X)
    m_pGameInstance->Set_PipelineState(_commandList, PSO_TYPE::UI);

    // -----------------------------------------------------------------
    // 1) 배경 = 탑다운 맵 RTT 텍스처. 네모 패널에 그대로(불투명·선명) 출력.
    // -----------------------------------------------------------------
    Bind_NDCWorld(_commandList);

    const _uint iRTIndex = m_pGameInstance->Get_MapRT_SRVIndex();

    if (iRTIndex != 0)
    {
        const _float4 vOld = m_vColor;
        m_vColor = _float4(1.f, 1.f, 1.f, 1.f); // 불투명 흰색 곱 -> 선명
        Bind_UIColor(_commandList, true);
        m_vColor = vOld;

        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpu = m_pGameInstance->Get_GPUHandle(iRTIndex);
        _commandList->SetGraphicsRootDescriptorTable((_uint)RootParameterIndex::TEXTURE_Diffuse, hGpu);
    }
    else
    {
        Bind_UIColor(_commandList, false);
    }
    m_pVIBufferCom->Render(_commandList);

    // -----------------------------------------------------------------
    // 2) 내 플레이어 마커 (중앙 고정). 회전식이라 항상 위를 향함.
    // -----------------------------------------------------------------
    const _float cxPx = m_fX + m_fW * 0.5f;
    const _float cyPx = m_fY + m_fH * 0.5f;
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

// =====================================================================
//  blip 사각형을 원(레이더) 안으로 잘라낸다.
//   - 먼저 원의 외접 정사각형([cx-rad, cx+rad] x [cy-rad, cy+rad])과 교차.
//     => 어떤 거대한 blip 도 위젯 밖으로 절대 못 나간다(화면 가림 방지).
//   - 그 다음, 각 변의 중점이 원 안에 들어오도록 한 번 더 줄여 원형 느낌 유지.
//   - 교차 영역이 없으면 false.
// =====================================================================
bool CMiniMap::Clip_RectToCircle(_float inX, _float inY, _float inW, _float inH,
    _float cx, _float cy, _float rad,
    _float& outX, _float& outY, _float& outW, _float& outH) const
{
    if (rad <= 0.f)
        return false;

    // 입력 사각형
    _float l = inX, t = inY, r = inX + inW, b = inY + inH;

    // 1) 원의 외접 정사각형으로 클램프 (위젯 밖으로 절대 못 나가게)
    const _float bl = cx - rad, bt = cy - rad, br = cx + rad, bb = cy + rad;
    if (l < bl) l = bl;
    if (t < bt) t = bt;
    if (r > br) r = br;
    if (b > bb) b = bb;
    if (r <= l || b <= t)
        return false;

    outX = l; outY = t; outW = r - l; outH = b - t;
    return true;
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