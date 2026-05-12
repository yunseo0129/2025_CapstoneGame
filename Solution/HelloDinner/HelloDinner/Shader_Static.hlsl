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

    Out.ShadowPos = mul(vWorldPos, g_matLightTransform);
    
    float3x3 matWorld3x3 = (float3x3) g_matWorld;

    Out.vNormal = normalize(mul(In.vNormal, matWorld3x3));
    Out.vTangent = normalize(mul(In.vTangent, matWorld3x3));
    
    Out.vBinormal = normalize(cross(Out.vNormal, Out.vTangent));
    
    Out.vUV = In.vUV;
    
    return Out;
}

struct VS_SHADOW_OUT {
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0; // 알파 클리핑용 텍스처 좌표
};

VS_SHADOW_OUT VS_Main_Shadow(VS_IN_STATIC vin)
{
    VS_SHADOW_OUT vout = (VS_SHADOW_OUT) 0;

    // 1. Local Space -> World Space
    float4 posW = mul(float4(vin.vPos, 1.0f), g_matWorld);

    // 2. World Space -> Light View Space (빛의 시점)
    float4 posV = mul(posW, g_matView);

    // 3. Light View Space -> Light Projection Space (빛의 화면 공간)
    vout.Pos = mul(posV, g_matProj);

    // 텍스처 좌표 전달 (알파 클리핑용)
    vout.Tex = vin.vUV;

    return vout;
}
