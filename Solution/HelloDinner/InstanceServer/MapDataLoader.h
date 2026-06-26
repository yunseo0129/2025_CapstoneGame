#pragma once
// MapData.json을 읽어 서버 맵 콜라이더 목록을 생성한다.
// 좌표 변환은 클라이언트 Loader_Map.cpp:241-277 과 동일:
//   position/center/extents × 50, radius × 100
//   rotation(deg) → radian, Y축에만 +180 적용
#include <vector>
#include <string>
#include "ServerCollision.h"

// jsonPath: MapData.json 파일 경로
// outColliders: 생성된 콜라이더 배열 (vector<SServerCollider> 에 push_back)
// 반환: 1개 이상 로드 성공 시 true
bool LoadMapColliders(const std::string& jsonPath,
                      std::vector<SServerCollider>& outColliders);
