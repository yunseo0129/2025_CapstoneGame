#include "Shader_Manager.h"

IMPLEMENT_SINGLETON ( CShader_Manager )

CShader_Manager::CShader_Manager ()
{
}

HRESULT CShader_Manager::Initialize ( const ComPtr<ID3D12Device>& pDevice )
{
	m_pDevice = pDevice;
	// 전역 루트 시그니처 생성
	if ( FAILED ( Create_GlobalRootSignature () ) )
	{
		MSG_BOX ( "Failed to Create : GlobalRootSignature" );
		return E_FAIL;
	}

	if ( FAILED ( Create_PSO () ) )
	{
		MSG_BOX ( "Failed to Create : PSO" );
		return E_FAIL;
	}

	return S_OK;
}

void CShader_Manager::Set_PipelineState ( ID3D12GraphicsCommandList* pCmdList , const PSO_TYPE& _eType )
{
	ComPtr<ID3D12PipelineState> pPSO = Get_PSO ( _eType );
	if ( pPSO )
	{
		pCmdList->SetPipelineState ( pPSO.Get () );
	}
}

HRESULT CShader_Manager::Create_GlobalRootSignature ()
{
	//----------------------------------------------------------------------
	// Root Parameter 설정
	//----------------------------------------------------------------------
	// 계속 추가하자
	CD3DX12_ROOT_PARAMETER parameters[( _uint )RootParameterIndex::End];

	// [t0, space0]
	// RangeType: SRV (Shader Resource View)
	// NumDescriptors: 1
	// BaseShaderRegister: 0 (t0 부터 시작)
	// RegisterSpace: 0 (space0)
	CD3DX12_DESCRIPTOR_RANGE rangesDiffuse[1]; // 
	rangesDiffuse[0].Init ( D3D12_DESCRIPTOR_RANGE_TYPE_SRV , 1 , 0 , 0 );	//t0

	CD3DX12_DESCRIPTOR_RANGE rangesNormal[1]; // 
	rangesNormal[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);	//t1



	// Material용으로 추가

	// [Parameter 0] : CBV (Camera)
	// -> cbTransform : register(b0)
	// 카메라 정보 (뷰, 프로젝션 행렬)
	parameters[RootParameterIndex::Camera].InitAsConstantBufferView ( 0 , 0 );

	// [Parameter 1] : Constant32 (Object)
	// -> cbObject : register(b1)
	// Transform 정보
	parameters[RootParameterIndex::GameObject].InitAsConstants ( 16 , 1 );

	// [Parameter 2, 3] : Texture Table
	// Diffuse Texture (t0)과 Normal Texture (t1)로 나눠서 관리
	parameters[RootParameterIndex::TEXTURE_Diffuse].InitAsDescriptorTable ( 1 , &rangesDiffuse[0] );
	parameters[RootParameterIndex::TEXTURE_Normal].InitAsDescriptorTable(1, &rangesNormal[0]);

	// [Parameter 3] : CBV (BoneMatrix)
	// -> cbBoneMatrices : register(b2)
	parameters[RootParameterIndex::BoneMatrix].InitAsConstantBufferView(2, 0);

	//----------------------------------------------------------------------
	// Static Sampler s0 ~ s4
	//----------------------------------------------------------------------
	CD3DX12_STATIC_SAMPLER_DESC samplers[5];
	// s0: Linear / Wrap (기본 3D 물체)
	samplers[0].Init ( 0 ,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR ,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP ,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP ,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP );

	// s1: Linear / Clamp (스카이박스, 이펙트, UI)
	samplers[1].Init ( 1 ,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR ,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP ,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP ,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP );

	// s2: Point / Clamp (도트 아트, 폰트, 정확한 UV 참조)
	samplers[2].Init ( 2 ,
		D3D12_FILTER_MIN_MAG_MIP_POINT ,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP ,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP ,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP );

	// s3: Anisotropic (지형, 바닥 - 멀리서도 선명함)
	// 잘 안 쓸듯?
	samplers[3].Init ( 3 ,
		D3D12_FILTER_ANISOTROPIC ,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP ,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP ,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP ,
		0.0f , 8 ); // MaxAnisotropy = 8

	// s4: Shadow Comparison (그림자 맵 전용)
	samplers[4].Init ( 4 ,
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT ,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER ,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER ,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER ,
		0.0f , 16 ,
		D3D12_COMPARISON_FUNC_LESS_EQUAL ,
		D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK );

	//----------------------------------------------------------------------
	// Flags 설정
	//----------------------------------------------------------------------
	D3D12_ROOT_SIGNATURE_FLAGS rootSigFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // IA 단계 사용 허용 (필수)
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |       // HS는 루트 서명 접근 금지 (최적화)
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |     // DS는 접근 금지
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;    // GS는 접근 금지


	//----------------------------------------------------------------------
	// Root Signature 생성
	//----------------------------------------------------------------------
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init (
		_countof ( parameters ) ,
		parameters ,
		_countof ( samplers ) ,
		samplers ,
		rootSigFlags );

	// 직렬화 (Serialize) - 텍스트 설정을 바이너리로 변환
	ID3DBlob* pSignature = nullptr;
	ID3DBlob* pError = nullptr;

	if ( FAILED ( D3D12SerializeRootSignature ( &rootSignatureDesc , D3D_ROOT_SIGNATURE_VERSION_1 , &pSignature , &pError ) ) )
	{
		if ( pError )
		{
			OutputDebugStringA ( ( char* )pError->GetBufferPointer () );
			pError->Release ();
		}
		return E_FAIL;
	}

	// 실제 객체 생성
	if ( FAILED ( m_pDevice->CreateRootSignature ( 0 , pSignature->GetBufferPointer () , pSignature->GetBufferSize () , IID_PPV_ARGS ( &m_pRootSignature ) ) ) )
		return E_FAIL;

	Safe_Release ( pSignature );
	Safe_Release ( pError );

	return S_OK;
}

