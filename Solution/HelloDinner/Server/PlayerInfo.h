#pragma once
#include "protocol.h"
#include <cmath>
#include <cstring>

// 월드 행렬 정보 (4x4 = float[16], 행 우선)
// [0..3]=Right, [4..7]=Up, [8..11]=Look, [12..15]=Position
// CTransform::STATE_RIGHT=0행, STATE_UP=1행, STATE_LOOK=2행, STATE_POSITION=3행
struct WorldMatrixInfo
{
	float m[16] = {
		1.f, 0.f, 0.f, 0.f,   // Right  (m[0], m[1], m[2], m[3])
		0.f, 1.f, 0.f, 0.f,   // Up     (m[4], m[5], m[6], m[7])
		0.f, 0.f, 1.f, 0.f,   // Look   (m[8], m[9], m[10], m[11])
		0.f, 0.f, 0.f, 1.f    // Pos    (m[12], m[13], m[14], m[15])
	};

	void SetPosition(float x, float y, float z)
	{
		m[12] = x;  m[13] = y;  m[14] = z;
	}

	// ==========================================
	// CTransform::Turn(Y축, angle) 과 동일
	// XMMatrixRotationAxis({0,1,0,0}, rotationPerSec * fTimeDelta)
	// → Right, Up, Look 벡터 3개를 모두 Y축 기준으로 회전
	// ==========================================
	void TurnY(float angle)
	{
		float cosA = cosf(angle);
		float sinA = sinf(angle);

		// Right 벡터 회전
		float r0 = m[0], r1 = m[1], r2 = m[2];
		m[0] =  r0 * cosA + r2 * sinA;
		m[1] =  r1;
		m[2] = -r0 * sinA + r2 * cosA;

		// Up 벡터 회전
		float u0 = m[4], u1 = m[5], u2 = m[6];
		m[4] =  u0 * cosA + u2 * sinA;
		m[5] =  u1;
		m[6] = -u0 * sinA + u2 * cosA;

		// Look 벡터 회전
		float l0 = m[8], l1 = m[9], l2 = m[10];
		m[8]  =  l0 * cosA + l2 * sinA;
		m[9]  =  l1;
		m[10] = -l0 * sinA + l2 * cosA;
	}

	// ==========================================
	// CTransform::Go_Straight 과 동일
	// Look 벡터에서 Y=0으로 만든 뒤 normalize → position += dir * speed * dt
	// ==========================================
	void GoStraight(float fSpeed, float fTimeDelta)
	{
		// Look 벡터 (Y=0으로 flatten)
		float lx = m[8], lz = m[10];
		float len = sqrtf(lx * lx + lz * lz);
		if (len < 0.0001f) return;
		lx /= len;  lz /= len;

		m[12] += lx * fSpeed * fTimeDelta;
		m[14] += lz * fSpeed * fTimeDelta;
	}

	// ==========================================
	// CTransform::Go_Backward 과 동일
	// ==========================================
	void GoBackward(float fSpeed, float fTimeDelta)
	{
		float lx = m[8], lz = m[10];
		float len = sqrtf(lx * lx + lz * lz);
		if (len < 0.0001f) return;
		lx /= len;  lz /= len;

		m[12] -= lx * fSpeed * fTimeDelta;
		m[14] -= lz * fSpeed * fTimeDelta;
	}

	// ==========================================
	// CTransform::Go_Left 과 동일
	// Right 벡터를 normalize → position -= dir * speed * dt
	// ==========================================
	void GoLeft(float fSpeed, float fTimeDelta)
	{
		float rx = m[0], ry = m[1], rz = m[2];
		float len = sqrtf(rx * rx + ry * ry + rz * rz);
		if (len < 0.0001f) return;
		rx /= len;  ry /= len;  rz /= len;

		m[12] -= rx * fSpeed * fTimeDelta;
		m[13] -= ry * fSpeed * fTimeDelta;
		m[14] -= rz * fSpeed * fTimeDelta;
	}

	// ==========================================
	// CTransform::Go_Right 과 동일
	// ==========================================
	void GoRight(float fSpeed, float fTimeDelta)
	{
		float rx = m[0], ry = m[1], rz = m[2];
		float len = sqrtf(rx * rx + ry * ry + rz * rz);
		if (len < 0.0001f) return;
		rx /= len;  ry /= len;  rz /= len;

		m[12] += rx * fSpeed * fTimeDelta;
		m[13] += ry * fSpeed * fTimeDelta;
		m[14] += rz * fSpeed * fTimeDelta;
	}

	// ==========================================
	// CTransform::Go_Up 과 동일
	// 월드 Up(0,1,0) 방향으로 position += {0,1,0} * speed * dt
	// ==========================================
	void GoUp(float fSpeed, float fTimeDelta)
	{
		m[13] += fSpeed * fTimeDelta;
	}

};

// 플레이어 정보
struct PlayerInfo
{
	int				id = -1;
	char			name[NAME_SIZE] = {};
	unsigned char	keyInput = 0;
	float			speedPerSec = 1.f;
	float			rotationPerSec = 1.f;
};