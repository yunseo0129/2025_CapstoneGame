#include "Common.hlsli"


struct VS_IN_Instanced
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
    float3 vTangent : TANGENT;

    // Per-instance world matrix (slot 1)
    float4 vInstWorld0 : INSTWORLD0;
    float4 vInstWorld1 : INSTWORLD1;
    float4 vInstWorld2 : INSTWORLD2;
    float4 vInstWorld3 : INSTWORLD3;
};

VS_OUT VS_Main_Instanced(VS_IN_Instanced In)
{
    VS_OUT Out;

    matrix matInstWorld = matrix(In.vInstWorld0, In.vInstWorld1, In.vInstWorld2, In.vInstWorld3);

    float4 vPosWorld = mul(float4(In.vPosition, 1.f), matInstWorld);
    Out.vWorldPos = vPosWorld.xyz;

    Out.vPosition = mul(mul(vPosWorld, g_matView), g_matProj);
    Out.vNormal = normalize(mul(float4(In.vNormal, 0.f), matInstWorld)).xyz;
    Out.vTangent = normalize(mul(float4(In.vTangent, 0.f), matInstWorld)).xyz;
    Out.vUV = In.vTexcoord;
    Out.vBinormal = normalize(cross(Out.vNormal, Out.vTangent));
    
    Out.ShadowPos = mul(vPosWorld, g_matLightTransform);

    return Out;
}

// Shadow Pass용 VS

struct VS_OUT_Shadow
{
    float4 vPosition : SV_POSITION;
};

VS_OUT_Shadow VS_Main_Shadow_Instanced(VS_IN_Instanced In)
{
    VS_OUT_Shadow Out;

    matrix matInstWorld = matrix(In.vInstWorld0, In.vInstWorld1, In.vInstWorld2, In.vInstWorld3);

    float4 vPosWorld = mul(float4(In.vPosition, 1.f), matInstWorld);

    // Light view-projection — g_matLightTransform이 Bind_ShadowBuffer에서 cbTransform(b0)에 세팅됨
    Out.vPosition = mul(mul(vPosWorld, g_matView), g_matProj);

    return Out;
}