HRESULT CShader_Manager::Create_PSO ()
{
	Create_InputLayouts (); // 레이아웃 준비

	// ----------------------------------------------------------------
	// 1. 쉐이더 컴파일
	// ----------------------------------------------------------------
	// Vertex Shader (Input Layout별)
	ComPtr<ID3DBlob> vsStatic = Compile_Shader ( L"Shader_Default.hlsl" , "VS_Main_Static" , "vs_5_1" );
	ComPtr<ID3DBlob> vsSkybox = Compile_Shader ( L"Shader_Skybox.hlsl" , "VS_Main_Skybox" , "vs_5_1" );
	ComPtr<ID3DBlob> vsAnim = Compile_Shader ( L"Shader_Anim.hlsl" , "VS_Main_Anim" , "vs_5_1" );
	// ComPtr<ID3DBlob> vsUI = Compile_Shader ( L"Shader_UI.hlsl" , "VS_Main_UI" , "vs_5_1" );

	// Pixel Shader (재질별)
	ComPtr<ID3DBlob> psLit = Compile_Shader ( L"Shader_Default.hlsl" , "PS_Main_Lit" , "ps_5_1" ); // 조명 O
	ComPtr<ID3DBlob> psSkybox = Compile_Shader ( L"Shader_Skybox.hlsl" , "PS_Main_Skybox" , "ps_5_1" ); // Skybox
	// ComPtr<ID3DBlob> psUI = Compile_Shader ( L"Shader_UI.hlsl" , "PS_Main_UI" , "ps_5_1" ); // 조명 X

	// ----------------------------------------------------------------
	// 2. 기본 PSO Desc 작성 (공통 설정)
	// ----------------------------------------------------------------
	D3D12_GRAPHICS_PIPELINE_STATE_DESC baseDesc = {};
	baseDesc.pRootSignature = m_pRootSignature.Get ();
	baseDesc.RasterizerState = CD3DX12_RASTERIZER_DESC ( D3D12_DEFAULT ); // Cull Back
	baseDesc.BlendState = CD3DX12_BLEND_DESC ( D3D12_DEFAULT );      // Opaque
	baseDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC ( D3D12_DEFAULT ); // Z-Write On
	baseDesc.SampleMask = UINT_MAX;
	baseDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	baseDesc.NumRenderTargets = 1;
	baseDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	baseDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	baseDesc.SampleDesc.Count = 1;

	// ================================================================
	// DEFAULT (Static Mesh / Opaque / Lit)
	// ================================================================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = baseDesc;
	psoDesc.InputLayout = { m_LayoutStatic.data (), ( UINT )m_LayoutStatic.size () };
	psoDesc.VS = { vsStatic->GetBufferPointer (), vsStatic->GetBufferSize () };
	psoDesc.PS = { psLit->GetBufferPointer (), psLit->GetBufferSize () };

	m_pDevice->CreateGraphicsPipelineState ( &psoDesc , IID_PPV_ARGS ( &m_pPSOs[( UINT )PSO_TYPE::DEFAULT] ) );

	// ================================================================
	// SKYBOX (Static Mesh / TextureCube / DepthFunc LessEqual)
	// ================================================================
	psoDesc = baseDesc; // 리셋
	psoDesc.InputLayout = { m_LayoutStatic.data (), ( UINT )m_LayoutStatic.size () };
	psoDesc.VS = { vsSkybox->GetBufferPointer (), vsSkybox->GetBufferSize () };
	psoDesc.PS = { psSkybox->GetBufferPointer (), psSkybox->GetBufferSize () };

	// 스카이박스는 항상 가장 뒤에 있어야 하므로 z=w 트릭 사용
	// DepthFunc를 LESS_EQUAL로 변경해야 z=1(far plane)에서도 통과
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// 스카이박스 내부에서 렌더링하므로 Front Face Culling (안쪽 면을 보여줌)
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	m_pDevice->CreateGraphicsPipelineState ( &psoDesc , IID_PPV_ARGS ( &m_pPSOs[( UINT )PSO_TYPE::SKYBOX] ) );

	
	// ================================================================
	// ANIMATION (Anim Mesh / Opaque / Lit)
	// ================================================================
	psoDesc = baseDesc; // 리셋
	// 변경점: InputLayout과 VertexShader가 애니메이션용으로 바뀜
	psoDesc.InputLayout = { m_LayoutAnim.data (), ( UINT )m_LayoutAnim.size () };
	psoDesc.VS = { vsAnim->GetBufferPointer (), vsAnim->GetBufferSize () };
	psoDesc.PS = { psLit->GetBufferPointer (), psLit->GetBufferSize () };

	m_pDevice->CreateGraphicsPipelineState ( &psoDesc , IID_PPV_ARGS ( &m_pPSOs[( UINT )PSO_TYPE::ANIM] ) );

	/*
	// ================================================================
	// ALPHA BLEND (Static Mesh / Transparent / Lit)
	// ================================================================
	psoDesc = baseDesc; // 리셋
	psoDesc.InputLayout = { m_LayoutStatic.data (), ( UINT )m_LayoutStatic.size () };
	psoDesc.VS = { vsStatic->GetBufferPointer (), vsStatic->GetBufferSize () };
	psoDesc.PS = { psLit->GetBufferPointer (), psLit->GetBufferSize () };

	// 변경점: 블렌드 켜기 & 깊이 쓰기 끄기(Z-Write Off)
	D3D12_RENDER_TARGET_BLEND_DESC& blend = psoDesc.BlendState.RenderTarget[0];
	blend.BlendEnable = TRUE;
	blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend.BlendOp = D3D12_BLEND_OP_ADD;

	// 반투명은 보통 깊이 테스트는 하되, 기록(Write)은 안 함 (뒤에 있는 것도 보여야 하니까)
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	m_pDevice->CreateGraphicsPipelineState ( &psoDesc , IID_PPV_ARGS ( &m_pPSOs[( UINT )PSO_TYPE::ALPHA_BLEND] ) );


	// ================================================================
	// UI (UI Mesh / Transparent / Unlit)
	// ================================================================
	psoDesc = baseDesc; // 리셋
	// 변경점: UI 레이아웃, UI 쉐이더(조명X), 깊이 검사 끄기
	psoDesc.InputLayout = { m_LayoutUI.data (), ( UINT )m_LayoutUI.size () };
	psoDesc.VS = { vsUI->GetBufferPointer (), vsUI->GetBufferSize () };
	psoDesc.PS = { psUI->GetBufferPointer (), psUI->GetBufferSize () };

	// 블렌드 켜기
	psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

	// 깊이 검사 아예 끄기 (항상 그림)
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 양면 다 그림

	m_pDevice->CreateGraphicsPipelineState ( &psoDesc , IID_PPV_ARGS ( &m_pPSOs[( UINT )PSO_TYPE::UI] ) );


	// ================================================================
	// SHADOW (Static Mesh / No Color / Depth Only)
	// ================================================================
	psoDesc = baseDesc; // 리셋
	psoDesc.InputLayout = { m_LayoutStatic.data (), ( UINT )m_LayoutStatic.size () };
	// 그림자용 가벼운 VS 사용 (위치만 계산)
	ComPtr<ID3DBlob> vsShadow = Compile_Shader ( L"Shadow.hlsl" , "VS_Shadow" , "vs_5_1" );
	psoDesc.VS = { vsShadow->GetBufferPointer (), vsShadow->GetBufferSize () };

	// 픽셀 쉐이더 없음 (Null)
	psoDesc.PS = { nullptr, 0 };

	// 렌더 타겟 없음 (오직 Depth Buffer에만 기록)
	psoDesc.NumRenderTargets = 0;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

	// 그림자 아티팩트(Shadow Acne) 방지 바이어스
	psoDesc.RasterizerState.DepthBias = 1000;
	psoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;

	m_pDevice->CreateGraphicsPipelineState ( &psoDesc , IID_PPV_ARGS ( &m_pPSOs[( UINT )PSO_TYPE::SHADOW_STATIC] ) );
	*/

	return S_OK;
}

