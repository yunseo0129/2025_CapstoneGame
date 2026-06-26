#pragma once
#include <iostream>
#include <array>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <random>
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include <string>
#include <filesystem>
// DirectXMath/Collision: 서버 맵 충돌 처리에 사용 (Windows SDK 헤더, 링크 불필요)
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include "../Server/PlayerInfo.h"
#include "../Server/protocol.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

using namespace std;
using namespace std::chrono;
using namespace DirectX;