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
