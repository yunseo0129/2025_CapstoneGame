// =============================================================================
//  Shader_Fracture.hlsl  -  파괴 조각(fracture) 전용 렌더 (VS/PS)
//  - 기존 애니메이션 스키닝(VS_Main_Anim)과 동일 원리지만, 본 매트릭스를
//    글로벌 CB(g_BoneMatrices) 대신 compute 가 채운 StructuredBuffer(SRV)에서 룩업한다.
//    -> 글로벌 RS/본 CB 경로 무수정. 파티클처럼 자체 RS/PSO 를 갖는 독립 모듈.
//  - rigid: 각 정점은 단일 본(=조각)에 100% weight -> 가중합 없이 BlendIndices.x 한 번 룩업.
//  - 본(=조각) 매트릭스 = T(-centerBind)·R(quat)·T(pos)  (compute 가 합성, 무게중심 기준 강체).
//  - 레지스터: b0(VP+World) / t0(chunk 매트릭스 SRV) t1(diffuse) / s0(샘플러)
// =============================================================================

cbuffer FractureFrameCB : register(b0)
{
    row_major matrix g_matWorld; // 벽 배치(파괴돼도 고정). chunk 변환 후 여기에 곱해 월드로.
    row_major matrix g_matView;
    row_major matrix g_matProj;
    float3 g_vLightDir; // 간단 디렉셔널 라이트(정규화). 조명 통합 전 임시.
    float g_fAmbient; // 앰비언트 하한(0~1)
};

// compute(Shader_Fracture_Compute.hlsl)가 매 프레임 채우는 조각별 본 매트릭스.
//  인덱스 = 정점의 BlendIndices.x (= chunkID, 메시 로컬 본 인덱스).
StructuredBuffer<float4x4> g_ChunkMatrices : register(t0);

Texture2D g_DiffuseTex : register(t1);
SamplerState g_Samp : register(s0);

struct VS_IN
{
    float3 vPos : POSITION;
    float3 vNormal : NORMAL;
    float2 vUV : TEXCOORD0;
    float3 vTangent : TANGENT;
    uint4 vBoneIndices : BLENDINDICES; // .x = chunkID (rigid: 단일 본)
    float4 vBoneWeights : BLENDWEIGHT; // .x = 1.0 (나머지 0)
};

struct VS_OUT
{
    float4 vPos : SV_POSITION;
    float3 vNormal : NORMAL;
    float2 vUV : TEXCOORD0;
    float3 vWorldPos : TEXCOORD1;
};

VS_OUT VS_Fracture(VS_IN In)
{
    VS_OUT Out = (VS_OUT) 0;

    // rigid skinning: 단일 본 100% 이므로 가중합 불필요. chunkID 로 본 매트릭스 1회 룩업.
    //  (안전을 위해 weight.x 를 곱해도 되지만 1.0 이라 생략)
    float4x4 matBone = g_ChunkMatrices[In.vBoneIndices.x];

    // 본(조각) 변환 -> 벽 배치(g_matWorld) -> 월드.  (VS_Main_Anim 과 동일한 mul 순서)
    float4x4 matWorld = mul(matBone, g_matWorld);

    float4 vWorldPos = mul(float4(In.vPos, 1.0f), matWorld);
    Out.vWorldPos = vWorldPos.xyz;
    Out.vPos = mul(vWorldPos, mul(g_matView, g_matProj));

    Out.vNormal = normalize(mul(In.vNormal, (float3x3) matWorld));
    Out.vUV = In.vUV;

    return Out;
}

float4 PS_Fracture(VS_OUT In) : SV_TARGET
{
    float4 tex = g_DiffuseTex.Sample(g_Samp, In.vUV);

    // 간단 디렉셔널 라이팅(엔진 조명 통합 전 임시). 조각 단면도 음영이 생겨 입체감을 준다.
    float ndl = saturate(dot(normalize(In.vNormal), -g_vLightDir));
    float lit = max(ndl, g_fAmbient);

    return float4(tex.rgb * lit, tex.a);
}