// header.h: 표준 시스템 포함 파일
// 또는 프로젝트 특정 포함 파일이 들어 있는 포함 파일입니다.
//

#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.
// Windows 헤더 파일
#include <windows.h>
// C 런타임 헤더 파일입니다.
#include <stdlib.h>
#include <malloc.h>
#include <memory>
#include <tchar.h>

#include <wrl.h>	//comptr 사용 (com 객체 주기 관리를 효율적으로 하기 위해)
#include <comdef.h> // _com_error 사용
using namespace Microsoft::WRL;

#include <d3d12.h>
#include "../HelloDinner/Common/d3dx12.h"
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#pragma comment(lib, "dxguid.lib")

#include <string>
#include <ranges>
#include <vector>
#include <algorithm>
#include <unordered_map>
using namespace std;

#include "Defines.h"
#include "Utiles.h"

