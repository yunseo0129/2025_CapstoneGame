#include "MapSelect.h"
#include "GameInstance.h"
#include "VIBuffer_Rect.h"
#include "Map.h"
#include "Collider.h"
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

CMapSelect::CMapSelect(EngineContext* _pContext)
    : CUIObject(_pContext)
{
}

CMapSelect::CMapSelect(const CMapSelect& Prototype)
    : CUIObject(Prototype)
    , m_pVIBufferCom(Prototype.m_pVIBufferCom)
{
    Safe_AddRef(m_pVIBufferCom);
}

HRESULT CMapSelect::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CMapSelect::Initialize(void* pArg)
{
    if (nullptr == pArg)
        return E_FAIL;

    MAPSELECT_DESC* pDesc = static_cast<MAPSELECT_DESC*>(pArg);
    m_vCenterWorld = pDesc->vCenterWorld;
    m_fWorldRange = pDesc->fWorldRange;
    m_iMapLevelIndex = pDesc->iMapLevelIndex;
    m_strMapLayerTag = pDesc->strMapLayerTag;
    m_strTableModelTag = pDesc->strTableModelTag;
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
    m_fGradeStrength = pDesc->fGradeStrength;

    // 베이스(CUIObject)가 위치/크기/배경색(m_vColor)/화면크기 셋업
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    return S_OK;
}

void CMapSelect::Update(_float fTimeDelta)
{
    __super::Update(fTimeDelta);

    const _bool bShown = (m_bVisible && GetOnOff());

    // [맵 RTT] 보일 때만 탑다운 맵 텍스처를 그리도록 활성화 + 카메라 영역 설정.
    //  중심 = vCenterWorld(x,z), 반경 = fWorldRange, 위쪽 = 북쪽(+Z 고정, 회전 없음).
    if (bShown)
    {
        // [식탁 영역] 최초 표시 시 1회: 중심/반지름 자동 확보 (맵 로드 이후 보이므로 안전)
        Resolve_TableRegion();

        m_pGameInstance->Set_MapRTView(
            _float3(m_vCenterWorld.x, 0.f, m_vCenterWorld.y),
            m_fWorldRange,
            _float3(0.f, 0.f, 1.f));
        m_pGameInstance->Set_MapRTActive(true);

        Handle_Click();
    }
}

void CMapSelect::Render(ID3D12GraphicsCommandList* _commandList)
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
        // b4 : 흰색 곱(선명) + [3]컬러 그레이딩. 사각이라 shapeMode=0(원형 마스크 끔).
        //  rgba(흰색) / x=useTexture, y=shapeMode, z=feather, w=gradeStrength
        _float fParams[8] = {
            1.f, 1.f, 1.f, 1.f,   // 흰색 틴트 (풀 밝기)
            1.f,                  // useTexture
            0.f,                  // shapeMode = 사각 (마스크 없음)
            0.f,                  // feather (미사용)
            m_fGradeStrength      // grade strength
        };
        _commandList->SetGraphicsRoot32BitConstants(RootParameterIndex::UIColor, 8, fParams, 0);

        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpu = m_pGameInstance->Get_GPUHandle(iRTIndex);
        _commandList->SetGraphicsRootDescriptorTable((_uint)RootParameterIndex::TEXTURE_Diffuse, hGpu);
    }
    else
    {
        // RTT 미준비 시 단색 폴백
        Bind_UIColor(_commandList, false);
    }
    m_pVIBufferCom->Render(_commandList);

    // -----------------------------------------------------------------
    // 2) 선택 마커 (스폰 지점을 선택했을 때만)
    // -----------------------------------------------------------------
    if (m_bHasSelection)
    {
        _float mx, my;
        World_To_Pixel(m_vSelectedWorld.x, m_vSelectedWorld.z, mx, my);
        _float4x4 matMarker = Make_PixelRectNDC(
            mx - m_fMarkerSize * 0.5f, my - m_fMarkerSize * 0.5f,
            m_fMarkerSize, m_fMarkerSize);
        Draw_Solid(_commandList, m_pVIBufferCom, matMarker, m_vMarkerColor);
    }
}

