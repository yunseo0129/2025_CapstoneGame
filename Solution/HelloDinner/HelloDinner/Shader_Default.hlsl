#include "Common.hlsli"

struct VS_IN_STATIC
{
    float3 vPos : POSITION;
    float3 vNormal : NORMAL;
    float2 vUV : TEXCOORD0;
    float3 vTangent : TANGENT;
};

// 일반 물체용 VS
VS_OUT VS_Main_Static(VS_IN_STATIC In)
{
    VS_OUT Out = (VS_OUT) 0;

    float4 vWorldPos = mul(float4(In.vPos, 1.0f), g_matWorld);
    Out.vWorldPos = vWorldPos.xyz;

    float4 vViewPos = mul(vWorldPos, g_matView);
    Out.vPosition = mul(vViewPos, g_matProj);

    float3x3 matWorld3x3 = (float3x3) g_matWorld;

    Out.vNormal = normalize(mul(In.vNormal, matWorld3x3));
    Out.vTangent = normalize(mul(In.vTangent, matWorld3x3));
    
    Out.vBinormal = normalize(cross(Out.vNormal, Out.vTangent));
    
    Out.vUV = In.vUV;
    
    return Out;
}