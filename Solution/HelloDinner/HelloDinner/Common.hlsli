// 중복 정의 방지
#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

// --------------------------------------------------------
//  Constant Buffers (상수 버퍼)
// --------------------------------------------------------
// b0 : camera 행렬 정보 (매 프레임 갱신)
cbuffer cbTransform : register(b0)
{
    row_major matrix g_matView; // 뷰 행렬
    row_major matrix g_matProj; // 투영 행렬
    float3 g_vCamPosWS; // 월드 공간 카메라 위치
};
// b1 : Object 행렬 정보 (오브젝트마다 갱신)
cbuffer cbObject : register(b1)
{
    row_major matrix g_matWorld; // 월드 행렬
};

// b2 : Bone Matrices (애니메이션 메쉬 전용)
cbuffer cbBoneMatrices : register(b2)
{
    row_major matrix g_BoneMatrices[512];
};

// b3 : 조명 정보 (매 프레임 갱신)
cbuffer LightParams : register(b3)
{
    float4 g_vLightDir;
    float4 g_vLightPos;
    float4 g_vLightDiffuse;
    float4 g_vLightAmbient;
    float4 g_vLightSpecular;
    float g_fLightRange;
    float3 g_vPadding;
};

// ------------------------------------------------
// Textures & Samplers
// ------------------------------------------------
// t0 ~ t1 : 텍스처 정보
Texture2D g_DiffuseTextures : register(t0);
Texture2D g_NormalTextures : register(t1);
// t0, space1 : 큐브맵 텍스처 (Skybox 전용, 별도 space 사용)
TextureCube g_DiffuseTexCube : register(t0);

// Static Samplers
SamplerState g_samWrap : register(s0); // 일반 3D
SamplerState g_samClamp : register(s1); // UI
SamplerState g_samPoint : register(s2); // 도트
SamplerState g_samAnisotropic : register(s3); // 지형
// ------------------------------------------------

// --------------------------------------------------------
// Common Structures
// --------------------------------------------------------
struct VS_OUT
{
    float4 vPosition : SV_POSITION; // 화면 좌표
    float3 vWorldPos : TEXCOORD1; // 월드 좌표 (조명 계산용)
    float3 vNormal : NORMAL; // 노멀
    float2 vUV : TEXCOORD0; // UV
    float3 vBinormal : BINORMAL; 
    float3 vTangent : TANGENT;
};

// --------------------------------------------------------
// Common Pixel Shader (Lit)
// --------------------------------------------------------
// 일반/애니메이션 메쉬 공통 사용
float4 PS_Main_Lit(VS_OUT In) : SV_TARGET
{
    float4 normalMapColor = g_NormalTextures.Sample(g_samWrap, In.vUV);
    float x = normalMapColor.r * 2.0f - 1.0f;
    float y = normalMapColor.g * 2.0f - 1.0f;
    float z = sqrt(max(1.0f - (x * x + y * y), 0.0f));
    
    float3 vNormal = float3(x, y, z);

    // 2. World Normal 변환
    float3x3 matTBN = float3x3(normalize(In.vTangent),
                               normalize(In.vBinormal),
                               normalize(In.vNormal));
    float3 vWorldNormal = normalize(mul(vNormal, matTBN));

    // 3. 라이팅
    float3 vL = normalize(g_vLightDir.xyz * -1.0f);
    float fDiffuseIdx = max(dot(vWorldNormal, vL), 0.0f);

    float4 diffuseColor = g_DiffuseTextures.Sample(g_samWrap, In.vUV);

    float3 vDiffuse = diffuseColor.rgb * (g_vLightDiffuse.rgb * fDiffuseIdx);
    float3 vAmbient = diffuseColor.rgb * g_vLightAmbient.rgb;

    return float4(vDiffuse + vAmbient, diffuseColor.a);
}

#endif