// =====================================================================
//  좌표 변환 (회전 없음)
//   화면 오른쪽(+X 픽셀) = 월드 +X,  화면 위(-Y 픽셀) = 월드 +Z
// =====================================================================
void CMapSelect::World_To_Pixel(_float wx, _float wz, _float& outPx, _float& outPy) const
{
    const _float cxPx = m_fX + m_fW * 0.5f;
    const _float cyPx = m_fY + m_fH * 0.5f;
    const _float scale = (m_fW * 0.5f) / m_fWorldRange;

    const _float dx = wx - m_vCenterWorld.x;
    const _float dz = wz - m_vCenterWorld.y;

    outPx = cxPx + dx * scale;   // +X 월드 -> 오른쪽
    outPy = cyPx - dz * scale;   // +Z 월드 -> 위쪽(-Y)
}

void CMapSelect::Pixel_To_World(_float px, _float py, _float& outWx, _float& outWz) const
{
    const _float cxPx = m_fX + m_fW * 0.5f;
    const _float cyPx = m_fY + m_fH * 0.5f;
    const _float scale = (m_fW * 0.5f) / m_fWorldRange; // 0 나눗셈은 range>0 보장으로 회피

    const _float dPx = px - cxPx;
    const _float dPy = py - cyPx;

    outWx = m_vCenterWorld.x + dPx / scale;   // 오른쪽 -> +X
    outWz = m_vCenterWorld.y - dPy / scale;   // 위쪽(-Y) -> +Z
}

// =====================================================================
//  식탁(원형) 영역 자동 확보 (최초 1회)
//   - Layer_Map 에서 식탁 모델 태그를 가진 CMap 을 찾고
//   - 콜라이더 AABB 의 월드 중심/반칸을 읽어
//   - 뷰/선택 중심 = (cx, cz), 선택 반지름 = min(ex, ez) 로 세팅.
//   (맵 콜라이더는 런타임에 월드행렬 재변환이 없어 AABB = 로드값 그대로다)
// =====================================================================
void CMapSelect::Resolve_TableRegion()
{
    if (m_bTableResolved)
        return;

    list<CGameObject*> MapObjs = m_pGameInstance->Get_List(m_iMapLevelIndex, m_strMapLayerTag);
    for (auto& pObj : MapObjs)
    {
        CMap* pMap = dynamic_cast<CMap*>(pObj);
        if (nullptr == pMap)
            continue;
        if (pMap->Get_ModelTag() != m_strTableModelTag)
            continue;

        CCollider* pCol = dynamic_cast<CCollider*>(pMap->Find_Component(TEXT("Com_Collider")));
        if (nullptr == pCol)
            break;

        _float3 vCenter, vExtents;
        if (!pCol->Get_AABBBound(vCenter, vExtents))
            break;

        // 뷰 중심 = 선택 원 중심 = 식탁 중심(XZ)
        m_vCenterWorld = _float2(vCenter.x, vCenter.z);
        // 원형 식탁: 작은 반칸을 반지름으로 → 선택 원이 식탁 안에 완전히 포함
        m_fSelectRadius = fminf(fabsf(vExtents.x), fabsf(vExtents.z));

        m_bTableResolved = true;
        break;
    }
}

