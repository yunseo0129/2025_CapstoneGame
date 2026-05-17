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
#include <chrono>
#include "PlayerInfo.h"
#include "protocol.h"

// 윈속 라이브러리
#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

using namespace std;
using namespace std::chrono;