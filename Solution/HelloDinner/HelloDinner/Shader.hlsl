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

struct VS_IN_ANIM
{
    float3 vPos : POSITION;
    float3 vNormal : NORMAL;
    float2 vUV : TEXCOORD0;
    float3 vTangent : TANGENT;
    uint4  vBoneIndices : BLENDINDICES; // 본 인덱스 (최대 4개)
    float4 vBoneWeights : BLENDWEIGHT;  // 본 가중치 (최대 4개)
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION; // 화면 좌표
    float3 vWorldPos : POSITION; // 월드 좌표 (조명 계산용)
    float3 vNormal : NORMAL; // 노멀
    float2 vUV : TEXCOORD0; // UV
};

struct VS_OUT_SKYBOX
{
    float4 vPosition : SV_POSITION;
    float3 vTexCoord : TEXCOORD0; // 3D 방향 벡터 (큐브맵 샘플링용)
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
    float4 vViewPos = mul(vWorldPos, g_matView);
    Out.vPosition = mul(vViewPos, g_matProj);
    
    // 3. 노멀 변환 (Local -> World)
    // 회전만 적용 (스케일링이 균등하다는 가정 하에 3x3 사용)
    Out.vNormal = normalize(mul(In.vNormal, (float3x3) g_matWorld));
    
    Out.vUV = In.vUV;
    
    return Out;
}

// 애니메이션 물체용 VS (메쉬 단위 본 매트릭스 적용)
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

// Skybox용 VS
VS_OUT_SKYBOX VS_Main_Skybox(VS_IN_STATIC In)
{
    VS_OUT_SKYBOX Out = (VS_OUT_SKYBOX) 0;

    // 카메라를 중심으로 스카이박스 배치 (이동 제거)
    float4x4 viewNoTranslation = g_matView;
    viewNoTranslation._41 = 0;
    viewNoTranslation._42 = 0;
    viewNoTranslation._43 = 0;
    viewNoTranslation._44 = 1;

    float4 vWorldPos = mul(float4(In.vPos, 1.0f), g_matWorld);
    float4 vViewPos = mul(vWorldPos, viewNoTranslation);
    Out.vPosition = mul(vViewPos, g_matProj).xyww;
    Out.vTexCoord = In.vPos;
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
    // texColor.rgb = pow(texColor.rgb, 2.2f);
    
    // clip(texColor.a - 0.001f); // 알파 테스트 (투명도 0.1 미만 픽셀 버림)
   
    return float4(texColor);
}

// Skybox PS (큐브맵 샘플링)
float4 PS_Main_Skybox(VS_OUT_SKYBOX In) : SV_TARGET
{
    float4 texColor = g_TexCube.Sample(g_samClamp, In.vTexCoord);
    return texColor;
}