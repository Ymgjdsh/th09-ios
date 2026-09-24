#ifdef TH095_MATCH_EXACT
#include "ReplayBrowserExact.hpp"
#else
#ifndef TH095_REPLAY_BROWSER_HPP
#define TH095_REPLAY_BROWSER_HPP

#include "FrontEndGlobals.hpp"
#include "ReplayManager.hpp"
#include "ResultScreen.hpp"
#include "SceneSelect.hpp"

namespace th095
{

#ifndef DIFFBUILD
typedef i32 ReplayBrowserState;
enum ReplayBrowserStateValue
{
    REPLAY_BROWSER_STATE_INITIALIZE = 0,
    REPLAY_BROWSER_STATE_BROWSE = 1,
};
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ReplayBrowserStateSizeIs4[
    (sizeof(ReplayBrowserState) == sizeof(i32)) ? 1 : -1];
#endif
#endif

#ifdef TH095_IOS_PORTABLE_LAYOUT
#pragma pack(push, 4)
#endif
struct ReplayBrowserView
{
    SceneAnmLoadedView *sceneAnm;
    u8 unknown0004[sizeof(void *)];
    ZunTimer stateTimer;
    ZunTimer animationTimer; // +0x14; advanced by the shared front-end update
    ResultScreenReplayCursor rowCursor;
    ResultScreenReplayCursor columnCursor;
    u8 unknown01d0[0xa20];
    i32 selectedReplayIndex;
    SceneAnmVmIdArray vmIds;
#ifdef TH095_IOS_PORTABLE_LAYOUT
    u8 nativeTextGrowth[kFrontEndTextGrowth];
#endif
    u8 unknown0e88[0x20];
    ReplayManager *replays[80];
    u8 unknown0fe8[0x5118];
#ifdef TH095_IOS_PORTABLE_LAYOUT
    u8 nativeReplayListGrowth[kFrontEndPointerGrowth];
#endif
    AnmVmId transitionVm;
    u8 unknown6104[8];
#ifdef DIFFBUILD
    i32 state;
#else
    ReplayBrowserState state;
#endif
#ifdef DIFFBUILD
    i32 requestedState;
#else
    FrontEndRequestedState requestedState;
#endif

    ZunResult LoadReplaySlot(i32 slot, char *path);
    ChainCallbackResult Update();
};

#ifdef TH095_IOS_PORTABLE_LAYOUT
#pragma pack(pop)
static_assert(offsetof(ReplayBrowserView, replays) == kFrontEndReplayOffset, "replay array owner");
static_assert(offsetof(ReplayBrowserView, requestedState) == kFrontEndFlagsOffset - 0x10, "replay state owner");
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ReplayBrowserAnimationTimerAt14[
    (offsetof(ReplayBrowserView, animationTimer) == 0x14) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ReplayBrowserReplaysAtEA8[
    (offsetof(ReplayBrowserView, replays) == 0xea8) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ReplayBrowserStateAt610C[
    (offsetof(ReplayBrowserView, state) == 0x610c) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ReplayBrowserRequestedStateAt6110[
    (offsetof(ReplayBrowserView, requestedState) == 0x6110) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ReplayBrowserRowCursorAt20[
    (offsetof(ReplayBrowserView, rowCursor) == 0x20) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ReplayBrowserColumnCursorAtF8[
    (offsetof(ReplayBrowserView, columnCursor) == 0xf8) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ReplayBrowserVmIdsAtBF4[
    (offsetof(ReplayBrowserView, vmIds) == 0xbf4) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ReplayBrowserTransitionVmAt6100[
    (offsetof(ReplayBrowserView, transitionVm) == 0x6100) ? 1 : -1];
#endif

struct ReplayBrowserExitSignal
{
    u8 unknown000[8];
    i32 requested;

    void Request();
};
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ReplayBrowserExitRequestedAt08[
    (offsetof(ReplayBrowserExitSignal, requested) == 0x08) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ReplayBrowserExitSignalSizeIs0C[
    (sizeof(ReplayBrowserExitSignal) == 0x0c) ? 1 : -1];
#endif

extern i32 g_ReplayBrowserSelection;
extern char g_SelectedReplayPath[0x100];
#if defined(DIFFBUILD) || defined(TH095_MATCH_EXACT)
extern ReplayBrowserExitSignal g_ReplayBrowserExitSignal;
#else
extern ReplayBrowserExitSignal &g_ReplayBrowserExitSignal;
#endif

void __fastcall LoadReplayBrowserEntries(void *unused);

} // namespace th095

#endif

#endif // TH095_MATCH_EXACT
