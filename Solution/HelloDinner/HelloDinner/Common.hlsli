// 중복 정의 방지
#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

// --------------------------------------------------------
//  Constant Buffers (상수 버퍼)
// --------------------------------------------------------
// b0 : camera 행렬 정보 (매 프레임 갱신)
cbuffer cbTransform : register(b0)
{
    row_major matrix g_matView; // 뷰 행렬
    row_major matrix g_matProj; // 투영 행렬
    float3 g_vCamPosWS; // 월드 공간 카메라 위치
};
// b1 : Object 행렬 정보 (오브젝트마다 갱신)
cbuffer cbObject : register(b1)
{
    row_major matrix g_matWorld; // 월드 행렬
};

// b2 : Bone Matrices (애니메이션 메쉬 전용)
cbuffer cbBoneMatrices : register(b2)
{
    row_major matrix g_BoneMatrices[512];
};


// ------------------------------------------------
// Textures & Samplers
// ------------------------------------------------
// t0 : 텍스처 정보
Texture2D g_Textures : register(t0);

// t0, space1 : 큐브맵 텍스처 (Skybox 전용, 별도 space 사용)
TextureCube g_TexCube : register(t0);

// Static Samplers
SamplerState g_samWrap : register(s0); // 일반 3D
SamplerState g_samClamp : register(s1); // UI
SamplerState g_samPoint : register(s2); // 도트
SamplerState g_samAnisotropic : register(s3); // 지형
// ------------------------------------------------

#endif