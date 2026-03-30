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

    // 4번째 본의 가중치 계산 (정점의 모든 가중치 합은 1이어야 함)
    float fWeightW = 1.f - (In.vBoneWeights.x + In.vBoneWeights.y + In.vBoneWeights.z);

    // [수정 완료] 인덱스(Indices)가 아닌 가중치(Weights)를 곱하도록 변경
    matrix BoneMatrix = g_BoneMatrices[In.vBoneIndices.x] * In.vBoneWeights.x +
                        g_BoneMatrices[In.vBoneIndices.y] * In.vBoneWeights.y +
                        g_BoneMatrices[In.vBoneIndices.z] * In.vBoneWeights.z +
                        g_BoneMatrices[In.vBoneIndices.w] * fWeightW;
    
    // 1. [Local -> Skinned Local] 변환
    // 뼈대의 애니메이션이 적용된 로컬 좌표를 구합니다.
    float4 vSkinnedPos = mul(float4(In.vPos, 1.0f), BoneMatrix);
    float3 vSkinnedNormal = mul(In.vNormal, (float3x3) BoneMatrix);

    // 2. [Skinned Local -> World] 변환
    float4 vWorldPos = mul(vSkinnedPos, g_matWorld);
    Out.vWorldPos = vWorldPos.xyz;

    // 3. [World -> Clip] 변환
    float4 vViewPos = mul(vWorldPos, g_matView);
    Out.vPosition = mul(vViewPos, g_matProj);

    // 4. 노멀 변환 (Skinned -> World)
    Out.vNormal = normalize(mul(vSkinnedNormal, (float3x3) g_matWorld));

    Out.vUV = In.vUV;

    return Out;
}