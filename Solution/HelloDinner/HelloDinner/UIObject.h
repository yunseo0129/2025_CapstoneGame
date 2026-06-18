#pragma once
#include "GameObject.h"

/*
    CUIObject (추상)
    --------------------------------------------------------------------
    모든 2D UI 요소의 베이스.

    좌표계:
     - 화면 좌상단 (0,0), 우하단 (g_iWinSizeX, g_iWinSizeY) 인 "픽셀 좌표".
     - (m_fX, m_fY) 는 UI 의 좌상단 위치, (m_fW, m_fH) 는 크기(픽셀).
     - Compute_NDCWorld() 가 픽셀 사각형 -> NDC(-1~1) 사각형 변환 행렬을
       만들어 b1(GameObject 슬롯)으로 넘긴다. (Shader_UI.hlsl 참고)

    렌더:
     - Late_Update 에서 m_bVisible 이면 RG_UI 큐에 자신을 등록한다.
     - 자식(CUI_Texture 등)이 Render() 에서 텍스처/버퍼를 바인딩한다.

    깊이(정렬):
     - m_fDepth 가 작을수록 먼저(아래에) 그려진다. (Renderer 에서 정렬)
*/
class CUIObject abstract: public CGameObject
{
public:
    struct UIOBJECT_DESC: public CGameObject::GAMEOBJECT_DESC
    {
        _float fX = 0.f;       // 좌상단 X (픽셀)
        _float fY = 0.f;       // 좌상단 Y (픽셀)
        _float fSizeX = 100.f; // 너비 (픽셀)
        _float fSizeY = 100.f; // 높이 (픽셀)
        _float fDepth = 0.5f;  // 0(앞) ~ 1(뒤) 정렬용
        _float4 vColor = _float4(1.f, 1.f, 1.f, 1.f); // 색상 틴트(rgb)+알파(a)
    };

protected:
    CUIObject(EngineContext* _pContext);
    CUIObject(const CUIObject& Prototype);
    virtual ~CUIObject() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* pArg) override;
    virtual void    Priority_Update(_float fTimeDelta) override;
    virtual void    Update(_float fTimeDelta) override;
    virtual void    Late_Update(_float fTimeDelta) override;
    virtual void    Render(ID3D12GraphicsCommandList* _commandList) override = 0;

public:
    // ---- 속성 ----
    void   Set_Position(_float fX, _float fY) { m_fX = fX; m_fY = fY; }
    void   Set_Size(_float fW, _float fH) { m_fW = fW; m_fH = fH; }
    void   Set_Depth(_float fDepth) { m_fDepth = fDepth; }
    _float Get_Depth() const { return m_fDepth; }

    void   Set_Visible(_bool b) { m_bVisible = b; }
    _bool  Is_Visible() const { return m_bVisible; }

    void   Set_Color(_float4 v) { m_vColor = v; }
    void   Set_Alpha(_float a) { m_vColor.w = a; }
    _float4 Get_Color() const { return m_vColor; }

protected:
    // 픽셀 사각형 -> NDC 사각형 world 행렬을 계산해 m_matNDCWorld 에 저장
    void   Compute_NDCWorld();
    // 계산된 NDC world 를 b1(GameObject)으로 바인딩
    void   Bind_NDCWorld(ID3D12GraphicsCommandList* _commandList);
    // 색상 틴트 + useTexture 플래그를 b4(UIColor)로 바인딩
    //  bUseTexture: true 면 텍스처 사용, false 면 단색
    void   Bind_UIColor(ID3D12GraphicsCommandList* _commandList, _bool bUseTexture);

protected:
    _float     m_fX = 0.f;
    _float     m_fY = 0.f;
    _float     m_fW = 100.f;
    _float     m_fH = 100.f;
    _float     m_fDepth = 0.5f;
    _bool      m_bVisible = true;
    _float4    m_vColor = _float4(1.f, 1.f, 1.f, 1.f); // 색상 틴트 + 알파

    // 화면 크기(픽셀) 캐시
    _float     m_fViewportW = 0.f;
    _float     m_fViewportH = 0.f;

    // 픽셀 -> NDC 변환이 적용된 world 행렬
    _float4x4  m_matNDCWorld;

public:
    virtual void Free() override;
};