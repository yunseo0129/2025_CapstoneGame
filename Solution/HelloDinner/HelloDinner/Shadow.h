#pragma once
#include "Base.h"
#include "Graphic_Device.h"

class CShadow final: public CBase
{
private:
    CShadow(EngineContext* pContext, _uint width, _uint height);
    virtual ~CShadow() = default;

public:
    HRESULT Initialize();
    void Update(class CCamera* _camera, class CLight* _light);

    // Light에서 경계구를 바라보는 매트릭스 바인딩
    void Bind_ShadowBuffer(ID3D12GraphicsCommandList* cmdList, RootParameterIndex _eIndex);
    void Bind_ShadowMap(ID3D12GraphicsCommandList* cmdList, RootParameterIndex _eIndex);

    XMFLOAT4X4 Get_LightTransform() const { return m_ShadowTransform; }

    // [파편 그림자] 렌더용 라이트 view/proj (샘플링용 m_ShadowTransform 과 별개)
    void Get_LightViewProj(_float4x4& outView, _float4x4& outProj) const { outView = m_LightView; outProj = m_LightProj; }

    void Begin_Pass(ID3D12GraphicsCommandList* cmdList);
    void End_Pass(ID3D12GraphicsCommandList* cmdList);

    _bool IsSphereInBounds(const _float3& vCenter, _float fRadius) const;

    const DirectX::BoundingSphere& Get_SceneBounds() const { return mSceneBounds; }

private:
    // 카메라 위치에 맞게 경계구 업데이트  - 이거 먼저 업데이트
    void UpdateBoundingSphere(class CCamera* _camera);
    // Light에서 경계구를 바라보는 매트릭스 업데이트  - 이걸 나중에 업데이트
    void UpdateMatrix(class CLight* _light);

    HRESULT Create_ShadowBuffer();
    void Create_Resource();

private:
    EngineContext* m_pContext = {nullptr};
    class CGameInstance* m_pGameInstance = {nullptr};

    D3D12_VIEWPORT m_Viewport = {};
    D3D12_RECT m_ScissorRect = {};

    _uint m_iWidth = 0;
    _uint m_iHeight = 0;

    // Camera를 Light 위치로 변환한 값들 저장하는 변수
    XMFLOAT4X4 m_LightView = {};
    XMFLOAT4X4 m_LightProj = {};
    XMFLOAT3 m_LightPos = {};

    XMFLOAT4X4 m_ShadowTransform = {};

    _float m_Lightnear;
    _float m_Lightfar;

    // srv dsv 관련
    _uint m_iSRVIndex = {0};
    CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuDsvHandle = {};
    ComPtr<ID3D12Resource> m_pShadowMap = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_pDsvHeap = nullptr;

    //cb 관련
    static const _int FRAME_COUNT = CGraphic_Device::SWAP_CHAIN_BUFFER_COUNT;
    ComPtr<ID3D12Resource> m_pShadowbuffer[FRAME_COUNT];
    CB_VS_CAMERA* m_pCbMappedShadow[FRAME_COUNT] {};

    // 경계구
    BoundingSphere mSceneBounds;

public:
    static CShadow* Create(EngineContext* pContext, _uint width, _uint height);
    virtual void Free() override;
};