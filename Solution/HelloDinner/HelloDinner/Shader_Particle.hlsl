// ============================================================================
//  Shader_Particle.hlsl  -  GPU 컴퓨트 파티클 (렌더 VS/PS + 업데이트/컴팩션 CS)
//  - 렌더: IA 없음. VS가 SV_VertexID로 빌보드 쿼드를 합성하고, SV_InstanceID로
//          alive목록(t1) -> 입자(t0) 인덱싱. ExecuteIndirect 가 alive 수만 그림.
//  - 컴퓨트(CS_Update): 입자(u0) 거동 갱신 + 살아있는 입자를 alive목록(u2)에 append 하고
//          간접 드로우 인자(u1)의 InstanceCount 를 InterlockedAdd 로 집계.
//  [7b] 외형 다양성: Particle.packed 에 슬라이스/회전/플립/절차적 플래그를 패킹(CPU Emit 이 채움).
//       VS 가 언패킹해 빌보드를 회전시키고 UV 를 플립하며, 슬라이스/절차적여부를 PS 로 전달한다.
//       흙먼지(proc=1)는 텍스처 미사용 -> PS 가 소프트 원을 GPU 로 직접 그린다(케첩/파편만 텍스처 샘플).
//  - 레지스터: VS = b0(카메라) t0(입자) t1(alive) / PS = t2(텍스처배열) s0(샘플러) / CS = b1(상수) u0(입자) u1(args) u2(alive) b2(거동)
// ============================================================================

// 거동 테이블(b2) 크기 (C++ CParticle_System::TYPE_END 와 반드시 동일).
//  외형 슬라이스 수(6)와는 별개 - 슬라이스 인덱싱은 packed 가 직접 들고 있어 셰이더 상수 불필요.
#define PARTICLE_TYPE_COUNT 3

// C++ CParticle_System::PARTICLE 와 동일 레이아웃 (64 bytes, 16B 정렬)
struct Particle
{
    float3 pos;
    float life;
    float3 vel;
    float size; // 현재(렌더) 크기 - CS가 수명 곡선으로 갱신
    float4 color; // 현재(렌더) 색  - CS가 수명 곡선으로 갱신
    uint type; // PARTICLE_TYPE (거동용)
    float maxLife; // 방출 시점 수명 (age = 1 - life/maxLife)
    float spawnSize; // 방출 기준 크기(불변) - 크기 곡선이 여기에 곱
    uint packed; // [7b] 외형: [0..7] slice | [8..15] angleQ | [16] flipU | [17] flipV | [18] proc
};

// C++ CParticle_System::FRAME_CB 와 동일 레이아웃 (256 정렬)
cbuffer ParticleFrameCB : register(b0)
{
    row_major matrix g_matView;
    row_major matrix g_matProj;
    float3 g_vCamRight;
    float g_fDeltaTime;
    float3 g_vCamUp;
    uint g_uMaxParticles;
};

StructuredBuffer<Particle> g_Particles : register(t0);
StructuredBuffer<uint> g_AliveList : register(t1); // 살아있는 입자 인덱스 목록

// [7b] 외형 텍스처 배열(케첩 = 슬라이스 0~1 / 파편 = 슬라이스 2~5). proc 입자(흙먼지)는 샘플하지 않는다.
//  모양/디테일은 텍스처가, 밝기/수명 페이드는 vColor(거동 색곡선: 거의 흰색 + 알파)가 결정 -> 이중 채색 방지.
Texture2DArray g_ParticleTex : register(t2);
SamplerState g_Samp : register(s0); // 정적 샘플러(linear/clamp)

struct VS_OUT
{
    float4 vPos : SV_POSITION;
    float2 vUV : TEXCOORD0;
    float4 vColor : COLOR0;
    nointerpolation uint uSprite : TEXCOORD1; // [7b] 텍스처 배열 슬라이스(쿼드 6정점 동일값)
    nointerpolation uint uProc : TEXCOORD2; // [7b] 1=절차적(흙먼지, GPU 드로우) 0=텍스처
};

// 쿼드 두 삼각형 (TRIANGLELIST, 6 정점)
static const float2 g_Corners[6] =
{
    float2(-1.0f, -1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f),
    float2(-1.0f, -1.0f), float2(1.0f, 1.0f), float2(-1.0f, 1.0f)
};

static const float TWO_PI = 6.28318530718f;

VS_OUT VS_Particle(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VS_OUT Out;

    // ExecuteIndirect 가 alive 수만큼 인스턴스를 그린다. iid -> alive목록 -> 실제 입자 인덱스.
    uint particleIndex = g_AliveList[iid];
    Particle p = g_Particles[particleIndex];

    // 안전망: 컴팩션으로 alive만 그려지지만 혹시 모를 경우 size 0 디제너릿.
    float sz = p.size * (p.life > 0.0f ? 1.0f : 0.0f);

    // [7b] 외형 언패킹
    uint packed = p.packed;
    uint uSprite = packed & 0xFFu;
    uint angleQ = (packed >> 8) & 0xFFu;
    uint flipU = (packed >> 16) & 0x1u;
    uint flipV = (packed >> 17) & 0x1u;
    uint proc = (packed >> 18) & 0x1u;

    // 회전 각 (angleQ/256 * 2pi)
    float angle = (float) angleQ * (TWO_PI / 155.0f);
    float sinA, cosA;
    sincos(angle, sinA, cosA);

    float2 q = g_Corners[vid];
    // 빌보드 평면 내에서 코너를 회전(각진 파편/케첩에 효과적, 원형 먼지엔 무해).
    float2 qr = float2(q.x * cosA - q.y * sinA,
                       q.x * sinA + q.y * cosA);

    // 카메라 정렬 빌보드: 입자 위치 + (회전된 코너 * 크기)를 카메라 right/up(월드) 축으로 펼침
    float3 worldPos = p.pos
                    + (qr.x * sz) * g_vCamRight
                    + (qr.y * sz) * g_vCamUp;

    float4 viewPos = mul(float4(worldPos, 1.0f), g_matView);
    Out.vPos = mul(viewPos, g_matProj);

    // UV 는 회전 전 원래 코너로 계산(빌보드만 회전 -> 스프라이트가 회전돼 보임). 이후 플립.
    float2 uv = q * 0.5f + 0.5f; // [-1,1] -> [0,1]
    if (flipU)
        uv.x = 1.0f - uv.x;
    if (flipV)
        uv.y = 1.0f - uv.y;
    Out.vUV = uv;

    Out.vColor = p.color;
    Out.uSprite = uSprite;
    Out.uProc = proc;

    return Out;
}

