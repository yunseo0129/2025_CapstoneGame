// =====================================================================
//  Shader_UI.hlsl
//  2D UI 전용 셰이더 (조명 X / 깊이 X / 알파 블렌딩)
//
//  설계:
//   - b1(GameObject 슬롯, 16 float): "이미 NDC로 변환된" world 행렬.
//     (CPU 측 CUIObject 에서 픽셀->NDC 계산)
//   - b4(UIColor 슬롯, 4 float): 색상 틴트 + 알파 (rgba).
//     · 텍스처 패널 : 텍스처 색에 곱한다.
//     · 단색 패널   : 텍스처를 흰색(1)으로 보고 색만 출력 -> g_bUseTexture=0
//   - t0(Diffuse): 텍스처 한 장. (단색 패널은 바인딩 안 함)
//   - 정점은 단위 사각형 (0~1). world 가 NDC 사각형으로 보낸다.
// =====================================================================

#include "Common.hlsli"   // g_matWorld(b1), g_DiffuseTextures(t0), g_samClamp(s1)

// b4 : UI 색상 (틴트 rgb + 알파 a). w 성분 .a 사용.
//  추가로 색상 .a 와 별개로, x>=0 이면 텍스처 사용 / x<0 이면 단색 으로 쓰는
//  플래그가 필요하지만, 분기 단순화를 위해 별도 상수(g_UIParam)로 받는다.
cbuffer cbUIColor : register(b4)
{
    float4 g_vUIColor; // rgba 틴트
    float4 g_vUIParam; // x: useTexture(1=텍스처, 0=단색), yzw 예약
};

struct VS_IN_UI
{
    float3 vPos : POSITION;
    float2 vUV : TEXCOORD0;
};

struct VS_OUT_UI
{
    float4 vPosition : SV_POSITION;
    float2 vUV : TEXCOORD0;
};

VS_OUT_UI VS_Main_UI(VS_IN_UI In)
{
    VS_OUT_UI Out = (VS_OUT_UI) 0;
    Out.vPosition = mul(float4(In.vPos, 1.0f), g_matWorld);
    Out.vUV = In.vUV;
    return Out;
}

float4 PS_Main_UI(VS_OUT_UI In) : SV_TARGET
{
    float4 baseColor = float4(1.f, 1.f, 1.f, 1.f);

    // 텍스처 사용 시에만 샘플 (단색 패널은 흰색 유지)
    if (g_vUIParam.x > 0.5f)
        baseColor = g_DiffuseTextures.Sample(g_samClamp, In.vUV);

    float4 outColor = baseColor * g_vUIColor;

    if (outColor.a <= 0.001f)
        discard;

    return outColor;
}
