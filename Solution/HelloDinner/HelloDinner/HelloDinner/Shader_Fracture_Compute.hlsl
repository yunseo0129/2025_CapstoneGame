// =============================================================================
//  Shader_Fracture_Compute.hlsl  -  파괴 조각(chunk) 강체 물리 (CS)
//  - 파티클 CS_Update 와 동형: chunk 상태 버퍼(u0)를 적분하고, 결과 본 매트릭스(u1)를 채운다.
//  - "간단 물리": chunk-chunk 충돌 없음. 중력 + 공기저항 + 회전 + 바닥 평면 충돌(반발/마찰)만.
//  - 본(조각) 매트릭스 = T(-centerBind)·R(quat)·T(pos)  (무게중심 기준 강체 변환)
//      bind(quat=identity, pos=centerBind) -> identity -> 정점 제자리(렌더 VS의 g_matWorld만 적용)
//  - 레지스터: b0(상수) / u0(chunk 상태) u1(본 매트릭스 출력)
// =============================================================================

// C++ CHUNK 와 동일 레이아웃 (80 bytes, 16B 정렬)
struct Chunk
{
    float3 pos;
    float invMass; // invMass==0 -> 비활성(bind 상태, 적분 스킵)
    float4 rot; // 쿼터니언 (x,y,z,w)
    float3 centerBind;
    float restitution; // bind 시점 무게중심(회전축, 불변) / 바닥 반발계수
    float3 linVel;
    float drag; // 선속도 / 공기저항[1/s]
    float3 angVel;
    float radius; // 각속도(축*크기) / 바닥 충돌 반경
};

RWStructuredBuffer<Chunk> g_Chunks : register(u0);
RWStructuredBuffer<float4x4> g_ChunkMatrices : register(u1); // 렌더 VS 가 SRV 로 읽음

// [A] 모델 공간 OBB 콜라이더 (C++ GPU_COLLIDER 와 동일 레이아웃, 64 bytes)
struct Collider
{
    float3 center;
    float extX;
    float3 axisX;
    float extY;
    float3 axisY;
    float extZ;
    float3 axisZ;
    float pad;
};
StructuredBuffer<Collider> g_Colliders : register(t0); // 2a: 선언만. 2b 에서 충돌에 사용.

cbuffer FractureComputeCB : register(b0)
{
    float g_dt; // delta time(클램프 권장)
    uint g_chunkCount; // 활성/전체 chunk 수
    float g_floorY; // 바닥 평면 높이(월드 y)
    float g_friction; // 접지 시 수평속도/각속도 감쇠(0~1)
    float3 g_gravity; // 중력 가속도 (예: 0,-9.8,0)
    uint g_colliderCount; // [A] 활성 콜라이더 수 (였던 g_pad)
};

// ---- 쿼터니언/행렬 헬퍼 (row-major) ----
float4 quatMul(float4 a, float4 b)
{
    return float4(
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
}

float4x4 quatToMat(float4 q)
{
    float x = q.x, y = q.y, z = q.z, w = q.w;
    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;
    // row-major 회전행렬 (mul(rowVec, M) 규약)
    return float4x4(
        1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy), 0,
        2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx), 0,
        2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy), 0,
        0, 0, 0, 1);
}

float4x4 translate(float3 t)
{
    return float4x4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        t.x, t.y, t.z, 1);
}

[numthreads(64, 1, 1)]
void CS_Fracture(uint3 dtid : SV_DispatchThreadID)
{
    uint i = dtid.x;
    if (i >= g_chunkCount)
        return;

    Chunk c = g_Chunks[i];

    // 활성 조각만 적분 (Break 로 invMass>0 가 된 것). 비활성은 bind 상태 유지 -> 아래에서 identity.
    if (c.invMass > 0.0f)
    {
        float dt = g_dt;

        // 선운동: 중력 + 공기저항
        c.linVel += g_gravity * dt;
        c.linVel *= saturate(1.0f - c.drag * dt);
        c.pos += c.linVel * dt;

        // 회전: 쿼터니언 적분  q += 0.5 * (angVel as quat) * q * dt, 정규화
        float4 wq = float4(c.angVel, 0.0f);
        c.rot = normalize(c.rot + 0.5f * quatMul(wq, c.rot) * dt);

        // [축 보정] 명시적 바닥 평면(model-Y) 충돌 제거 — 월드축과 어긋나는 가정이었음.
        //   바닥은 이제 OBB 콜라이더(바닥 패널)로 처리되어 아래 충돌 루프가 담당한다.

        // [A-2b] 월드 오브젝트(모델 공간 OBB) 충돌 — sphere(c.pos, c.radius) vs 각 OBB
        [loop]
        for (uint k = 0; k < g_colliderCount; ++k)
        {
            Collider ob = g_Colliders[k];
            float3 ext = float3(ob.extX, ob.extY, ob.extZ);
            float3 dd = c.pos - ob.center;
            float3 lp = float3(dot(dd, ob.axisX), dot(dd, ob.axisY), dot(dd, ob.axisZ)); // OBB 로컬 좌표

            float3 nrm;
            float depth;
            if (all(abs(lp) <= ext))
            {
                // 중심이 OBB 내부 → 최소 침투 면 방향으로 밀어냄
                float3 fdist = ext - abs(lp);
                if (fdist.x <= fdist.y && fdist.x <= fdist.z)
                {
                    nrm = (lp.x >= 0 ? ob.axisX : -ob.axisX);
                    depth = fdist.x + c.radius;
                }
                else if (fdist.y <= fdist.z)
                {
                    nrm = (lp.y >= 0 ? ob.axisY : -ob.axisY);
                    depth = fdist.y + c.radius;
                }
                else
                {
                    nrm = (lp.z >= 0 ? ob.axisZ : -ob.axisZ);
                    depth = fdist.z + c.radius;
                }
            }
            else
            {
                float3 cl = clamp(lp, -ext, ext);
                float3 closest = ob.center + cl.x * ob.axisX + cl.y * ob.axisY + cl.z * ob.axisZ;
                float3 toC = c.pos - closest;
                float dist2 = dot(toC, toC);
                if (dist2 >= c.radius * c.radius)
                    continue; // 충돌 아님
                float dist = sqrt(max(dist2, 1e-12f));
                nrm = toC / dist;
                depth = c.radius - dist;
            }

            // 위치 밀어내기(관통 해소)
            c.pos += nrm * depth;

            // 속도/회전 응답 (표면으로 들어가는 경우만)
            float vn = dot(c.linVel, nrm);
            if (vn < 0.0f)
            {
                float3 vt = c.linVel - vn * nrm; // 접선(슬라이딩) 속도
                c.linVel = vt * g_friction + (-c.restitution * vn) * nrm; // 접선 마찰 + 법선 반발
                c.angVel += cross(nrm, vt) * (1.0f / max(c.radius, 1e-3f)); // 슬라이딩 → 텀블링
                c.angVel = clamp(c.angVel, -30.0f, 30.0f); // 폭주 방지
            }
        }

        g_Chunks[i] = c;
    }

    // 본(조각) 매트릭스 합성: T(-centerBind)·R(quat)·T(pos)
    //  정점 -> 무게중심 원점으로 -> 회전 -> 현재 위치로.  bind 면 identity.
    float4x4 m = mul(mul(translate(-c.centerBind), quatToMat(c.rot)), translate(c.pos));
    g_ChunkMatrices[i] = m;
}
