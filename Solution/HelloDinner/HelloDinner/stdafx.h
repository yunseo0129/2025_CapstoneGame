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
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
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
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")


#include <string>
#include <ranges>
#include <vector>
#include <map>
#include <algorithm>
#include <unordered_map>
using namespace std;

#include "Defines.h"
#include "Utiles.h"

typedef		bool						_bool;

typedef		signed char					_byte;
typedef		unsigned char				_ubyte;
typedef		char						_char;


typedef		wchar_t						_tchar;
typedef		wstring						_wstring;

typedef		signed short				_short;
typedef		unsigned short				_ushort;

typedef		signed int					_int;
typedef		unsigned int				_uint;

typedef		signed long					_long;
typedef		unsigned long				_ulong;

typedef		float						_float;
typedef		double						_double;

typedef		XMUINT2						_uint2;
typedef		XMFLOAT2					_float2;
typedef		XMFLOAT3					_float3;
typedef		XMFLOAT4					_float4;
typedef		XMVECTOR					_vector;
typedef		FXMVECTOR					_fvector;
typedef		GXMVECTOR					_gvector;
typedef		HXMVECTOR					_hvector;
typedef		CXMVECTOR					_cvector;

typedef		XMFLOAT4X4					_float4x4;
typedef		XMMATRIX					_matrix;
typedef		FXMMATRIX					_fmatrix;
