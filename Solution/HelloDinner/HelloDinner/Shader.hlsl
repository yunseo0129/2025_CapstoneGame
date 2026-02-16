#include "Common.hlsli"

// --------------------------------------------------------
// Input Structures
// --------------------------------------------------------
struct VS_IN_STATIC
{
    float3 vPos : POSITION;
    float3 vNormal : NORMAL;
    float2 vUV : TEXCOORD0;
    float3 vTangent : TANGENT;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION; // 화면 좌표
    float3 vWorldPos : POSITION; // 월드 좌표 (조명 계산용)
    float3 vNormal : NORMAL; // 노멀
    float2 vUV : TEXCOORD0; // UV
};

// --------------------------------------------------------
// Vertex Shaders
// --------------------------------------------------------
// 일반 물체용 VS
VS_OUT VS_Main_Static(VS_IN_STATIC In)
{
    VS_OUT Out = (VS_OUT) 0;

    // 1. [Local -> World] 변환
    // g_matWorld는 cbObject(b1)에서 가져옴
    float4 vWorldPos = mul(float4(In.vPos, 1.0f), g_matWorld);
    
    // 픽셀 셰이더에서 조명 계산 등을 위해 월드 좌표 저장
    Out.vWorldPos = vWorldPos.xyz;

    // 2. [World -> Clip] 변환 (카메라 적용)
    // 월드 좌표에 ViewProj(b0)를 곱함
    Out.vPosition = mul(vWorldPos, mul(g_matView, g_matProj));
    
    // 3. 노멀 변환 (Local -> World)
    // 회전만 적용 (스케일링이 균등하다는 가정 하에 3x3 사용)
    Out.vNormal = normalize(mul(In.vNormal, (float3x3) g_matWorld));
    
    Out.vUV = In.vUV;
    
    return Out;
}

// --------------------------------------------------------
// Pixel Shaders
// --------------------------------------------------------
// 조명 적용 PS (Lit)
float4 PS_Main_Lit(VS_OUT In) : SV_TARGET
{

    float4 texColor = g_Textures.Sample(g_samWrap, In.vUV);
        
   // sRGB -> Linear 변환 (감마 보정)
   texColor.rgb = pow(texColor.rgb, 2.2f);
    
    
    return float4(texColor);
}