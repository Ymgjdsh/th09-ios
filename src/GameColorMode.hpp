#pragma once

#include "inttypes.hpp"

namespace th095
{

typedef u8 GameColorMode;

enum GameColorModeValue
{
    GAME_COLOR_MODE_32_BIT = 0,
    GAME_COLOR_MODE_16_BIT = 1,
    GAME_COLOR_MODE_COUNT = 2,
};

#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char GameColorModeSizeIs1[(sizeof(GameColorMode) == 1) ? 1 : -1];
#endif

} // namespace th095
