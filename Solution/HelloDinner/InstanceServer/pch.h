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
#include "../Server/PlayerInfo.h"
#include "../Server/protocol.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

using namespace std;
using namespace std::chrono;