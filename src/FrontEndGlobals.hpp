#pragma once

#include "diffbuild.hpp"

namespace th095
{

#if defined(DIFFBUILD)
#define FRONT_END_REQUESTED_STATE_INITIALIZE 0
#define FRONT_END_REQUESTED_STATE_MAIN_MENU 1
#define FRONT_END_REQUESTED_STATE_SCENE_SELECT 2
#define FRONT_END_REQUESTED_STATE_REPLAY_BROWSER 3
#define FRONT_END_REQUESTED_STATE_EXIT 4
#define FRONT_END_REQUESTED_STATE_START_GAME 5
#define FRONT_END_REQUESTED_STATE_START_REPLAY 6
#define FRONT_END_REQUESTED_STATE_OPTIONS 7
#define FRONT_END_REQUESTED_STATE_MUSIC_ROOM 8
#define FRONT_END_REQUESTED_STATE_HELP 9
#elif !defined(TH095_MATCH_EXACT)
enum FrontEndRequestedState
{
    FRONT_END_REQUESTED_STATE_INITIALIZE = 0,
    FRONT_END_REQUESTED_STATE_MAIN_MENU = 1,
    FRONT_END_REQUESTED_STATE_SCENE_SELECT = 2,
    FRONT_END_REQUESTED_STATE_REPLAY_BROWSER = 3,
    FRONT_END_REQUESTED_STATE_EXIT = 4,
    FRONT_END_REQUESTED_STATE_START_GAME = 5,
    FRONT_END_REQUESTED_STATE_START_REPLAY = 6,
    FRONT_END_REQUESTED_STATE_OPTIONS = 7,
    FRONT_END_REQUESTED_STATE_MUSIC_ROOM = 8,
    FRONT_END_REQUESTED_STATE_HELP = 9,
};

#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char FrontEndRequestedStateSizeIs4[
    (sizeof(FrontEndRequestedState) == 4) ? 1 : -1];
#endif
#endif

// The target has one active front-end/pause controller pointer at 0x004CA2F4.
// Its concrete view changes with the current menu, so keep the storage type
// neutral and cast only at the owning call site.
DIFFABLE_EXTERN(void *, g_ActiveMenuController);
#ifdef TH095_IOS_PORTABLE_LAYOUT
// Read the loader barrier through its allocating owner, not a retail-offset
// overlay whose preceding native pointer arrays have a different size.
bool FrontEndTitleLoadIncomplete(const void *controller);
bool FrontEndTapMainMenu(float x, float y);
bool FrontEndSceneSelectReady();
// 0: no hit, 1: selected, 2: selected and activate via normal input.
int FrontEndTapSubmenu(float x, float y);
int FrontEndTapScene(float x, float y);
int ResultScreenTapMenu(float x, float y);
// Defined only in simulator regression builds.
bool ResultScreenTouchPointForItem(int item, float *x, float *y);
#endif

// These historical target-facing globals are not independent storage. They are
// the +0x08 exit handshake and +0x0c active fields of
// Supervisor::replayScanWorker at target 0x004C4CB8. Exact probes retain the
// original relocation spellings; the runnable build binds them to the embedded
// worker fields.
#if defined(DIFFBUILD) && !defined(TH095_MATCH_EXACT)
DIFFABLE_EXTERN(int, g_HelpLoadComplete);
DIFFABLE_EXTERN(int, g_HelpLoadActive);
#elif !defined(TH095_MATCH_EXACT)
extern int &g_HelpLoadComplete;
extern int &g_HelpLoadActive;
#endif

} // namespace th095
