#pragma once
// Declaration-only probe: no Windows implementation is claimed here.
// Keep all retail layout assertions enabled to expose arm64 blockers.
#include <windows.h>
#include <cstring>
#include <cstdlib>
#include <cmath>
#define __forceinline inline
#define __thiscall
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _snprintf snprintf
#define _vsnprintf vsnprintf
