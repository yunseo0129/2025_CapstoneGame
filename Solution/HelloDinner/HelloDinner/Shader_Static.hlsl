#include "Common.hlsli"

// [UV] 팔레트 crop UV 재매핑 상수 (맵 전용).
//   Loader/CModel 이 머티리얼 슬롯별로 b5 에 push: finalUV = meshUV * uvScale + uvOffset.
//   맵이 아닌 모델은 이 셰이더를 쓰지 않으므로 영향 없음.
//   기본값(scale=1, offset=0)이면 변환 없음과 동일.
cbuffer cbMapUV : register(b5)
{
    float2 g_uvOffset;
    float2 g_uvScale;
    // [투명] 머티리얼 슬롯별 알파/표면타입 (맵 전용, 로더/CModel 이 push).
    //   비-맵/불투명은 alpha=1, surfaceType=0  영향 없음.
    float g_fMatAlpha; // baseColor.a (1.0 = 불투명)
    float g_fAlphaCutoff; // 컷아웃 임계값 (현재 미사용, Kitchen_alpha 대비)
    float g_fSurfaceType; // 0=Opaque, 1=Transparent, 2=Cutout
    float g_fPadMapUV;
};

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
    
    Out.vUV = In.vUV * g_uvScale + g_uvOffset;
    //Out.vUV = In.vUV;
    
    return Out;
}

struct VS_SHADOW_OUT
{
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

    // 텍스처 좌표 전달 (알파 클리핑용) — 동일 crop 재매핑 적용
    vout.Tex = vin.vUV * g_uvScale + g_uvOffset;

    return vout;
}

// [투명] 유리 전용 PS. 단순 알파블렌딩은 '색 입힌 투명필름' 같아 유리 느낌이 안 난다.
//   유리답게 = Fresnel(가장자리 불투명·밝음) + Specular(하이라이트 글린트) + 바닥 불투명도.
float4 PS_Glass(VS_OUT In) : SV_TARGET
{
    // ── 튜닝 파라미터 (값만 바꿔 느낌 조절) ──────────────────────
    const float FRESNEL_POW = 3.0; // 작을수록 테두리 두꺼움
    const float FRESNEL_GAIN = 0.8; // 가장자리 불투명 강도
    const float SPEC_POW = 120.0; // 클수록 하이라이트 좁고 날카로움
    const float SPEC_GAIN = 1.2; // 하이라이트 밝기/불투명 강도
    const float ALPHA_FLOOR = 0.4; // 정면 최소 불투명도(표면이 항상 살짝 보이게)
    // ──────────────────────────────────────────────────────────

    float texA;
    float3 litRGB = ComputeLit(In, texA);

    float3 N = normalize(In.vNormal);
    float3 V = normalize(g_vCamPosWS - In.vWorldPos); // 표면 → 카메라

    // Fresnel: 정면=0(투명) → 가장자리=1(불투명/밝음). 유리 테두리.
    float fresnel = pow(1.0 - saturate(dot(N, V)), FRESNEL_POW);

    // Specular(Blinn-Phong): 좁고 밝은 글린트.
    float3 L = normalize(-g_vLightDir.xyz);
    float3 H = normalize(L + V);
    float spec = pow(saturate(dot(N, H)), SPEC_POW);

    // 알파 = 베이스(머티리얼, 바닥값 보장) + 가장자리 + 하이라이트.
    float baseA = max(saturate(texA * g_fMatAlpha), ALPHA_FLOOR);
    float alpha = saturate(baseA + fresnel * FRESNEL_GAIN + spec * SPEC_GAIN);

    // 색에 하이라이트를 흰색으로 더해 글린트.
    float3 color = litRGB + spec.xxx * SPEC_GAIN;

    return float4(color, alpha);
}