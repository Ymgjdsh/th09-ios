#pragma once
#include "AnmManager.hpp"
#include <cmath>

namespace th095 { namespace modern { namespace ios {
// Menu scripts position their labels at a stable baseline after revealing.
// Expand the label to a finger-sized row without changing its screen position.
inline bool HitMenuLabel(const AnmVm *vm, float x, float y)
{
    if (!vm || !vm->visible || !vm->loadedSprite) return false;
    const float left = vm->position.x + vm->positionOffset.x;
    const float top = vm->position.y + vm->positionOffset.y;
    const float width = std::fmax(160.f, std::fabs(vm->spriteSize.x * vm->scale.x));
    return x >= left - 16 && x <= left + width + 16 &&
           y >= top - 10 && y <= top + 20;
}
} } }
