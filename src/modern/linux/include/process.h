#pragma once

#include <stdint.h>
#include <windows.h>

// Small compatibility surface for the two CRT thread helpers used by the
// reconstructed source.  The iOS runtime already implements CreateThread and
// its handle waiting semantics; route the old names through that layer.
typedef unsigned (__stdcall *_beginthreadex_proc_type)(void *);

static inline uintptr_t _beginthreadex(
    void *, unsigned, _beginthreadex_proc_type start, void *argument,
    unsigned, unsigned *threadId)
{
    return reinterpret_cast<uintptr_t>(CreateThread(
        NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(start), argument,
        0, reinterpret_cast<LPDWORD>(threadId)));
}

static inline uintptr_t _beginthread(
    void (__cdecl *start)(void *), unsigned, void *argument)
{
    return reinterpret_cast<uintptr_t>(CreateThread(
        NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(start), argument,
        0, NULL));
}

static inline void _endthread() {}
static inline void _endthreadex(unsigned) {}
