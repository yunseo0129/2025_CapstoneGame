#pragma once
#include "Base.h"
#include "Graphic_Device.h"

/*
    CMapRenderTarget
    --------------------------------------------------------------------
    "맵을 위에서 내려다본" 컬러 텍스처를 만드는 오프스크린 렌더 타겟.
    (CShadow 의 컬러 버전. 셰도우는 깊이만 그리지만 여기는 색이 필요해
     RTV(컬러 타겟)를 추가로 만든다.)

    용도:
     - CMiniMap / CMapSelect 가 직접 blip 을 그리는 대신, 이 타겟에 한 번
       탑다운으로 씬(Layer_Map)을 그려 그 결과 텍스처를 배경으로 표시한다.
       => 오브젝트의 실제 모양/텍스처/회전이 그대로 반영된다.

    카메라(탑다운):
     - eye = (center.x, 높은 곳, center.z), look = 수직 아래(-Y).
     - up = "수평 회전 방향". 미니맵은 플레이어가 보는 수평 방향을 up 으로 줘서
       좌우 회전만 반영(피치 무시). MapSelect 는 up=+Z(북쪽 고정).
     - 직교 폭 = 보여줄 반경(fHalfExtent)*2. (미니맵 10m, 맵셀렉트 50m 등)

    사용 흐름 (GameInstance::Draw 안에서):
       pRT->Set_View(center, halfExtent, upDir);   // 매 프레임(미니맵) 또는 1회(맵셀렉트)
       pRT->Begin_Pass(cmd);                        // RT 클리어 + 바인딩 + 카메라 바인딩
       pRT->Render_MapLayer(cmd, level, L"Layer_Map");
       pRT->End_Pass(cmd);                          // SRV 로 읽기 가능 상태로 전이
     그 뒤 UI 단계에서:
       pMiniMap->Set_RTSRVIndex(pRT->Get_SRVIndex());
*/
class CMapRenderTarget final: public CBase
{
private:
    CMapRenderTarget(EngineContext* _pContext, _uint _iWidth, _uint _iHeight);
    virtual ~CMapRenderTarget() = default;

public:
    HRESULT Initialize();

    // 탑다운 카메라 갱신.
    //  vCenterXZ : 창 중심이 될 월드 (x, z)
    //  fHalfExtent : 중심에서 ±범위(월드유닛). 직교 폭의 절반.
    //  vUpDirXZ : 화면 위쪽이 될 수평 방향(정규화 불필요, 내부에서 정규화).
    //             (0,0,1) 이면 월드 +Z 가 위(북쪽 고정).
    void Set_View(const _float3& vCenterXZ, _float fHalfExtent, const _float3& vUpDirXZ);

    // RT 를 렌더 타겟으로 바인딩(+클리어) 하고 탑다운 카메라를 b0(Camera)로 바인딩.
    void Begin_Pass(ID3D12GraphicsCommandList* _cmdList);
    // RT 를 SRV 로 읽을 수 있는 상태로 전이.
    void End_Pass(ID3D12GraphicsCommandList* _cmdList);

    // Layer_Map 의 오브젝트들을 인스턴싱으로 이 타겟에 그린다.
    //  (메인 인스턴스 큐는 메인 패스에서 소비/비워지므로 여기서 직접 Get_List)
    HRESULT Render_MapLayer(ID3D12GraphicsCommandList* _cmdList, _uint _iLevelIndex, const _wstring& _strLayerTag);

    // 결과 컬러 텍스처의 글로벌 SRV 힙 인덱스 (UI 가 배경으로 바인딩)
    _uint  Get_SRVIndex() const { return m_iColorSRVIndex; }

    // 배경(클리어) 색 지정 (기본 투명)
    void   Set_ClearColor(const _float4& v) { m_vClearColor = v; }

private:
    void    Create_ColorTarget();   // 컬러 RT + RTV + SRV
    void    Create_DepthTarget();   // 깊이 + DSV
    HRESULT Create_CameraBuffer();  // 탑다운 카메라 CB (프레임별)
    HRESULT Create_InstanceBuffer();// 월드행렬 인스턴스 버퍼 (프레임별)
    void    Bind_CameraBuffer(ID3D12GraphicsCommandList* _cmdList);

private:
    EngineContext* m_pContext = {nullptr};
    class CGameInstance* m_pGameInstance = {nullptr};

    _uint  m_iWidth = 256;
    _uint  m_iHeight = 256;

    D3D12_VIEWPORT m_Viewport = {};
    D3D12_RECT     m_ScissorRect = {};

    _float4 m_vClearColor = _float4(0.f, 0.f, 0.f, 0.f);

    // 탑다운 카메라 행렬
    _float4x4 m_matView = {};
    _float4x4 m_matProj = {};
    _float3   m_vEyePos = {};

    // ---- 컬러 타겟 ----
    ComPtr<ID3D12Resource>       m_pColorTex = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_pRtvHeap = nullptr;
    CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuRtvHandle = {};
    _uint m_iColorSRVIndex = 0;           // 글로벌 SRV 힙 인덱스
    D3D12_RESOURCE_STATES m_eColorState = D3D12_RESOURCE_STATE_GENERIC_READ;

    // ---- 깊이 타겟 ----
    ComPtr<ID3D12Resource>       m_pDepthTex = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_pDsvHeap = nullptr;
    CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuDsvHandle = {};

    // ---- 카메라 CB (프레임별) ----
    static const _int FRAME_COUNT = CGraphic_Device::SWAP_CHAIN_BUFFER_COUNT;
    ComPtr<ID3D12Resource> m_pCameraBuffer[FRAME_COUNT];
    CB_VS_CAMERA* m_pCbMappedCamera[FRAME_COUNT] = {};

    // ---- 인스턴스 버퍼 (프레임별, 월드행렬 업로드) ----
    static const _uint MAX_MAP_INSTANCES = 1024;
    ComPtr<ID3D12Resource>   m_pInstanceBuffer[FRAME_COUNT];
    XMFLOAT4X4* m_pInstMapped[FRAME_COUNT] = {};
    D3D12_VERTEX_BUFFER_VIEW m_InstanceVBV[FRAME_COUNT] = {};

public:
    static CMapRenderTarget* Create(EngineContext* _pContext, _uint _iWidth, _uint _iHeight, const _float4& _vClearColor = _float4(0.f, 0.f, 0.f, 0.f));
    virtual void Free() override;
};