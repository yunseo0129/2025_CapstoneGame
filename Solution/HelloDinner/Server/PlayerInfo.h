#pragma once
#include "protocol.h"

// 카메라 정보 
struct CameraInfo
{
	float	positionX = 0.f;		// 카메라(=플레이어) 위치
	float	positionY = 0.f;
	float	positionZ = 0.f;
	float	yaw = 0.f;				// 좌우 회전 (Y축)
	float	pitch = 0.f;			// 상하 회전 (X축)
	float	lookX = 0.f;			// look 벡터
	float	lookY = 0.f;
	float	lookZ = 1.f;
};

// 플레이어 정보 (카메라로부터 position, yaw를 상속받는 구조)
// position → CameraInfo.positionXYZ 사용
// rotationY(Yaw) → CameraInfo.yaw 사용
struct PlayerInfo
{
	int		id = -1;
	char	name[NAME_SIZE] = {};
	char	keyInput = 0;
};