#include "Common.hlsli"

struct VS_IN_STATIC
{
    float3 vPos : POSITION;
    float3 vNormal : NORMAL;
    float2 vUV : TEXCOORD0;
    float3 vTangent : TANGENT;
};

struct VS_OUT_SKYBOX
{
    float4 vPosition : SV_POSITION;
    float3 vTexCoord : TEXCOORD0;
};

// Skybox용 VS
VS_OUT_SKYBOX VS_Main_Skybox(VS_IN_STATIC In)
{
    VS_OUT_SKYBOX Out = (VS_OUT_SKYBOX) 0;

    float4x4 viewNoTranslation = g_matView;
    viewNoTranslation._41 = 0;
    viewNoTranslation._42 = 0;
    viewNoTranslation._43 = 0;
    viewNoTranslation._44 = 1;

    float4 vWorldPos = mul(float4(In.vPos, 1.0f), g_matWorld);
    float4 vViewPos = mul(vWorldPos, viewNoTranslation);
    
    // .xyww를 통해 Z값을 무조건 최대 깊이(1.0)로 만듦
    Out.vPosition = mul(vViewPos, g_matProj).xyww;
    Out.vTexCoord = In.vPos;
    
    return Out;
}

// Skybox PS 
float4 PS_Main_Skybox(VS_OUT_SKYBOX In) : SV_TARGET
{
    float4 texColor = g_DiffuseTexCube.Sample(g_samClamp, In.vTexCoord);
    return texColor;
}