// 쉐이더 컴파일 헬퍼 (매번 치기 귀찮으니 함수로 뺌)
ID3DBlob* CShader_Manager::Compile_Shader ( const wstring& strPath , const char* strEntry , const char* strTarget )
{
	ID3DBlob* pBlob = nullptr;
	ID3DBlob* pError = nullptr;

	// 디버그 모드면 최적화 끄고 디버그 정보 포함
	UINT iFlag = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	iFlag |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	if ( FAILED ( D3DCompileFromFile ( strPath.c_str () , nullptr , D3D_COMPILE_STANDARD_FILE_INCLUDE ,
		strEntry , strTarget , iFlag , 0 , &pBlob , &pError ) ) )
	{
		if ( pError ) {
			OutputDebugStringA ( ( char* )pError->GetBufferPointer () );
			MSG_BOX ( "Failed to Created : Compile_Shader" );
			pError->Release ();
		}
		return nullptr;
	}
	Safe_Release ( pError );
	return pBlob;
}

void CShader_Manager::Create_InputLayouts ()
{
	// 1) Static Mesh (Pos, Normal, UV, Tangent)
	m_LayoutStatic = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// 2) Animation Mesh (Static + BoneID, Weights)
	m_LayoutAnim = m_LayoutStatic; // 기본 복사 후 추가
	m_LayoutAnim.push_back({ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
	m_LayoutAnim.push_back({ "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
	
	// 3) UI (Pos, UV)
	m_LayoutUI = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

CShader_Manager* CShader_Manager::Create(const ComPtr<ID3D12Device>& pDevice)
{
	CShader_Manager* pInstance = new CShader_Manager();
	if ( FAILED ( pInstance->Initialize ( pDevice ) ) )
	{
		MSG_BOX ( "Failed to Created : CShader_Manager" );
		Safe_Release ( pInstance );
	}
	return pInstance;
}

void CShader_Manager::Free ()
{
	for (UINT i = 0; i < (UINT)PSO_TYPE::END; ++i)
		m_pPSOs[i].Reset();

	m_pRootSignature.Reset();
	m_pDevice.Reset();

	CBase::Free ();
}

