#ifdef TH095_MATCH_EXACT
#include "ResultScreenExact.hpp"
#else
#ifndef TH095_RESULT_SCREEN_HPP
#define TH095_RESULT_SCREEN_HPP

#include "AnmManager.hpp"
#include "Global.hpp"
#include "AnmVmId.hpp"
#include "ReplayManager.hpp"
#include "ScoreData.hpp"
#include "ZunTimer.hpp"
#include <stdlib.h>
#ifdef TH095_IOS_PORTABLE_LAYOUT
#include "PhotoStage.hpp"
#include "PhotoPlayerRuntime.hpp"
#endif

namespace th095
{

#ifdef TH095_MATCH_EXACT
typedef ::ZunResult ResultScreenResult;
#else
typedef ZunResult ResultScreenResult;
#endif

#ifdef TH095_MATCH_EXACT
struct ResultScreenTimer
{
    i32 previous;
    f32 subFrame;
    i32 current;

    ResultScreenTimer()
    {
        this->current = 0;
        this->previous = -999999;
        this->subFrame = 0.0f;
    }

    u32 operator==(i32 value) { return this->current == value; }
    u32 operator<(i32 value) { return this->current < value; }
    u32 operator>=(i32 value) { return this->current >= value; }
    i32 GetCurrent() { return this->current; }
    i32 Tick();
    void Reset()
    {
        this->current = 0;
        this->subFrame = 0.0f;
        this->previous = -999999;
    }
    void Set(i32 value)
    {
        this->current = value;
        this->subFrame = (f32)value;
        this->previous = -999999;
    }
};
typedef ResultScreenTimer ResultScreenStateTimer;
#else
typedef ZunTimer ResultScreenStateTimer;
#endif

struct ResultScreenReplayCursor
{
    i32 current;
    i32 previous;
    i32 count;
    i32 savedCurrent[16];
    i32 savedCount[16];
    i32 saveDepth;
    i32 disabledEntries[16];
    i32 wraps;
    i32 disabledEntryCount;

    ResultScreenReplayCursor()
    {
        this->saveDepth = 0;
        this->current = 0;
        this->disabledEntryCount = 0;
        this->wraps = 1;
        this->count = 999;
    }

    // The target constructs ResultScreen::replayCursor in a distinct frontend
    // allocation phase before the otherwise-identical photoCursor.  The tag
    // overload selects that source phase without changing object layout or
    // runtime cursor initialization.
    ResultScreenReplayCursor(i32)
    {
        u8 compilerStorage[0x18];
        this->saveDepth = 0;
        this->current = 0;
        this->disabledEntryCount = 0;
        this->wraps = 1;
        this->count = 999;
    }

    i32 Move(i32 amount);
    void Push();
    void Pop();

    i32 GetCurrent() { return this->current; }
    i32 GetPrevious() { return this->previous; }
    i32 GetCount() { return this->count; }
    void SaveCurrent() { this->previous = this->current; }
    u32 HasChanged() { return this->previous != this->current; }

    void Disable(i32 value)
    {
        this->disabledEntries[this->disabledEntryCount] = value;
        this->disabledEntryCount++;
    }

    void Set(i32 value)
    {
        if (this->count != 0)
        {
            this->current = value >= this->count
                                ? this->count - 1
                                : (value < 0 ? 0 : value);
        }
        else
        {
            this->current = value;
        }
    }
};

typedef AnmVm ResultScreenAnmVm;

#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenAnmVmSizeIs2CC[
    (sizeof(ResultScreenAnmVm) == 0x2cc) ? 1 : -1];
#endif

typedef AnmLoaded ResultScreenAnmLoadedView;
typedef AnmTextureEntryView ResultScreenTextureEntryView;
typedef AnmLoadedSprite ResultScreenLoadedSpriteView;

#ifdef TH095_IOS_PORTABLE_LAYOUT
struct alignas(PhotoStageSlot) ResultPhotoSlotView
#else
struct ResultPhotoSlotView
#endif
{
#ifdef TH095_IOS_PORTABLE_LAYOUT
    u8 unknown0000[offsetof(PhotoStageSlot,display) + offsetof(PhotoStageDisplayView,scoreData)];
#else
    u8 unknown0000[0x21d4];
#endif
#ifdef DIFFBUILD
    u32 metadata[8];
#else
    PhotoScoreBreakdownView scoreBreakdown;
#endif
    i32 score;                        // +0x21f4
#ifdef TH095_IOS_PORTABLE_LAYOUT
    u8 displayTail[sizeof(PhotoStageDisplayView) - offsetof(PhotoStageDisplayView,score) - sizeof(i32)];
#endif
    i32 captureTime;                  // +0x21f8
    f32 slowRate;                     // +0x21fc
    u16 width;                        // +0x2200
    u16 unknown2202;
    u16 height;                       // +0x2204
    u16 unknown2206;
    char comment[12];                 // +0x2208
};

struct ResultPhotoDataView
{
    ResultPhotoSlotView slots[11];
    u8 unknown176dc[0x17720 - 0x176dc];
    AnmVmId photoVms[11];
#ifdef TH095_IOS_PORTABLE_LAYOUT
    u8 unknown1774c[offsetof(PhotoStageStateView,anm) - offsetof(PhotoStageStateView,capturedPhotoVms) - sizeof(PhotoAnmVmId)*11];
#else
    u8 unknown1774c[0x2571c - 0x1774c];
#endif
    ResultScreenAnmLoadedView *anm;

    i32 FindBestShot();
};

struct ResultPhotoControllerView
{
#ifdef TH095_IOS_PORTABLE_LAYOUT
    u8 unknown000[offsetof(PhotoPlayerRuntimeView,camera) + offsetof(PhotoPlayerCameraRuntimeView,photoLimit)];
#else
    u8 unknown000[0x29ec];
#endif
    i32 photoCount;

