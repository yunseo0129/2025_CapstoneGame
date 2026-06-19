#include "stdafx.h"
#include "Font_Manager.h"
#include "GameInstance.h"

#include <CommonStates.h>
#include <DirectXHelpers.h>

using namespace DirectX;

IMPLEMENT_SINGLETON(CFont_Manager)

CFont_Manager::CFont_Manager()
    : m_pGameInstance {CGameInstance::GetInstance()}
{
    Safe_AddRef(m_pGameInstance);
}

HRESULT CFont_Manager::Initialize(EngineContext* _pContext)
{
    if (nullptr == _pContext || nullptr == _pContext->device)
        return E_FAIL;

    m_pContext = _pContext;

    // 폰트 전용 SRV 디스크립터 힙 (셰이더 가시)
    m_pFontHeap = std::make_unique<DescriptorHeap>(
        m_pContext->device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        MAX_FONTS);

    // SpriteBatch 생성 (RTV/DSV 포맷 = 백버퍼와 동일)
    //  RTV: R8G8B8A8_UNORM, DSV: D24_UNORM_S8_UINT
    ResourceUploadBatch uploadBatch(m_pContext->device);
    uploadBatch.Begin();

    RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);

    // UI 처럼 알파 블렌딩 + 깊이 끔
    SpriteBatchPipelineStateDescription pd(rtState,
        &CommonStates::NonPremultiplied,   // straight alpha (틴트 색 곱 자연스러움)
        &CommonStates::DepthNone,
        &CommonStates::CullNone);

    m_pSpriteBatch = std::make_unique<SpriteBatch>(m_pContext->device, uploadBatch, pd);

    // 뷰포트 설정 (화면 크기)
    D3D12_VIEWPORT vp = {};
    vp.TopLeftX = 0.f; vp.TopLeftY = 0.f;
    vp.Width = (float)Client::g_iWinSizeX;
    vp.Height = (float)Client::g_iWinSizeY;
    vp.MinDepth = 0.f; vp.MaxDepth = 1.f;
    m_pSpriteBatch->SetViewport(vp);

    // 업로드 완료 대기 (cmdQueue 로 제출)
    auto finish = uploadBatch.End(m_pContext->cmdQueue);
    finish.wait();

    return S_OK;
}

HRESULT CFont_Manager::Add_Font(const _wstring& strFontTag, const _wstring& strFilePath)
{
    if (m_iHeapUsed >= MAX_FONTS)
    {
        MSG_BOX("Font_Manager: 폰트 슬롯 초과");
        return E_FAIL;
    }
    if (m_Fonts.find(strFontTag) != m_Fonts.end())
        return S_OK; // 이미 로드됨

    // 폰트 텍스처를 GPU 로 업로드하며 SpriteFont 생성
    ResourceUploadBatch uploadBatch(m_pContext->device);
    uploadBatch.Begin();

    _uint iSlot = m_iHeapUsed;
    unique_ptr<SpriteFont> pFont;
    try
    {
        pFont = std::make_unique<SpriteFont>(
            m_pContext->device, uploadBatch,
            strFilePath.c_str(),
            m_pFontHeap->GetCpuHandle(iSlot),
            m_pFontHeap->GetGpuHandle(iSlot));
    }
    catch (const std::exception&)
    {
        MSG_BOX("Font_Manager: .spritefont 로드 실패 (경로 확인)");
        uploadBatch.End(m_pContext->cmdQueue).wait();
        return E_FAIL;
    }

    auto finish = uploadBatch.End(m_pContext->cmdQueue);
    finish.wait();

    m_Fonts.emplace(strFontTag, std::move(pFont));
    ++m_iHeapUsed;

    return S_OK;
}

// =====================================================================
//  프레임 텍스트 렌더
// =====================================================================
void CFont_Manager::Begin(ID3D12GraphicsCommandList* _pCmdList)
{
    if (m_bBegun || nullptr == m_pSpriteBatch)
        return;

    // SpriteBatch 가 자체 폰트 힙을 바인딩한다.
    //  (이 호출이 글로벌 SRV 힙 바인딩을 덮어쓰므로,
    //   텍스트는 다른 렌더링이 끝난 뒤 마지막에 그려야 한다.)
    ID3D12DescriptorHeap* heaps[] = {m_pFontHeap->Heap()};
    _pCmdList->SetDescriptorHeaps(_countof(heaps), heaps);

    m_pSpriteBatch->Begin(_pCmdList);
    m_bBegun = true;
}

void CFont_Manager::Draw_Text(const _wstring& strFontTag, const _wstring& strText,
    const _float2& vPos, const _float4& vColor, _float fScale, _float fLayerDepth)
{
    if (!m_bBegun) return;
    SpriteFont* pFont = Find_Font(strFontTag);
    if (nullptr == pFont) return;

    XMVECTOR color = XMVectorSet(vColor.x, vColor.y, vColor.z, vColor.w);
    pFont->DrawString(m_pSpriteBatch.get(), strText.c_str(),
        XMFLOAT2(vPos.x, vPos.y), color, 0.f, XMFLOAT2(0.f, 0.f),
        fScale, SpriteEffects_None, fLayerDepth);
}

void CFont_Manager::Draw_Text_Centered(const _wstring& strFontTag, const _wstring& strText,
    const _float2& vPos, const _float4& vColor, _float fScale, _float fLayerDepth)
{
    if (!m_bBegun) return;
    SpriteFont* pFont = Find_Font(strFontTag);
    if (nullptr == pFont) return;

    // 텍스트 크기의 절반을 origin 으로 주면 중앙 정렬
    XMVECTOR size = pFont->MeasureString(strText.c_str());
    XMFLOAT2 origin;
    XMStoreFloat2(&origin, XMVectorScale(size, 0.5f));

    XMVECTOR color = XMVectorSet(vColor.x, vColor.y, vColor.z, vColor.w);
    pFont->DrawString(m_pSpriteBatch.get(), strText.c_str(),
        XMFLOAT2(vPos.x, vPos.y), color, 0.f, origin,
        fScale, SpriteEffects_None, fLayerDepth);
}

void CFont_Manager::End()
{
    if (!m_bBegun) return;
    m_pSpriteBatch->End();
    m_bBegun = false;
}

_float2 CFont_Manager::Measure(const _wstring& strFontTag, const _wstring& strText, _float fScale)
{
    SpriteFont* pFont = Find_Font(strFontTag);
    if (nullptr == pFont) return _float2(0.f, 0.f);

    XMVECTOR size = pFont->MeasureString(strText.c_str());
    XMFLOAT2 s; XMStoreFloat2(&s, size);
    return _float2(s.x * fScale, s.y * fScale);
}

DirectX::SpriteFont* CFont_Manager::Find_Font(const _wstring& strFontTag)
{
    auto it = m_Fonts.find(strFontTag);
    if (it == m_Fonts.end())
        return nullptr;
    return it->second.get();
}

// =====================================================================
CFont_Manager* CFont_Manager::Create(EngineContext* _pContext)
{
    CFont_Manager* pInstance = new CFont_Manager();
    if (FAILED(pInstance->Initialize(_pContext)))
    {
        MSG_BOX("Failed to Create : CFont_Manager");
        Safe_Release(pInstance);
    }
    return pInstance;
}

void CFont_Manager::Free()
{
    m_Fonts.clear();
    m_pSpriteBatch.reset();
    m_pFontHeap.reset();

    Safe_Release(m_pGameInstance);
    __super::Free();
}