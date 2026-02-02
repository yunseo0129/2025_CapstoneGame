#pragma once

namespace Client
{
	constexpr int g_iWinSizeX = 1280;
	constexpr int g_iWinSizeY = 720;
}

namespace RootParameter
{
    constexpr UINT GameObject = 0;
    constexpr UINT Camera = 1;
    constexpr UINT TextureCube = 2;
    constexpr UINT Texture = 3;
}

namespace DescriptorRange
{
    constexpr UINT Texture = 0;
    constexpr UINT TextureCube = 1;
}

namespace Engine
{
    enum class PROTOTYPE { PROTO_GAMEOBJ, PROTO_COMPONENT };

    enum MOUSEKEYSTATE { DIM_LB, DIM_RB, DIM_MB, DIM_END };
    enum MOUSEMOVESTATE { DIMS_X, DIMS_Y, DIMS_Z, DIMS_END };
}

#ifndef			MSG_BOX
#define			MSG_BOX(_message)			MessageBox(NULL, TEXT(_message), L"System Message", MB_OK)
#endif