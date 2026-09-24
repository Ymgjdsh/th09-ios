#pragma once

#include "Global.hpp"
#include <SDL.h>

namespace th095
{
namespace modern
{
void LogStartup(const char *phase);

namespace ios
{

enum IosLanguage
{
    IOS_LANGUAGE_JP = 0,
    IOS_LANGUAGE_ZH = 1,
    IOS_LANGUAGE_EN = 2,
};

// Runtime language state is persisted with the mobile controls configuration.
IosLanguage GetLanguage();
const char *LanguageName();
void CycleLanguage();
u32 LanguageRevision();
bool ConsumeLanguageRestartRequest();
bool IsLanguageRestartInProgress();
void CompleteLanguageRestart();

// Text used by original in-game menu sprites. These strings are passed to the
// sprite replacement path rather than drawn as a separate overlay. The title
// screens intentionally remain in their original Japanese form.
const char *PauseMenuText(int index);
const char *RetryMenuText(int index);

struct PresentationLayout
{
    int drawableWidth;
    int drawableHeight;
    int viewportX;
    int viewportY;
    int viewportWidth;
    int viewportHeight;
};

void SetPresentationLayout(const PresentationLayout &layout);
PresentationLayout GetPresentationLayout();
bool UsePortraitBattle();
float ControlScale(int drawableWidth, int drawableHeight);

// Polls SDL touch events and returns the current Touhou button bitset. The
// controls use drawable coordinates; game menu taps are transformed back to
// the original 640x480 canvas, including the portrait playfield crop.
u16 PollButtons();
void ProcessEvent(const SDL_Event &event);
void ResetTouchState();
void DrawVirtualControls();
bool IsDeveloperInvincible();
bool IsAutoBombEnabled();
bool ConsumeDragDelta(f32 *dx, f32 *dy);
void SetPresentationShake(f32 x, f32 y);
void GetPresentationShake(f32 *x, f32 *y);

} // namespace ios
} // namespace modern
} // namespace th095
