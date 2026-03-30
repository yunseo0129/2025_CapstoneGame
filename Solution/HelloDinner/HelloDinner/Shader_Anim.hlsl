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
    Out.vWorldPos = vWorldPos.xyz;

    Out.vNormal = normalize(mul(In.vNormal, (float3x3) matWorldAnim));
    Out.vTangent = normalize(mul(In.vTangent, (float3x3) matWorldAnim));
    
    Out.vBinormal = normalize(cross(Out.vNormal, Out.vTangent));

    Out.vUV = In.vUV;

    return Out;
}