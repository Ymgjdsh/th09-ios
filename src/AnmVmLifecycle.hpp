#ifndef TH095_ANM_VM_LIFECYCLE_HPP
#define TH095_ANM_VM_LIFECYCLE_HPP

#include "AnmManager.hpp"
#include "AnmVmId.hpp"

#include <stdlib.h>

namespace th095
{

struct AnmVmLifecycleView
{
    AnmVmLifecycleView *next;
#if defined(TH095_MATCH_EXACT)
    u8 unknown004[4];
#else
    AnmVmLifecycleView *nextInDrawLayer;
#endif
    AnmVmLifecycleView *previous;
    u32 renderMode;
    struct Id
    {
        i32 value;

        Id()
        {
            this->value = 0;
        }

        i32 operator==(Id other)
        {
            return this->value == other.value;
        }

        i32 operator++(int)
        {
            return this->value++;
        }
    } id;
};

#ifdef TH095_IOS_PORTABLE_LAYOUT
typedef AnmVm AnmVmDeleteView;
#else
struct AnmVmDeleteView;
#endif

// Canonical bounded receiver used by the exact AddVm/RemoveVm units.  Its list
// pointers coincide with AnmManager::vmListHead/vmListTail at +0x381814.
struct AnmManagerVmLifecycleView
{
#ifdef TH095_IOS_PORTABLE_LAYOUT
    u8 unknown000[offsetof(AnmManager, vmListHead)];
#else
    u8 unknown000[0x381814];
#endif
    AnmVmLifecycleView *vmListHead;
    AnmVmLifecycleView *vmListTail;
#ifdef TH095_IOS_PORTABLE_LAYOUT
    u8 unknown38181c[sizeof(AnmVm) * 9];
#else
    u8 unknown38181c[0x192c];
#endif
    AnmVmLifecycleView::Id nextVmId;

    AnmVmId AddVm(AnmVmLifecycleView *vm);
    i32 RemoveVm(AnmVmDeleteView *vm);
};

#ifdef TH095_IOS_PORTABLE_LAYOUT
static_assert(offsetof(AnmManagerVmLifecycleView, nextVmId) == offsetof(AnmManager, nextVmId), "VM id owner");
static_assert(offsetof(AnmVmLifecycleView, id) == offsetof(AnmVm, id), "VM id member");
#else
struct AnmVmDeleteView
{
    AnmVmDeleteView *next;
    u8 unknown004[4];
    AnmVmDeleteView *previous;
    u8 unknown00c[8];
#if defined(TH095_MATCH_EXACT) || defined(DIFFBUILD)
    void *generatedVertices;
#else
    void *ownedRenderData;
#endif

    ~AnmVmDeleteView()
    {
#if defined(TH095_MATCH_EXACT) || defined(DIFFBUILD)
        if (this->generatedVertices != NULL)
        {
            void *generatedVertices = this->generatedVertices;
            free(generatedVertices);
        }
#else
        if (this->ownedRenderData != NULL)
        {
            void *ownedRenderData = this->ownedRenderData;
            free(ownedRenderData);
        }
#endif
    }
};
#endif

} // namespace th095

#endif