    i32 GetPhotoCount() { return this->photoCount; }
};
#ifdef TH095_IOS_PORTABLE_LAYOUT
static_assert(sizeof(ResultPhotoSlotView)==sizeof(PhotoStageSlot),"result slot stride matches photo owner");
static_assert(offsetof(ResultPhotoSlotView,captureTime)==offsetof(PhotoStageSlot,timestamp),"result timestamp owner");
static_assert(offsetof(ResultPhotoDataView,anm)==offsetof(PhotoStageStateView,anm),"result photo resource owner");
#endif

#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultPhotoSlotViewSizeIs2214[
    (sizeof(ResultPhotoSlotView) == 0x2214) ? 1 : -1];
#endif
#ifdef DIFFBUILD
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultPhotoSlotMetadataAt21D4[
    (offsetof(ResultPhotoSlotView, metadata) == 0x21d4) ? 1 : -1];
#endif
#else
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultPhotoSlotScoreBreakdownAt21D4[
    (offsetof(ResultPhotoSlotView, scoreBreakdown) == 0x21d4) ? 1 : -1];
#endif
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultPhotoSlotScoreAt21F4[
    (offsetof(ResultPhotoSlotView, score) == 0x21f4) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultPhotoSlotCaptureTimeAt21F8[
    (offsetof(ResultPhotoSlotView, captureTime) == 0x21f8) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultPhotoSlotSlowRateAt21FC[
    (offsetof(ResultPhotoSlotView, slowRate) == 0x21fc) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultPhotoSlotWidthAt2200[
    (offsetof(ResultPhotoSlotView, width) == 0x2200) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultPhotoSlotHeightAt2204[
    (offsetof(ResultPhotoSlotView, height) == 0x2204) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultPhotoSlotCommentAt2208[
    (offsetof(ResultPhotoSlotView, comment) == 0x2208) ? 1 : -1];
#endif

struct ResultScreenSceneLabel
{
    char firstLine[0x2c];
    char secondLine[0x2c];
};

#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenSceneLabelSizeIs58[
    (sizeof(ResultScreenSceneLabel) == 0x58) ? 1 : -1];
#endif

struct ResultScreen
{
    ResultScreenAnmLoadedView *anm;       // +0x0000
    i32 state;                            // +0x0004
    ResultScreenStateTimer stateTimer;    // +0x0008
    f32 savedGameSpeed;                   // +0x0014
    ResultScreenAnmVm vms[21];            // +0x0018
    ResultScreenAnmVm auxiliaryVms[2];    // +0x3ad4
    ResultScreenAnmVm photoVm;             // +0x406c
    ResultScreenAnmVm photoTransitionVm;   // +0x4338
    ResultScreenReplayCursor replayCursor; // +0x4604
    i32 keyboardSelection;                // +0x46dc
    i32 replayNameCursor;                 // +0x46e0
    u8 *helpTextBuffer;                 // +0x46e4
    ResultScreenSceneLabel sceneLabels[11][10]; // +0x46e8
    i32 sceneCounts[11];                  // +0x6cb8
    i32 selectedGroup;                    // +0x6ce4
    ReplayManager *replays[20];            // +0x6ce8
    u8 unknown6d38[4];
    char replayName[9];                   // +0x6d3c
    u8 unknown6d45[3];
    ResultScreenReplayCursor photoCursor; // +0x6d48
    i32 notificationTimer;                // +0x6e20
    ChainElem *calcChain;                  // +0x6e24
    ChainElem *drawChain;                  // +0x6e28

    ResultScreen();
    ~ResultScreen();

    ResultScreenResult Initialize();
    static ResultScreenResult LoadAnm();
    static ResultScreenResult ReleaseAnm();
    static ResultScreen *Create();
    void Destroy();
    i32 UpdateCursor(i32 firstVm);
    void PrepareBestShot();
    ResultScreenResult LoadReplays();
    ChainCallbackResult Update();
    ChainCallbackResult Draw();
    static ChainCallbackResult OnUpdate(ResultScreen *resultScreen);
    static ChainCallbackResult OnDraw(ResultScreen *resultScreen);

    void SetState(i32 value)
    {
        this->state = value;
        this->stateTimer.Reset();
    }
};

#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenVmsAt18[
    (offsetof(ResultScreen, vms) == 0x18) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenSceneLabelsAt46E8[
    (offsetof(ResultScreen, sceneLabels) == 0x46e8) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenSceneCountsAt6CB8[
    (offsetof(ResultScreen, sceneCounts) == 0x6cb8) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenSelectedGroupAt6CE4[
    (offsetof(ResultScreen, selectedGroup) == 0x6ce4) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenReplayCursorAt4604[
    (offsetof(ResultScreen, replayCursor) == 0x4604) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenReplayCursorSizeIsD8[
    (sizeof(ResultScreenReplayCursor) == 0xd8) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenReplaysAt6CE8[
    (offsetof(ResultScreen, replays) == 0x6ce8) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenReplayNameAt6D3C[
    (offsetof(ResultScreen, replayName) == 0x6d3c) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenPhotoCursorAt6D48[
    (offsetof(ResultScreen, photoCursor) == 0x6d48) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenNotificationTimerAt6E20[
    (offsetof(ResultScreen, notificationTimer) == 0x6e20) ? 1 : -1];
#endif
#ifndef TH095_IOS_PORTABLE_LAYOUT
typedef char ResultScreenSizeIs6E2C[
    (sizeof(ResultScreen) == 0x6e2c) ? 1 : -1];
#endif

} // namespace th095

#endif

#endif // TH095_MATCH_EXACT