// =====================================================================
//  클릭 처리: 창 내부 좌클릭 -> 선택 위치 저장 (+ 그 지점 맵 높이 추정)
// =====================================================================
void CMapSelect::Handle_Click()
{
    // 좌클릭(누른 순간)만 처리
    if (!m_pGameInstance->Mouse_Down(Engine::DIM_LB))
        return;

    // ---- 1) 절대 커서 위치 -> 클라이언트 픽셀 ----
    POINT pt;
    if (!GetCursorPos(&pt))
        return;
    if (g_hWnd != nullptr)
        ScreenToClient(g_hWnd, &pt);

    // ---- 2) 클라이언트 픽셀 -> 디자인(1280x720) 좌표로 스케일 ----
    //  (UI 좌표가 1280x720 기준이므로, 실제 창 크기와 다르면 비율 환산)
    _float fDesignX = (_float)pt.x;
    _float fDesignY = (_float)pt.y;
    if (g_hWnd != nullptr)
    {
        RECT rc;
        if (GetClientRect(g_hWnd, &rc))
        {
            const _float fClientW = (_float)(rc.right - rc.left);
            const _float fClientH = (_float)(rc.bottom - rc.top);
            if (fClientW > 0.f && fClientH > 0.f)
            {
                fDesignX = (_float)pt.x * ((_float)Client::g_iWinSizeX / fClientW);
                fDesignY = (_float)pt.y * ((_float)Client::g_iWinSizeY / fClientH);
            }
        }
    }

    // ---- 3) 창(사각형) 안을 눌렀는지 검사 ----
    if (fDesignX < m_fX || fDesignX > m_fX + m_fW ||
        fDesignY < m_fY || fDesignY > m_fY + m_fH)
        return; // 창 밖 클릭은 무시
    // (맵이 네모 패널 전체에 출력되므로 원형 제한 없음 — 사각형 안 어디든 클릭 가능)

    // ---- 4) 픽셀 -> 월드 (x,z) ----
    _float wx, wz;
    Pixel_To_World(fDesignX, fDesignY, wx, wz);

    // ---- 4b) 식탁(원형) 영역 밖 클릭은 무시 ----
    if (m_fSelectRadius > 0.f)
    {
        const _float ddx = wx - m_vCenterWorld.x;
        const _float ddz = wz - m_vCenterWorld.y;
        if (ddx * ddx + ddz * ddz > m_fSelectRadius * m_fSelectRadius)
            return; // 식탁 반지름 밖 → 선택 무시
    }

    // ---- 5) 그 지점에서 가장 가까운 맵 조각의 높이를 y 로 채택(없으면 0) ----
    _float fBestY = 0.f;
    _float fBestDistSq = FLT_MAX;
    list<CGameObject*> MapObjs = m_pGameInstance->Get_List(m_iMapLevelIndex, m_strMapLayerTag);
    for (auto& pObj : MapObjs)
    {
        if (nullptr == pObj)
            continue;
        _float3 c; _float r;
        if (!pObj->Get_WorldBoundingSphere(c, r))
            continue;
        const _float ddx = c.x - wx;
        const _float ddz = c.z - wz;
        const _float distSq = ddx * ddx + ddz * ddz;
        if (distSq < fBestDistSq)
        {
            fBestDistSq = distSq;
            fBestY = c.y;
        }
    }

    m_vSelectedWorld = _float3(wx, fBestY, wz);
    m_bHasSelection = true;
}

// =====================================================================
//  임의 픽셀 사각형 -> NDC world (CUIObject::Compute_NDCWorld 와 동일 수식)
// =====================================================================
_float4x4 CMapSelect::Make_PixelRectNDC(_float fX, _float fY, _float fW, _float fH) const
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
//   원의 외접 정사각형과 교차시켜 위젯 밖으로 절대 나가지 않게 한다.
// =====================================================================
bool CMapSelect::Clip_RectToCircle(_float inX, _float inY, _float inW, _float inH,
    _float cx, _float cy, _float rad,
    _float& outX, _float& outY, _float& outW, _float& outH) const
{
    if (rad <= 0.f)
        return false;

    _float l = inX, t = inY, r = inX + inW, b = inY + inH;

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

void CMapSelect::Draw_Solid(ID3D12GraphicsCommandList* _commandList, CVIBuffer* pBuffer,
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

HRESULT CMapSelect::Ready_Components()
{
    // 공용 단위 사각형 버퍼 (Loader 에서 LEVEL_STATIC 에 등록되어 있음)
    if (FAILED(Add_Component(LEVEL_STATIC, L"Prototype_Component_VIBuffer_Rect",
        TEXT("Com_VIBuffer"), reinterpret_cast<CComponent**>(&m_pVIBufferCom))))
    {
        MSG_BOX("Failed to Add Component : VIBuffer_Rect in CMapSelect");
        return E_FAIL;
    }
    return S_OK;
}

CMapSelect* CMapSelect::Create(EngineContext* _pContext)
{
    CMapSelect* pInstance = new CMapSelect(_pContext);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Create : CMapSelect");
    }
    return pInstance;
}

CGameObject* CMapSelect::Clone(void* pArg)
{
    CMapSelect* pInstance = new CMapSelect(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        Safe_Release(pInstance);
        MSG_BOX("Failed to Clone : CMapSelect");
    }
    return pInstance;
}

void CMapSelect::Free()
{
    Safe_Release(m_pVIBufferCom);
    __super::Free();
}