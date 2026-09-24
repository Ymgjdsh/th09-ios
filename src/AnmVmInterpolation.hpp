#ifndef TH095_ANM_VM_INTERPOLATION_HPP
#define TH095_ANM_VM_INTERPOLATION_HPP

#include "AnmManager.hpp"

namespace th095
{

struct AnmVmColorInterpolationView : AnmVmBase
{
#ifdef TH095_IOS_PORTABLE_LAYOUT
    u8 unknown248[offsetof(AnmVm, color1Initial) - sizeof(AnmVmBase)];
#else
    u8 unknown248[0x50];
#endif
    ZunColor color1Initial;
    ZunColor color1Final;

    void SetColor1Interpolation(
        i32 duration, u8 mode, u32 initial, u32 final);
};

#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char AnmVmColorInitialAt298[
    (offsetof(AnmVmColorInterpolationView, color1Initial) == 0x298) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char AnmVmColorFinalAt29C[
    (offsetof(AnmVmColorInterpolationView, color1Final) == 0x29c) ? 1 : -1];
#endif

} // namespace th095

#endif
