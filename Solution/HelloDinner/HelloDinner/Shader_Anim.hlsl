#include "Common.hlsli"

struct VS_IN_ANIM
{
    float3 vPos : POSITION;
    float3 vNormal : NORMAL;
    float2 vUV : TEXCOORD0;
    float3 vTangent : TANGENT;
    uint4 vBoneIndices : BLENDINDICES;
    float4 vBoneWeights : BLENDWEIGHT;
};

// 애니메이션 물체용 VS
VS_OUT VS_Main_Anim(VS_IN_ANIM In)
{
    VS_OUT Out = (VS_OUT) 0;

    float4x4 matBone = g_BoneMatrices[In.vBoneIndices.x] * In.vBoneWeights.x;
    matBone += g_BoneMatrices[In.vBoneIndices.y] * In.vBoneWeights.y;
    matBone += g_BoneMatrices[In.vBoneIndices.z] * In.vBoneWeights.z;
    matBone += g_BoneMatrices[In.vBoneIndices.w] * In.vBoneWeights.w;

    float4x4 matWorldAnim = mul(matBone, g_matWorld);

    float4 vWorldPos = mul(float4(In.vPos, 1.f), matWorldAnim);
    
    Out.vPosition = mul(vWorldPos, mul(g_matView, g_matProj));
  
    Out.ShadowPos = mul(vWorldPos, g_matLightTransform);
    
    Out.vWorldPos = vWorldPos.xyz;

    Out.vNormal = normalize(mul(In.vNormal, (float3x3) matWorldAnim));
    Out.vTangent = normalize(mul(In.vTangent, (float3x3) matWorldAnim));
    
    Out.vBinormal = normalize(cross(Out.vNormal, Out.vTangent));

    Out.vUV = In.vUV;

    return Out;
}

struct VS_SHADOW_OUT {
    float4 Pos : SV_POSITION;
};

VS_SHADOW_OUT VS_Main_Shadow(VS_IN_ANIM In)
{
    VS_SHADOW_OUT vout = (VS_SHADOW_OUT) 0;

    // 스킨닝 연산 (정점 위치 = 본 행렬 * 가중치)
    float4x4 matBone = g_BoneMatrices[In.vBoneIndices.x] * In.vBoneWeights.x;
    matBone += g_BoneMatrices[In.vBoneIndices.y] * In.vBoneWeights.y;
    matBone += g_BoneMatrices[In.vBoneIndices.z] * In.vBoneWeights.z;
    matBone += g_BoneMatrices[In.vBoneIndices.w] * In.vBoneWeights.w;

    float4x4 matWorldAnim = mul(matBone, g_matWorld);

    float4 vWorldPos = mul(float4(In.vPos, 1.f), matWorldAnim);
    
    // 빛의 시점으로 변환
    float4 posV = mul(vWorldPos, g_matView);
    vout.Pos = mul(posV, g_matProj);

    return vout;
}
