#pragma once
#include <stddef.h>

namespace th095
{
// Native front-end views share the original POD regions but expand the same
// pointer-bearing regions. Keep these deltas named and assert every consumer.
constexpr size_t kFrontEndPointerGrowth = sizeof(void *) - 4;
constexpr size_t kFrontEndTextGrowth = 11 * kFrontEndPointerGrowth;
constexpr size_t kFrontEndReplayGrowth = 81 * kFrontEndPointerGrowth;
constexpr size_t kFrontEndVmOffset = 0xbf4 + 2 * kFrontEndPointerGrowth;
constexpr size_t kFrontEndReplayOffset = 0xea8 + 2 * kFrontEndPointerGrowth + kFrontEndTextGrowth;
constexpr size_t kFrontEndFlagsOffset = 0x6120 + 2 * kFrontEndPointerGrowth + kFrontEndTextGrowth + kFrontEndReplayGrowth;
}
