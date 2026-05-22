#include "Common.hlsli"

cbuffer CameraBuffer : register(b0)
{
    row_major matrix g_ViewMatrix;
    row_major matrix g_ProjMatrix;
    float3 g_CamPos;
};

// GameObject root constant (slot 1) — 인스턴싱에선 무시되지만 root signature 호환을 위해 유지
cbuffer GameObjectBuffer : register(b1)
{
    matrix g_WorldMatrix_Unused;
};

struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
    float3 vTangent : TANGENT;

    // Per-instance World matrix (slot 1, semantic INSTWORLD0~3)
    float4 vInstWorld0 : INSTWORLD0;
    float4 vInstWorld1 : INSTWORLD1;
    float4 vInstWorld2 : INSTWORLD2;
    float4 vInstWorld3 : INSTWORLD3;
};

struct VS_OUT_Instanced
{
    float4 vPosition : SV_POSITION; // 화면 좌표
    float3 vWorldPos : TEXCOORD1; // 월드 좌표 (조명 계산용)
    float3 vNormal : NORMAL; // 노멀
    float2 vUV : TEXCOORD0; // UV
    float3 vBinormal : BINORMAL;
    float3 vTangent : TANGENT;
    
    float4 ShadowPos : TEXCOORD2; // 그림자 맵용 좌표 (빛의 시점에서 변환된 위치)
};


VS_OUT_Instanced VS_Main_Instanced(VS_IN In)
{
    VS_OUT_Instanced Out;

    // 인스턴스 world matrix 재구성 (row-major)
    matrix matInstWorld = matrix(In.vInstWorld0, In.vInstWorld1, In.vInstWorld2, In.vInstWorld3);

    float4 vPosWorld = mul(float4(In.vPosition, 1.f), matInstWorld);
    Out.vWorldPos = vPosWorld.xyz;

    Out.vPosition = mul(mul(vPosWorld, g_ViewMatrix), g_ProjMatrix);
    Out.vNormal = normalize(mul(float4(In.vNormal, 0.f), matInstWorld)).xyz;
    Out.vTangent = normalize(mul(float4(In.vTangent, 0.f), matInstWorld)).xyz;
    Out.vUV = In.vTexcoord;
    Out.vBinormal = normalize(cross(Out.vNormal, Out.vTangent));
    
    return Out;
}

float4 PS_Main_Insatanced(VS_OUT_Instanced In) : SV_TARGET
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

    return float4(vDiffuse + vAmbient, diffuseColor.a);
}
