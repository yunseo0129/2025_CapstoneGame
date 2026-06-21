#pragma once
#include "UIObject.h"

/*
    CMapSelect  (맵 정보 창 + 스폰 위치 선택)
    --------------------------------------------------------------------
    - 상점 대신 띄우는 "맵을 위에서 내려다본" 정보 창.
    - CMiniMap 과 달리 "플레이어 중심/회전" 이 아니라,
      내가 정한 고정 영역(중심 vCenterWorld, 범위 fWorldRange)을 그린다.
        · 따라서 "맵의 어느 부분을 보여줄지" 는 desc 의 vCenterWorld /
          fWorldRange 만 바꾸면 자유롭게 정할 수 있다. (요구사항 2)
        · 회전 없음(항상 북쪽이 위). 월드 +X = 오른쪽, 월드 +Z = 위쪽 으로 매핑.
    - Layer_Map 조각들을 위에서 본 사각형(blip)으로 그리고, 높이를 색으로 인코딩.
    - 창 내부를 좌클릭하면 그 픽셀을 월드 (x,z) 로 역변환해서 "선택 스폰 위치"
      로 저장하고, 마커로 표시한다. (실제 이동은 게임 시작 시 Game_Manager 가 처리)

    좌표 매핑(회전 없음):
     - 창 중심 픽셀 (cx,cy) <-> 월드 (vCenterWorld.x, vCenterWorld.z)
     - 픽셀 1칸 = (fWorldRange*2 / 창너비) 월드유닛
     - 화면 오른쪽(+X 픽셀) = 월드 +X,  화면 위(-Y 픽셀) = 월드 +Z

    튜닝:
     - 보여줄 영역이 안 맞으면 vCenterWorld / fWorldRange 조정.
     - 높이 색이 단조로우면 fHeightMin/Max 조정.
     - 좌우/상하가 뒤집혀 보이면 World_To_Pixel / Pixel_To_World 의 부호를 맞춰 뒤집기.
*/
class CMapSelect final: public CUIObject
{
public:
    struct MAPSELECT_DESC: public CUIObject::UIOBJECT_DESC
    {
        // ---- 보여줄 맵 영역 (여기만 바꾸면 어느 부분이든 보여줄 수 있음) ----
        _float2  vCenterWorld = _float2(0.f, 0.f); // 창 중심에 올 월드 (x, z)
        _float   fWorldRange = 60.f;              // 중심에서 ±범위(월드유닛). 작을수록 확대.

        // ---- 맵(지형) 레이어 ----
        _uint    iMapLevelIndex = 3 /*LEVEL_GAMEPLAY*/;
        _wstring strMapLayerTag = L"Layer_Map";

        // ---- 높이 -> 색 매핑 ----
        _float   fHeightMin = -5.f;
        _float   fHeightMax = 20.f;
        _float4  vColorLow = _float4(0.10f, 0.15f, 0.45f, 1.f); // 낮음: 어두운 파랑
        _float4  vColorHigh = _float4(0.95f, 0.35f, 0.20f, 1.f); // 높음: 붉은 주황

        // ---- 맵 조각 표시 크기(픽셀) ----
        _float   fBlipScale = 1.0f;
        _float   fBlipMinPx = 3.f;

        // ---- 선택 마커 ----
        _float   fMarkerSize = 14.f;
        _float4  vMarkerColor = _float4(0.2f, 1.f, 0.3f, 1.f);

        // ---- 레이더(원형) 표시: 원 밖으로 새는 점/클릭을 잘라낸다 ----
        _float   fRadarEdgePx = 2.f;
        _float   fRingThickness = 2.f;
        _float4  vRingColor = _float4(0.35f, 0.9f, 0.45f, 0.8f);
    };

private:
    CMapSelect(EngineContext* _pContext);
    CMapSelect(const CMapSelect& Prototype);
    virtual ~CMapSelect() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* pArg) override;
    virtual void    Update(_float fTimeDelta) override;
    virtual void    Render(ID3D12GraphicsCommandList* _commandList) override;

public:
    // ---- 보여줄 영역 런타임 변경(원하면 코드 어디서든 호출) ----
    void Set_ViewCenter(_float fWorldX, _float fWorldZ) { m_vCenterWorld = _float2(fWorldX, fWorldZ); }
    void Set_WorldRange(_float f) { if (f > 0.f) m_fWorldRange = f; }
    void Set_HeightRange(_float lo, _float hi) { m_fHeightMin = lo; m_fHeightMax = hi; }

    // ---- 선택된 스폰 위치 ----
    _bool   Has_Selection() const { return m_bHasSelection; }
    _float3 Get_SelectedWorld() const { return m_vSelectedWorld; } // y 는 맵 조각 높이(없으면 0)
    void    Clear_Selection() { m_bHasSelection = false; }

private:
    HRESULT   Ready_Components();

    // 좌표 변환 (회전 없음)
    void      World_To_Pixel(_float wx, _float wz, _float& outPx, _float& outPy) const;
    void      Pixel_To_World(_float px, _float py, _float& outWx, _float& outWz) const;

    // 클릭 처리: 창 내부 좌클릭 -> 선택 위치 저장
    void      Handle_Click();

    // 임의의 픽셀 사각형 -> NDC world 행렬
    _float4x4 Make_PixelRectNDC(_float fX, _float fY, _float fW, _float fH) const;

    // blip 사각형을 레이더 원(중심 cx,cy / 반지름 rad) 안으로 잘라낸다.
    bool Clip_RectToCircle(_float inX, _float inY, _float inW, _float inH,
        _float cx, _float cy, _float rad,
        _float& outX, _float& outY, _float& outW, _float& outH) const;

    // 단색 도형 그리기
    void      Draw_Solid(ID3D12GraphicsCommandList* _commandList, class CVIBuffer* pBuffer,
        const _float4x4& matNDC, const _float4& vColor);

private:
    class CVIBuffer* m_pVIBufferCom = nullptr; // 공용 단위 사각형 (배경/맵 조각/마커)

    _float2  m_vCenterWorld = _float2(0.f, 0.f);
    _float   m_fWorldRange = 60.f;

    _uint    m_iMapLevelIndex = 3;
    _wstring m_strMapLayerTag = L"Layer_Map";

    _float   m_fHeightMin = -5.f;
    _float   m_fHeightMax = 20.f;
    _float4  m_vColorLow = _float4(0.10f, 0.15f, 0.45f, 1.f);
    _float4  m_vColorHigh = _float4(0.95f, 0.35f, 0.20f, 1.f);

    _float   m_fBlipScale = 1.0f;
    _float   m_fBlipMinPx = 3.f;

    _float   m_fMarkerSize = 14.f;
    _float4  m_vMarkerColor = _float4(0.2f, 1.f, 0.3f, 1.f);

    _float   m_fRadarEdgePx = 2.f;
    _float   m_fRingThickness = 2.f;
    _float4  m_vRingColor = _float4(0.35f, 0.9f, 0.45f, 0.8f);

    // 선택 상태
    _bool    m_bHasSelection = false;
    _float3  m_vSelectedWorld = _float3(0.f, 0.f, 0.f);

public:
    static CMapSelect* Create(EngineContext* _pContext);
    virtual CGameObject* Clone(void* pArg) override;
    virtual void Free() override;
};