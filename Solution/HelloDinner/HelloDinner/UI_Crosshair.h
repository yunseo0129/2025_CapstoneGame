#pragma once
#include "UIObject.h"

/*
    CUI_Crosshair
    --------------------------------------------------------------------
    화면 중앙에 표시되는 조준점(에임) UI.
    - "가운데 점이 찍힌 원" 텍스처 한 장을 화면 중앙에 그린다.
    - 발사(On_Fire) 시 원이 순간적으로 커졌다가 부드럽게 원래 크기로 복귀한다.
      (총알이 여러 군데로 튀는 반동 표현)
    - 게임플레이(PLAYING) 단계에서만 보이도록, 별도 레이어(Layer_UI_Crosshair)에
      올려두고 Game_Manager 가 ON/OFF 한다.

    좌표:
      (fX, fY)는 CUIObject 규약상 "좌상단" 픽셀 위치이므로,
      중앙 정렬을 위해 Initialize 에서 화면 중심 - 크기/2 로 보정한다.

    사용:
      CUI_Crosshair::CROSSHAIR_DESC d;
      d.fSizeX = 64.f; d.fSizeY = 64.f;     // 기본(쉬고 있을 때) 크기
      d.fDepth = 0.2f;                       // 미니맵(0.3)보다 앞
      d.vColor = _float4(1.f, 1.f, 1.f, 1.f);
      d.strTextureProtoTag = L"Prototype_Component_Texture_Crosshair";
      d.iTextureLevelIndex = LEVEL_GAMEPLAY;
      d.fExpandScale = 1.8f;                 // 발사 시 최대 배율
      d.fRecoverTime = 0.25f;                // 복귀에 걸리는 시간(초)
      Add_GameObject_ToLayer(LEVEL_STATIC, L"Prototype_GameObject_UI_Crosshair",
          LEVEL_GAMEPLAY, L"Layer_UI_Crosshair", &d);
*/
class CUI_Crosshair final: public CUIObject
{
public:
    struct CROSSHAIR_DESC: public CUIObject::UIOBJECT_DESC
    {
        _wstring strTextureProtoTag = L"";   // 원+점 크로스헤어 텍스처
        _uint    iTextureLevelIndex = 0;     // 그 텍스처가 등록된 레벨
        _float   fExpandScale = 1.8f;        // 발사 시 최대 배율(1=변화없음)
        _float   fRecoverTime = 0.25f;       // 원래 크기로 복귀하는 시간(초)
    };

private:
    CUI_Crosshair(EngineContext* _pContext);
    CUI_Crosshair(const CUI_Crosshair& Prototype);
    virtual ~CUI_Crosshair() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* pArg) override;
    virtual void    Update(_float fTimeDelta) override;   // 반동 애니메이션
    virtual void    Render(ID3D12GraphicsCommandList* _commandList) override;

public:
    // 실제 발사가 성공한 순간 호출 → 원이 커졌다가 복귀
    void On_Fire();

private:
    HRESULT Ready_Components(const _wstring& strTextureProtoTag, _uint iTextureLevelIndex);
    void    Apply_Centered_Size(_float fW, _float fH);    // 중앙 정렬 유지하며 크기 적용

private:
    class CVIBuffer* m_pVIBufferCom = nullptr;
    class CTexture* m_pTextureCom = nullptr;

    _wstring m_strTextureProtoTag = L"";
    _uint    m_iTextureLevelIndex = 0;

    // 기본(쉬고 있을 때) 크기. Initialize 에서 DESC 의 fSizeX/Y 로 보관.
    _float   m_fBaseW = 64.f;
    _float   m_fBaseH = 64.f;

    // 반동 파라미터
    _float   m_fExpandScale = 1.8f;   // 발사 직후 배율
    _float   m_fRecoverTime = 0.25f;  // 복귀 시간(초)

    // 반동 진행도: 0 = 완전히 복귀(기본 크기), 1 = 최대 확장.
    // On_Fire 에서 1로 튀고, Update 에서 0으로 선형 감소.
    _float   m_fKick = 0.f;

public:
    static CUI_Crosshair* Create(EngineContext* _pContext);
    virtual CGameObject* Clone(void* pArg) override;
    virtual void Free() override;
};