#include "pch.h"
#include "MapDataLoader.h"
// nlohmann/json: 클라이언트와 동일 헤더 공유
#include "../HelloDinner/json.hpp"

#include <fstream>
#include <iostream>
#include <cmath>
#include <string>

using json = nlohmann::json;

// =============================================================================
// 좌표 변환 상수
// =============================================================================
static constexpr float MAP_SCALE = 50.f;
static constexpr float RAD_SCALE = 100.f; // Sphere radius

static float DegToRad(float deg)
{
    constexpr float PI = 3.14159265358979323846f;
    return deg * PI / 180.f;
}

// =============================================================================
// LoadMapColliders
// =============================================================================

bool LoadMapColliders(const std::string& jsonPath,
                      std::vector<SServerCollider>& outColliders)
{
    std::ifstream f(jsonPath);
    if (!f.is_open()) {
        std::cout << "[Collision] Failed to open MapData.json: " << jsonPath << std::endl;
        return false;
    }

    json root;
    try {
        f >> root;
    }
    catch (const std::exception& e) {
        std::cout << "[Collision] Failed to parse MapData.json: " << e.what() << std::endl;
        return false;
    }

    int count = 0;

    for (auto& obj : root["mapData"]) {
        for (auto& inst : obj["instances"]) {
            if (!inst.contains("collider")) continue;

            auto& col = inst["collider"];
            if (!col.contains("colliderType")) continue;

            const std::string ctype = col["colliderType"].get<std::string>();

            // center (× 50) — Loader_Map.cpp:264-266
            float cx = col["center"]["x"].get<float>() * MAP_SCALE;
            float cy = col["center"]["y"].get<float>() * MAP_SCALE;
            float cz = col["center"]["z"].get<float>() * MAP_SCALE;

            if (ctype == "OBB") {
                // extents (× 50) — Loader_Map.cpp:269-271
                float ex = col["extents"]["x"].get<float>() * MAP_SCALE;
                float ey = col["extents"]["y"].get<float>() * MAP_SCALE;
                float ez = col["extents"]["z"].get<float>() * MAP_SCALE;

                // rotation: Y축 +180, 라디안 변환 — Loader_Map.cpp:273-275
                float rx = DegToRad(col["rotation"]["x"].get<float>());
                float ry = DegToRad(col["rotation"]["y"].get<float>() + 180.f);
                float rz = DegToRad(col["rotation"]["z"].get<float>());

                XMFLOAT4 q;
                XMStoreFloat4(&q, XMQuaternionRotationRollPitchYaw(rx, ry, rz));

                outColliders.push_back(
                    SServerCollider::FromOBB({ cx, cy, cz }, { ex, ey, ez }, q));
                ++count;
            }
            else if (ctype == "AABB") {
                float ex = col["extents"]["x"].get<float>() * MAP_SCALE;
                float ey = col["extents"]["y"].get<float>() * MAP_SCALE;
                float ez = col["extents"]["z"].get<float>() * MAP_SCALE;
                outColliders.push_back(
                    SServerCollider::FromAABB({ cx, cy, cz }, { ex, ey, ez }));
                ++count;
            }
            else if (ctype == "Sphere") {
                // radius (× 100) — Loader_Map.cpp:277
                float r = col["radius"].get<float>() * RAD_SCALE;
                outColliders.push_back(
                    SServerCollider::FromSphere({ cx, cy, cz }, r));
                ++count;
            }
            // 알 수 없는 타입은 건너뜀
        }
    }

    std::cout << "[Collision] Loaded " << count << " map colliders: " << jsonPath << std::endl;
    return count > 0;
}
