// ============================================================================
//  Shader_Particle.hlsl
//  GPU 컴퓨트 파티클 - 1단계 렌더 패스
//  - IA(정점버퍼/입력레이아웃) 없음. VS가 SV_VertexID로 카메라 정렬 빌보드 쿼드를
//    합성하고, SV_InstanceID로 StructuredBuffer<Particle>(t0)를 읽어 인스턴스 N개를 그림.
//  - 호출: DrawInstanced(6, aliveCount, 0, 0)  (TRIANGLELIST)
//  - 전용 루트시그: b0 = ParticleFrameCB(CBV), t0 = g_Particles(SRV table, VS 가시)
// ============================================================================

// C++ CParticle_System::PARTICLE 와 동일 레이아웃 (48 bytes, 16B 정렬)
struct Particle
{
    float3 pos;
    float life;
    float3 vel;
    float size;
    float4 color;
};

// C++ CParticle_System::FRAME_CB 와 동일 레이아웃 (256 정렬)
cbuffer ParticleFrameCB : register(b0)
{
    row_major matrix g_matView;
    row_major matrix g_matProj;
    float3 g_vCamRight;
    float g_fDeltaTime; // 2단계 컴퓨트에서 사용
    float3 g_vCamUp;
    uint g_uMaxParticles;
};

StructuredBuffer<Particle> g_Particles : register(t0);

struct VS_OUT
{
    float4 vPos : SV_POSITION;
    float2 vUV : TEXCOORD0;
    float4 vColor : COLOR0;
};

// 쿼드 두 삼각형 (TRIANGLELIST, 6 정점)
static const float2 g_Corners[6] =
{
    float2(-1.0f, -1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f),
    float2(-1.0f, -1.0f), float2(1.0f, 1.0f), float2(-1.0f, 1.0f)
};

VS_OUT VS_Particle(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VS_OUT Out;

    Particle p = g_Particles[iid];

    // 죽은 입자(life <= 0)는 size 0 -> 영(0)면적 쿼드(디제너릿) -> 픽셀 생성 안 됨.
    // 2단계에서 컴퓨트가 life를 감소시키면 자동으로 사라진다.
    float sz = p.size * (p.life > 0.0f ? 1.0f : 0.0f);

    float2 c = g_Corners[vid];

    // 카메라 정렬 빌보드: 입자 위치 + (코너 * 크기)를 카메라 right/up(월드) 축으로 펼침
    float3 worldPos = p.pos
                    + (c.x * sz) * g_vCamRight
                    + (c.y * sz) * g_vCamUp;

    float4 viewPos = mul(float4(worldPos, 1.0f), g_matView);
    Out.vPos = mul(viewPos, g_matProj);
    Out.vUV = c * 0.5f + 0.5f; // [-1,1] -> [0,1]
    Out.vColor = p.color;

    return Out;
}

float4 PS_Particle(VS_OUT In) : SV_TARGET
{
    // 원형 마스크 + 가장자리 소프트 페이드 (점이 부드러운 원으로 보이게)
    float2 d = In.vUV - 0.5f;
    float r2 = dot(d, d); // 중심에서 거리^2, 반지름 0.5 -> r2 0.25
    if (r2 > 0.25f)
        discard;

    float a = saturate(1.0f - r2 * 4.0f);
    return float4(In.vColor.rgb, In.vColor.a * a);
}

// ============================================================================
//  2단계: 컴퓨트 업데이트 패스
//  - 같은 풀 버퍼를 RWStructuredBuffer(u0)로 바인딩해 매 프레임 갱신.
//  - 레지스터 분리(VS의 SRV는 t0/CB는 b0, 여기 UAV는 u0/CB는 b1)로 한 파일에 공존.
//  - 루트 UAV(SetComputeRootUnorderedAccessView) + 루트 32비트 상수(b1)로 바인딩.
// ============================================================================
RWStructuredBuffer<Particle> g_RWParticles : register(u0);

// 주의: HLSL cbuffer 멤버는 전역 스코프라, VS의 ParticleFrameCB(b0) 멤버와 이름이 겹치면
//       재정의 오류가 난다. dt/풀크기는 g_cs* 접두사로 구분한다.
cbuffer ParticleComputeCB : register(b1)
{
    float g_csDeltaTime; // offset 0
    uint g_csMaxParticles; // offset 4
    float g_fGravityX; // offset 8   (float3를 쓰면 16정렬로 어긋나 스칼라 3개로 분리)
    float g_fGravityY; // offset 12
    float g_fGravityZ; // offset 16
};

[numthreads(256, 1, 1)]
void CS_Update(uint3 dtid : SV_DispatchThreadID)
{
    uint i = dtid.x;
    if (i >= g_csMaxParticles)
        return;

    Particle p = g_RWParticles[i];

    // 죽은 입자는 갱신하지 않음(렌더에서 size 0으로 사라진다)
    if (p.life <= 0.0f)
        return;

    float3 g = float3(g_fGravityX, g_fGravityY, g_fGravityZ);

    p.vel += g * g_csDeltaTime; // 중력 가속
    p.pos += p.vel * g_csDeltaTime; // 위치 적분
    p.life -= g_csDeltaTime; // 수명 감소

    g_RWParticles[i] = p;
}
