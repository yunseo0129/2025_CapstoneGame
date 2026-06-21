#pragma once
#include "UIObject.h"

/*
    CMiniMap  (방식 B: 2D 절차적 높이맵 / 플레이어 회전식)
    --------------------------------------------------------------------
    - 화면 우상단에 고정 크기로 배치.
    - "플레이어(카메라)가 보는 방향"이 항상 미니맵 위쪽이 되도록 회전.
    - Layer_Map 의 각 조각을 위에서 본 사각형(blip)으로 그리고,
      조각의 높이(center.y)를 색으로 인코딩(낮음<->높음 색 보간)한다.
    - 내 플레이어는 미니맵 중앙에 마커로 고정 표시.

    방향(회전):
     - GameInstance::Get_CurrentCameraView() 의 역행렬 = 카메라 월드.
       · 카메라 위치(_41,_43) = 미니맵 중심에 대응.
       · 카메라 RIGHT(_11,_13) 로 수평 기준축을 잡는다(피치에 강건).
     - 각 오브젝트의 (월드 - 카메라) 상대벡터를 right/forward 축에 투영해
       미니맵 로컬 좌표로 변환한다.

    튜닝(빌드 후):
     - 좌우가 반대로 보이면 right 축 부호를, 상하가 반대면 forward 축 부호를 뒤집는다.
       (MiniMap.cpp Render 의 rx/rz, fx/fz 주석 참고)
     - 너무 좁/넓게 보이면 fViewRange, 색이 단조로우면 fHeightMin/Max 를 조정.
*/
class CMiniMap final: public CUIObject
{
public:
    struct MINIMAP_DESC: public CUIObject::UIOBJECT_DESC
    {
        // 배경 (비우면 단색 m_vColor)
        _wstring strBGTextureProtoTag = L"";
        _uint    iBGTextureLevelIndex = 0;

        // 보여줄 범위 = 플레이어 주변 반경(월드 유닛). 작을수록 확대.
        _float   fViewRange = 40.f;

        // 맵(지형) 레이어
        _uint    iMapLevelIndex = 3 /*LEVEL_GAMEPLAY*/;
        _wstring strMapLayerTag = L"Layer_Map";

        // 높이 -> 색 매핑 구간 및 양 끝 색
        _float   fHeightMin = -5.f;
        _float   fHeightMax = 20.f;
        _float4  vColorLow = _float4(0.10f, 0.15f, 0.45f, 1.f); // 낮음: 어두운 파랑
        _float4  vColorHigh = _float4(0.95f, 0.35f, 0.20f, 1.f); // 높음: 붉은 주황

        // 맵 조각 표시 크기(픽셀): radius*scale*2*배율, 최소 보장치
        _float   fBlipScale = 1.0f;
        _float   fBlipMinPx = 3.f;

        // 내 플레이어 마커 (중앙 고정)
        _float   fMarkerSize = 12.f;
        _float4  vMarkerColor = _float4(0.2f, 1.f, 0.3f, 1.f);

        // 레이더(원형) 표시: 원 밖으로 새는 점을 잘라낸다.
        //  fRadarEdgePx: 위젯 반지름에서 안쪽으로 줄일 여백(테두리). 0이면 위젯에 꽉 참.
        _float   fRadarEdgePx = 2.f;
        // 테두리 링 그리기 (레이더 느낌). 두께 0이면 안 그림.
        _float   fRingThickness = 2.f;
        _float4  vRingColor = _float4(0.35f, 0.9f, 0.45f, 0.8f);
    };

private:
    CMiniMap(EngineContext* _pContext);
    CMiniMap(const CMiniMap& Prototype);
    virtual ~CMiniMap() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* pArg) override;
    virtual void    Update(_float fTimeDelta) override;
    virtual void    Render(ID3D12GraphicsCommandList* _commandList) override;

public:
    // 실행 중 튜닝용
    void Set_ViewRange(_float f) { if (f > 0.f) m_fViewRange = f; }
    void Set_HeightRange(_float lo, _float hi) { m_fHeightMin = lo; m_fHeightMax = hi; }
    void Set_MarkerColor(_float4 v) { m_vMarkerColor = v; }

private:
    HRESULT   Ready_Components();

    // 임의의 픽셀 사각형(좌상단 fX,fY / 크기 fW,fH) -> NDC world 행렬
    _float4x4 Make_PixelRectNDC(_float fX, _float fY, _float fW, _float fH) const;

    // blip 사각형을 레이더 원(중심 cx,cy / 반지름 rad) 안으로 잘라낸다.
    //  반환 false 면 원 밖이라 그릴 게 없음. true 면 (outX,outY,outW,outH)에 잘린 사각형.
    bool Clip_RectToCircle(_float inX, _float inY, _float inW, _float inH,
        _float cx, _float cy, _float rad,
        _float& outX, _float& outY, _float& outW, _float& outH) const;

    // 단색 도형 그리기 (b1=위치, b4=색/단색, 지정 버퍼 렌더)
    void      Draw_Solid(ID3D12GraphicsCommandList* _commandList, class CVIBuffer* pBuffer,
        const _float4x4& matNDC, const _float4& vColor);

private:
    class CVIBuffer* m_pVIBufferCom = nullptr; // 공용 단위 사각형 (배경/맵 조각)
    class CVIBuffer* m_pTriangleCom = nullptr; // 단위 삼각형 (플레이어 방향 마커)
    class CTexture* m_pBGTextureCom = nullptr; // 배경 텍스처(옵션)

    _wstring m_strBGTextureProtoTag = L"";
    _uint    m_iBGTextureLevelIndex = 0;

    _float   m_fViewRange = 40.f;

    _uint    m_iMapLevelIndex = 3;
    _wstring m_strMapLayerTag = L"Layer_Map";

    _float   m_fHeightMin = -5.f;
    _float   m_fHeightMax = 20.f;
    _float4  m_vColorLow = _float4(0.10f, 0.15f, 0.45f, 1.f);
    _float4  m_vColorHigh = _float4(0.95f, 0.35f, 0.20f, 1.f);

    _float   m_fBlipScale = 1.0f;
    _float   m_fBlipMinPx = 3.f;

    _float   m_fMarkerSize = 12.f;
    _float4  m_vMarkerColor = _float4(0.2f, 1.f, 0.3f, 1.f);

    _float   m_fRadarEdgePx = 2.f;
    _float   m_fRingThickness = 2.f;
    _float4  m_vRingColor = _float4(0.35f, 0.9f, 0.45f, 0.8f);

public:
    static CMiniMap* Create(EngineContext* _pContext);
    virtual CGameObject* Clone(void* pArg) override;
    virtual void Free() override;
};