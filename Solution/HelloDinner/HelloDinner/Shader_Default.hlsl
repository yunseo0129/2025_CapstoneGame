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

    // 1. [Local -> World] 변환
    float4 vWorldPos = mul(float4(In.vPos, 1.0f), g_matWorld);
    Out.vWorldPos = vWorldPos.xyz;

    // 2. [World -> Clip] 변환 (카메라 적용)
    float4 vViewPos = mul(vWorldPos, g_matView);
    Out.vPosition = mul(vViewPos, g_matProj);
    
    // 3. 노멀 변환 (Local -> World)
    Out.vNormal = normalize(mul(In.vNormal, (float3x3) g_matWorld));
    
    Out.vUV = In.vUV;
    
    return Out;
}