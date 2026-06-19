#pragma once
#include "Base.h"

#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <DescriptorHeap.h>
#include <ResourceUploadBatch.h>

/*
    CFont_Manager
    --------------------------------------------------------------------
    DirectXTK(D3D12) 기반 2D 텍스트 렌더링 매니저 (싱글톤).

    - .spritefont 비트맵 폰트를 로드해 SpriteFont 로 그린다.
    - SpriteBatch 가 자체 디스크립터 힙을 바인딩하므로, 텍스트는
      "다른 모든 렌더링이 끝난 뒤" (RG_UI 패스 다음) 한 번에 그린다.

    사용 흐름 (한 프레임):
        Begin(cmdList);
        Draw_Text(L"Hello", {x, y}, color, scale);
        ... (여러 번)
        End();

    좌표: 화면 픽셀 (좌상단 0,0). SpriteFont 의 DrawString 과 동일.
*/
class CFont_Manager final: public CBase
{
    DECLARE_SINGLETON(CFont_Manager)
private:
    CFont_Manager();
    virtual ~CFont_Manager() = default;

public:
    HRESULT Initialize(EngineContext* _pContext);

    // 폰트 1개 로드 (태그로 구분, 여러 폰트 등록 가능)
    //  strFilePath: ../Resources/Font/xxx.spritefont
    HRESULT Add_Font(const _wstring& strFontTag, const _wstring& strFilePath);

    // ---- 프레임 텍스트 렌더 ----
    void Begin(ID3D12GraphicsCommandList* _pCmdList);
    // strFontTag 폰트로 텍스트를 그린다. (Begin~End 사이에서 호출)
    void Draw_Text(const _wstring& strFontTag, const _wstring& strText,
        const _float2& vPos, const _float4& vColor = _float4(1.f, 1.f, 1.f, 1.f),
        _float fScale = 1.f, _float fLayerDepth = 0.f);
    // 중앙 정렬 버전 (vPos 가 텍스트 중심)
    void Draw_Text_Centered(const _wstring& strFontTag, const _wstring& strText,
        const _float2& vPos, const _float4& vColor = _float4(1.f, 1.f, 1.f, 1.f),
        _float fScale = 1.f, _float fLayerDepth = 0.f);
    void End();

    // 텍스트 픽셀 크기 측정 (정렬용)
    _float2 Measure(const _wstring& strFontTag, const _wstring& strText, _float fScale = 1.f);

private:
    DirectX::SpriteFont* Find_Font(const _wstring& strFontTag);

private:
    class CGameInstance* m_pGameInstance = nullptr;

    // SpriteBatch 1개 (모든 폰트가 공유)
    unique_ptr<DirectX::SpriteBatch>  m_pSpriteBatch;

    // 폰트 전용 디스크립터 힙 (폰트 텍스처 SRV 들)
    unique_ptr<DirectX::DescriptorHeap> m_pFontHeap;
    _uint m_iHeapUsed = 0;                      // 사용한 힙 슬롯 수
    static constexpr _uint MAX_FONTS = 16;      // 최대 폰트 개수(힙 크기)

    // 태그 -> SpriteFont
    map<_wstring, unique_ptr<DirectX::SpriteFont>> m_Fonts;

    _bool m_bBegun = false;

public:
    static CFont_Manager* Create(EngineContext* _pContext);
    virtual void Free() override;

private:
    EngineContext* m_pContext = nullptr;
};