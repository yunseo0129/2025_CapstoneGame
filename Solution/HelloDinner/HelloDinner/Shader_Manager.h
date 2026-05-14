#pragma once
#include "Base.h"

class CShader_Manager final : public CBase
{
    DECLARE_SINGLETON ( CShader_Manager )

private:
    CShader_Manager ();
    virtual ~CShader_Manager () = default;

public:
    // 초기화: 루트 시그니처 생성
    HRESULT Initialize ( const ComPtr<ID3D12Device>& pDevice );

    // 루트 시그니처 반환
    ComPtr<ID3D12RootSignature> Get_RootSignature () const { return m_pRootSignature; }

public:
    // 렌더링 파이프라인 설정을 도와주는 함수 (편의성)
    void Set_PipelineState ( ID3D12GraphicsCommandList* pCmdList , const PSO_TYPE& _eType );

    void Set_RootSignature ( ID3D12GraphicsCommandList* pCmdList )
    {
        pCmdList->SetGraphicsRootSignature ( m_pRootSignature.Get () );
    }

private:
    // 초기 생성 함수들
    HRESULT Create_GlobalRootSignature ();
    HRESULT Create_PSO ();

    // PSO 반환
    ComPtr<ID3D12PipelineState> Get_PSO ( PSO_TYPE eType ) { return m_pPSOs[( _uint )eType]; }

    // Create_PSO에서 사용할 함수
    ID3DBlob* Compile_Shader ( const wstring& strPath , const char* strEntry , const char* strTarget );
    void Create_InputLayouts (); // 레이아웃 초기화 함수
private:
    ComPtr<ID3D12Device> m_pDevice = nullptr;

    // 전역 루트 시그니처
    ComPtr<ID3D12RootSignature> m_pRootSignature = nullptr; // 전역 루트 시그니처

    // 파이프라인 상태 객체들
    ComPtr<ID3D12PipelineState> m_pPSOs[( _uint )PSO_TYPE::END] = { nullptr };

    // Input Layout 정의 (3가지 타입)
    vector<D3D12_INPUT_ELEMENT_DESC> m_LayoutStatic;
    vector<D3D12_INPUT_ELEMENT_DESC> m_LayoutAnim;
    vector<D3D12_INPUT_ELEMENT_DESC> m_LayoutUI;

public:
	static CShader_Manager* Create(const ComPtr<ID3D12Device>& pDevice);
    virtual void Free () override;
};

