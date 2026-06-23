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
    row_major matrix g_matLightTransform;
};

// ------------------------------------------------
// Textures & Samplers
// ------------------------------------------------
// t0 ~ t1 : 텍스처 정보
Texture2D g_DiffuseTextures : register(t0);
Texture2D g_NormalTextures : register(t1);
Texture2D g_ShadowMap : register(t2);
// t0, space1 : 큐브맵 텍스처 (Skybox 전용, 별도 space 사용)
TextureCube g_DiffuseTexCube : register(t0);

// Static Samplers
SamplerState g_samWrap : register(s0); // 일반 3D
SamplerState g_samClamp : register(s1); // UI
SamplerState g_samPoint : register(s2); // 도트
SamplerState g_samAnisotropic : register(s3); // 지형
SamplerComparisonState g_samShadowCmp : register(s4); // 그림자 PCF (LESS_EQUAL)
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
    
    float4 ShadowPos : TEXCOORD2; // 그림자 맵용 좌표 (빛의 시점에서 변환된 위치)
};

// --------------------------------------------------------
// shadow
float CalcShadowFactor(float4 shadowPos)
{
    shadowPos.xyz /= shadowPos.w;

    // 범위 밖 = 그림자 없음 (s4 border 가 TRANSPARENT_BLACK 이라 명시 체크 유지)
    if (shadowPos.x < 0.0f || shadowPos.x > 1.0f ||
        shadowPos.y < 0.0f || shadowPos.y > 1.0f ||
        shadowPos.z < 0.0f || shadowPos.z > 1.0f)
    {
        return 1.0f;
    }

    uint w, h;
    g_ShadowMap.GetDimensions(w, h);
    float texel = 1.0f / (float) w;

    float bias = 0.0015f; // depth-space 바이어스 (튜닝)
    float d = shadowPos.z - bias;

    // 3x3 PCF: 비교 샘플러(LESS_EQUAL) → ref <= 저장깊이 일 때 1(=조명됨)
    float sum = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            sum += g_ShadowMap.SampleCmpLevelZero(
                g_samShadowCmp, shadowPos.xy + float2(x, y) * texel, d);
        }
    }
    float lit = sum / 9.0f; // 0 = 완전 그림자, 1 = 완전 조명

    return lerp(0.5f, 1.0f, lit); // 기존 0.5 반그림자 톤 유지
}

// --------------------------------------------------------
// Common Lighting (Lit RGB 계산 공유)
// --------------------------------------------------------
//  PS_Main_Lit(불투명) 과 PS_Glass(반투명) 가 동일 조명을 공유한다.
//  lit RGB 를 반환하고, 디퓨즈 텍스처 알파를 out 으로 돌려준다.
float3 ComputeLit(VS_OUT In, out float texAlpha)
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
    float fDiffuseIdx = max(dot(In.vNormal, vL), 0.0f);

    float4 diffuseColor = g_DiffuseTextures.Sample(g_samWrap, In.vUV);

    float3 vDiffuse = diffuseColor.rgb * (g_vLightDiffuse.rgb * fDiffuseIdx);
    float3 vAmbient = diffuseColor.rgb * g_vLightAmbient.rgb;

    //그림자
    float shadowFactor = CalcShadowFactor(In.ShadowPos);
    vDiffuse *= shadowFactor;

    texAlpha = diffuseColor.a;
    return vDiffuse + vAmbient;
}

// --------------------------------------------------------
// Common Pixel Shader (Lit) — 일반/애니메이션 메쉬 공통
// --------------------------------------------------------
float4 PS_Main_Lit(VS_OUT In) : SV_TARGET
{
    float texA;
    float3 litRGB = ComputeLit(In, texA);
    return float4(litRGB, texA);
}
#endif