float4 PS_Particle(VS_OUT In) : SV_TARGET
{
    float4 outC;

    if (In.uProc != 0u)
    {
        // [7b] 흙먼지: GPU 절차적 소프트 원. 모양은 여기서 만들고, 색/알파 페이드는 vColor(거동 곡선)가 직접 표현.
        float2 d = In.vUV * 2.0f - 1.0f; // [-1,1]
        float r = length(d);
        float a = saturate(1.0f - r);
        a = a * a; // 가장자리 더 부드럽게(먼지 뭉게구름 느낌)
        outC = float4(In.vColor.rgb, In.vColor.a * a);
    }
    else
    {
        // [7b] 케첩/파편: 텍스처 배열을 슬라이스로 샘플. 텍스처가 색/디테일, vColor 는 밝기/페이드(거의 흰색+알파곡선).
        float4 tex = g_ParticleTex.Sample(g_Samp, float3(In.vUV, (float) In.uSprite));
        outC = float4(tex.rgb * In.vColor.rgb, tex.a * In.vColor.a);
    }

    if (outC.a <= 0.003f) // 완전 투명 픽셀 컬링(오버드로 절감)
        discard;

    return outC;
}

// ============================================================================
//  컴퓨트 업데이트 패스 (거동: iType 기반. 외형 packed 는 읽기/보존만, 갱신 안 함)
//  - 같은 풀 버퍼를 RWStructuredBuffer(u0)로 바인딩해 매 프레임 갱신.
//  - 루트 UAV(SetComputeRootUnorderedAccessView) + 루트 32비트 상수(b1) + 거동 CBV(b2)로 바인딩.
// ============================================================================
RWStructuredBuffer<Particle> g_RWParticles : register(u0);
RWByteAddressBuffer g_DrawArgs : register(u1); // 간접 인자 (InstanceCount @ byte 4)
RWStructuredBuffer<uint> g_RWAliveList : register(u2); // alive 인덱스 목록

cbuffer ParticleComputeCB : register(b1)
{
    float g_csDeltaTime; // offset 0
    uint g_csMaxParticles; // offset 4
    float g_fGravityX; // offset 8
    float g_fGravityY; // offset 12
    float g_fGravityZ; // offset 16
};

// [6단계] 종류별 거동 테이블 (C++ TYPE_BEHAVIOR 와 동일, 요소당 48 bytes). b2 루트 CBV.
struct TypeBehavior
{
    float gravityScale; // 0
    float drag; // 4
    float startSize; // 8
    float endSize; // 12 -> 16
    float4 startColor; // 16 -> 32
    float4 endColor; // 32 -> 48
};
cbuffer ParticleTypeCB : register(b2)
{
    TypeBehavior g_TypeBehaviors[PARTICLE_TYPE_COUNT];
};

[numthreads(256, 1, 1)]
void CS_Update(uint3 dtid : SV_DispatchThreadID)
{
    uint i = dtid.x;
    if (i >= g_csMaxParticles)
        return;

    Particle p = g_RWParticles[i];

    // 죽은 입자는 갱신/추가하지 않음
    if (p.life <= 0.0f)
        return;

    // 종류별 거동 테이블 (인덱스 안전 클램프)
    uint t = min(p.type, (uint) (PARTICLE_TYPE_COUNT - 1));
    TypeBehavior tb = g_TypeBehaviors[t];

    float dt = g_csDeltaTime;

    // 중력(종류별 배율) + 감쇠(공기저항). g=(0,-G,0) 이므로 gravityScale 이 클수록 무겁게 떨어진다.
    float3 g = float3(g_fGravityX, g_fGravityY, g_fGravityZ);
    p.vel += g * tb.gravityScale * dt;
    p.vel *= saturate(1.0f - tb.drag * dt);

    // 위치 적분 / 수명 감소
    p.pos += p.vel * dt;
    p.life -= dt;

    // 수명 비율 (0=갓 태어남, 1=소멸 직전). maxLife 0 보호.
    float age = saturate(1.0f - p.life / max(p.maxLife, 1e-4f));

    // 크기/색 수명 곡선 : 크기는 방출 기준 크기(spawnSize)에 곱, 색은 보간(a 로 페이드)
    p.size = lerp(tb.startSize, tb.endSize, age) * p.spawnSize;
    p.color = lerp(tb.startColor, tb.endColor, age);

    g_RWParticles[i] = p; // [7b] packed(외형)는 손대지 않으므로 그대로 보존된다

    // 갱신 후에도 살아있으면 alive 목록에 추가하고 간접 드로우의 InstanceCount 증가.
    if (p.life > 0.0f)
    {
        uint slot;
        g_DrawArgs.InterlockedAdd(4, 1, slot); // InstanceCount(byte offset 4) += 1, 이전값 = slot
        g_RWAliveList[slot] = i;
    }
}
