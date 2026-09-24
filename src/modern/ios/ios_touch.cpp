#include "ios_touch.hpp"
#include "ios_cheat.hpp"
#include "ios_gl_legacy.hpp"

#include "BulletManager.hpp"
#include "GameManager.hpp"
#include "Gui.hpp"
#include "Player.hpp"
#include "ResultScreen.hpp"
#include "SoundPlayer.hpp"
#include "Supervisor.hpp"

#include <SDL.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace th095
{

// TH095 does not expose the TH08 TitleScreen helper used by the optional
// mobile smoke probes.  Keep those probes dormant until the real title menu
// adapter is present; the production title state remains owned by Supervisor.
struct TitleScreen
{
    static int IosReadyStartMenuCursor() { return -1; }
};

namespace modern
{
namespace ios
{
namespace
{
struct TouchState
{
    SDL_FingerID moveFinger;
    SDL_FingerID shotFinger;
    SDL_FingerID bombFinger;
    SDL_FingerID focusFinger;
    SDL_FingerID menuFinger;
    float moveX;
    float moveY;
    float moveDx;
    float moveDy;
    float dragDeltaX;
    float dragDeltaY;
    bool moveIsDrag;
    u16 buttons;
    u16 keyboardButtons;
    u16 controllerButtons;
    Sint16 controllerAxisX;
    Sint16 controllerAxisY;
    float sensitivity;
    TouchState()
        : moveFinger(-1), shotFinger(-1), bombFinger(-1), focusFinger(-1), menuFinger(-1),
          moveX(0), moveY(0), moveDx(0), moveDy(0), dragDeltaX(0), dragDeltaY(0),
          moveIsDrag(false), buttons(0), keyboardButtons(0), controllerButtons(0),
          controllerAxisX(0), controllerAxisY(0), sensitivity(1.0f)
    {
    }
};

TouchState g_state;
PresentationLayout g_layout = {640, 480, 0, 0, 640, 480};
f32 g_presentationShakeX = 0.0f;
f32 g_presentationShakeY = 0.0f;
bool g_developerOpen = false;
bool g_developerInvincible = false;

enum ControlMode
{
    CONTROL_JOYSTICK = 0,
    CONTROL_DRAG = 1,
    CONTROL_HYBRID = 2
};

enum PerformanceMode
{
    PERFORMANCE_FULL = 0,
    PERFORMANCE_BALANCED = 1,
    PERFORMANCE_SPEED = 2
};

enum ControlIndex
{
    CONTROL_INDEX_JOYSTICK = 0,
    CONTROL_INDEX_SHOOT,
    CONTROL_INDEX_BOMB,
    CONTROL_INDEX_FOCUS,
    CONTROL_INDEX_MENU,
    CONTROL_INDEX_COUNT
};

struct NormalizedLayout
{
    float x[CONTROL_INDEX_COUNT];
    float y[CONTROL_INDEX_COUNT];
};

struct MobileConfig
{
    u32 magic;
    u32 version;
    i32 controlMode;
    float buttonScale;
    float dragSensitivity;
    float opacity;
    u8 showButtons;
    u8 shootToggle;
    u8 focusToggle;
    u8 autoBomb;
    u8 customized[2];
    u8 performanceMode;
    u8 dragAutoShoot;
    NormalizedLayout layouts[2];
    u8 showDeveloperButton;
    u8 language;
};

const u32 MOBILE_CONFIG_MAGIC = 0x38424f4d; // MOB8
const u32 MOBILE_CONFIG_VERSION = 5;
MobileConfig g_config = {};
bool g_configLoaded = false;
bool g_settingsOpen = false;
bool g_performanceOpen = false;
bool g_layoutEdit = false;
bool g_shootLatched = false;
bool g_focusLatched = false;
bool g_backgroundPausePending = false;
bool g_appInBackground = false;
u16 g_pulseButtons = 0;
SDL_FingerID g_settingsFinger = -1;
int g_settingsRow = -1;
SDL_FingerID g_layoutFinger = -1;
int g_layoutControl = -1;
MobileConfig g_layoutBackup = {};
SDL_FingerID g_menuGestureFinger = -1;
float g_menuGestureStartX = 0.0f;
float g_menuGestureStartY = 0.0f;
float g_menuGestureX = 0.0f;
float g_menuGestureY = 0.0f;
bool g_menuGestureCancelled = false;
int g_activeTouchCount = 0;
bool g_settingsNeedsSave = false;
SDL_FingerID g_secondaryFinger = -1;
Uint32 g_secondaryStartedAt = 0;
bool g_secondaryLongPress = false;
bool g_controllerInputReady = false;
u32 g_languageRevision = 1;
bool g_languageRestartRequested = false;
bool g_languageRestartActive = false;
int g_pendingLanguage = -1;
const Uint32 SECONDARY_LONG_PRESS_MS = 280;

struct RectGeometry
{
    float x;
    float y;
    float width;
    float height;
};

struct ControlGeometry
{
    float x;
    float y;
    float radius;
};

float ClampFloat(float value, float minimum, float maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

int LayoutOrientation()
{
    return g_layout.drawableHeight > g_layout.drawableWidth ? 1 : 0;
}

void SetDefaultLayout(NormalizedLayout *layout, bool portrait)
{
    layout->x[CONTROL_INDEX_JOYSTICK] = portrait ? 0.18f : 0.15f;
    layout->y[CONTROL_INDEX_JOYSTICK] = portrait ? 0.82f : 0.78f;
    layout->x[CONTROL_INDEX_SHOOT] = portrait ? 0.86f : 0.90f;
    layout->y[CONTROL_INDEX_SHOOT] = portrait ? 0.73f : 0.69f;
    layout->x[CONTROL_INDEX_BOMB] = portrait ? 0.72f : 0.81f;
    layout->y[CONTROL_INDEX_BOMB] = portrait ? 0.86f : 0.84f;
    layout->x[CONTROL_INDEX_FOCUS] = portrait ? 0.68f : 0.74f;
    layout->y[CONTROL_INDEX_FOCUS] = portrait ? 0.68f : 0.66f;
    layout->x[CONTROL_INDEX_MENU] = 0.92f;
    layout->y[CONTROL_INDEX_MENU] = portrait ? 0.08f : 0.12f;
}

void SetDefaultConfig()
{
    memset(&g_config, 0, sizeof(g_config));
    g_config.magic = MOBILE_CONFIG_MAGIC;
    g_config.version = MOBILE_CONFIG_VERSION;
    g_config.controlMode = CONTROL_HYBRID;
    g_config.buttonScale = 1.0f;
    g_config.dragSensitivity = 1.0f;
    g_config.opacity = 0.62f;
    g_config.showButtons = 1;
    g_config.dragAutoShoot = 1;
    g_config.performanceMode = PERFORMANCE_FULL;
    g_config.showDeveloperButton = 1;
    g_config.language = IOS_LANGUAGE_JP;
    SetDefaultLayout(&g_config.layouts[0], false);
    SetDefaultLayout(&g_config.layouts[1], true);
}

void ApplyPerformanceMode()
{
    if (g_config.performanceMode > PERFORMANCE_SPEED)
        g_config.performanceMode = PERFORMANCE_FULL;

    // main.cpp clears the complete render target every frame on iOS to avoid
    // stale bullets and dialogue fragments. The static HUD must therefore be
    // explicitly redrawn as well, otherwise its cleared region becomes black.
    g_Supervisor.config.options.redrawHUDEveryFrame = 1;
    g_Supervisor.config.options.displayMinimumGraphics = 0;
    if (g_config.performanceMode == PERFORMANCE_SPEED)
    {
        g_Supervisor.config.effectQuality = MINIMUM;
        g_Supervisor.config.options.disableFog = 1;
    }
    else if (g_config.performanceMode == PERFORMANCE_BALANCED)
    {
        g_Supervisor.config.effectQuality = MODERATE;
        g_Supervisor.config.options.disableFog = 0;
    }
    else
    {
        g_Supervisor.config.effectQuality = MAXIMUM;
        g_Supervisor.config.options.disableFog = 0;
    }
    g_Supervisor.fogState = FOG_UNSET;
}

bool GetMobileConfigPath(char *path, size_t size)
{
    const char *home = SDL_getenv("HOME");
    if (home == NULL || home[0] == '\0') return false;
    SDL_snprintf(path, size, "%s/Documents/mobile.cfg", home);
    return true;
}

void SaveMobileConfig()
{
    char path[1024];
    if (!GetMobileConfigPath(path, sizeof(path))) return;
    FILE *file = fopen(path, "wb");
    if (file == NULL)
    {
        modern::LogStartup("touch/config: save failed");
        return;
    }
    const bool saved = fwrite(&g_config, sizeof(g_config), 1, file) == 1;
    fclose(file);
    modern::LogStartup(saved ? "touch/config: saved" : "touch/config: short write");
}

void EnsureMobileConfig()
{
    if (g_configLoaded) return;
    g_configLoaded = true;
    SetDefaultConfig();
    char path[1024];
    if (GetMobileConfigPath(path, sizeof(path)))
    {
        FILE *file = fopen(path, "rb");
        MobileConfig stored = g_config;
        if (file != NULL)
        {
            const size_t loadedBytes = fread(&stored, 1, sizeof(stored), file);
            fclose(file);
            const size_t legacySize = offsetof(MobileConfig, showDeveloperButton);
            if (loadedBytes >= legacySize && stored.magic == MOBILE_CONFIG_MAGIC &&
                (stored.version >= 1 && stored.version <= MOBILE_CONFIG_VERSION))
            {
                const u32 storedVersion = stored.version;
                g_config = stored;
                g_config.version = MOBILE_CONFIG_VERSION;
                if (storedVersion < 3)
                    g_config.dragAutoShoot = 1;
                if (storedVersion < 4 || loadedBytes < sizeof(stored))
                    g_config.showDeveloperButton = 1;
            }
        }
    }
    g_config.controlMode = g_config.controlMode < CONTROL_JOYSTICK ||
                                   g_config.controlMode > CONTROL_HYBRID
                               ? CONTROL_HYBRID
                               : g_config.controlMode;
    g_config.buttonScale = ClampFloat(g_config.buttonScale, 0.65f, 1.45f);
    g_config.dragSensitivity = ClampFloat(g_config.dragSensitivity, 0.40f, 2.00f);
    g_config.opacity = ClampFloat(g_config.opacity, 0.20f, 1.00f);
    if (g_config.language > IOS_LANGUAGE_EN)
        g_config.language = IOS_LANGUAGE_JP;
#ifdef TH095_IOS
    // Simulator smoke tests can select a language without manufacturing a
    // binary mobile.cfg. Release launches do not set this variable.
    const char *languageOverride = SDL_getenv("TH095_IOS_LANGUAGE");
    if (languageOverride != NULL)
    {
        if (strcmp(languageOverride, "zh") == 0)
            g_config.language = IOS_LANGUAGE_ZH;
        else if (strcmp(languageOverride, "en") == 0)
            g_config.language = IOS_LANGUAGE_EN;
        else if (strcmp(languageOverride, "ja") == 0)
            g_config.language = IOS_LANGUAGE_JP;
    }
#endif
    ApplyPerformanceMode();
    modern::LogStartup("touch/config: ready");
}

namespace
{
const char *g_pauseMenuText[3][10] = {
    {"Pause", "Return to Game", "Quit", "Restart", "Confirm", "Yes", "No", "Difficulty", "Practice", "Slow Mode"},
    {"暂停", "返回游戏", "退出", "重新开始", "确认", "是", "否", "难度", "练习模式", "慢速模式"},
    {"Pause", "Resume", "Quit", "Restart", "Confirm", "Yes", "No", "Difficulty", "Practice", "Slow Mode"},
};

const char *g_retryMenuText[3][3] = {
    {"Retry", "Yes", "No"},
    {"重新开始", "是", "否"},
    {"Retry", "Yes", "No"},
};
}

ControlGeometry MakeControl(int index, float radiusScale)
{
    EnsureMobileConfig();
    const float width = g_layout.drawableWidth > 0 ? static_cast<float>(g_layout.drawableWidth) : 640.0f;
    const float height = g_layout.drawableHeight > 0 ? static_cast<float>(g_layout.drawableHeight) : 480.0f;
    const float scale = width < height ? width : height;
    const NormalizedLayout &layout = g_config.layouts[LayoutOrientation()];
    ControlGeometry geometry = {layout.x[index] * width, layout.y[index] * height,
                                radiusScale * scale * g_config.buttonScale};
    return geometry;
}

bool HitControl(const SDL_TouchFingerEvent &finger, const ControlGeometry &control, float hitScale)
{
    const float width = g_layout.drawableWidth > 0 ? static_cast<float>(g_layout.drawableWidth) : 640.0f;
    const float height = g_layout.drawableHeight > 0 ? static_cast<float>(g_layout.drawableHeight) : 480.0f;
    const float dx = finger.x * width - control.x;
    const float dy = finger.y * height - control.y;
    const float radius = control.radius * hitScale;
    return dx * dx + dy * dy <= radius * radius;
}

ControlGeometry JoystickGeometry() { return MakeControl(CONTROL_INDEX_JOYSTICK, 0.105f); }
ControlGeometry ShotGeometry() { return MakeControl(CONTROL_INDEX_SHOOT, 0.075f); }
ControlGeometry BombGeometry() { return MakeControl(CONTROL_INDEX_BOMB, 0.065f); }
ControlGeometry FocusGeometry() { return MakeControl(CONTROL_INDEX_FOCUS, 0.060f); }
ControlGeometry MenuGeometry() { return MakeControl(CONTROL_INDEX_MENU, 0.052f); }

RectGeometry DeveloperButtonGeometry()
{
    const float width = g_layout.drawableWidth > 0 ? static_cast<float>(g_layout.drawableWidth) : 640.0f;
    const float height = g_layout.drawableHeight > 0 ? static_cast<float>(g_layout.drawableHeight) : 480.0f;
    const float scale = width < height ? width : height;
    const float buttonWidth = scale * 0.16f;
    const float buttonHeight = scale * 0.072f;
    RectGeometry result = {width - buttonWidth - scale * 0.035f,
                           height * 0.225f, buttonWidth, buttonHeight};
    return result;
}

RectGeometry DeveloperPanelGeometry()
{
    const float width = g_layout.drawableWidth > 0 ? static_cast<float>(g_layout.drawableWidth) : 640.0f;
    const float height = g_layout.drawableHeight > 0 ? static_cast<float>(g_layout.drawableHeight) : 480.0f;
    const bool portrait = height > width;
    const float panelWidth = width * (portrait ? 0.88f : 0.60f);
    const float panelHeight = height * (portrait ? 0.76f : 0.84f);
    RectGeometry result = {(width - panelWidth) * 0.5f, (height - panelHeight) * 0.5f,
                           panelWidth, panelHeight};
    return result;
}

RectGeometry DeveloperRowGeometry(int row)
{
    const RectGeometry panel = DeveloperPanelGeometry();
    const float top = panel.y + panel.height * 0.155f;
    const float rowPitch = panel.height * 0.098f;
    RectGeometry result = {panel.x + panel.width * 0.065f, top + row * rowPitch,
                           panel.width * 0.87f, rowPitch * 0.78f};
    return result;
}

RectGeometry SettingsButtonGeometry()
{
    const float width = g_layout.drawableWidth > 0 ? static_cast<float>(g_layout.drawableWidth) : 640.0f;
    const float height = g_layout.drawableHeight > 0 ? static_cast<float>(g_layout.drawableHeight) : 480.0f;
    const float scale = width < height ? width : height;
    RectGeometry result = {scale * 0.035f, scale * 0.035f, scale * 0.17f, scale * 0.075f};
    return result;
}

RectGeometry PerformanceButtonGeometry()
{
    RectGeometry result = SettingsButtonGeometry();
    result.y += result.height * 1.22f;
    return result;
}

RectGeometry SettingsPanelGeometry()
{
    const float width = g_layout.drawableWidth > 0 ? static_cast<float>(g_layout.drawableWidth) : 640.0f;
    const float height = g_layout.drawableHeight > 0 ? static_cast<float>(g_layout.drawableHeight) : 480.0f;
    const bool portrait = height > width;
    const float panelWidth = width * (portrait ? 0.92f : 0.72f);
    const float panelHeight = height * (portrait ? 0.84f : 0.92f);
    RectGeometry result = {(width - panelWidth) * 0.5f, (height - panelHeight) * 0.5f,
                           panelWidth, panelHeight};
    return result;
}

RectGeometry SettingsRowGeometry(int row)
{
    const RectGeometry panel = SettingsPanelGeometry();
    const float top = panel.y + panel.height * 0.14f;
    const float pitch = panel.height * 0.073f;
    RectGeometry result = {panel.x + panel.width * 0.055f, top + row * pitch,
                           panel.width * 0.89f, pitch * 0.78f};
    return result;
}

RectGeometry LayoutToolGeometry(int index)
{
    const float width = g_layout.drawableWidth > 0 ? static_cast<float>(g_layout.drawableWidth) : 640.0f;
    const float height = g_layout.drawableHeight > 0 ? static_cast<float>(g_layout.drawableHeight) : 480.0f;
    const float scale = width < height ? width : height;
    const float toolWidth = scale * 0.18f;
    const float toolHeight = scale * 0.075f;
    RectGeometry result = {width * 0.5f + (index - 1.0f) * toolWidth * 1.08f - toolWidth * 0.5f,
                           scale * 0.03f, toolWidth, toolHeight};
    return result;
}

bool HitRect(const SDL_TouchFingerEvent &finger, const RectGeometry &rect)
{
    const float width = g_layout.drawableWidth > 0 ? static_cast<float>(g_layout.drawableWidth) : 640.0f;
    const float height = g_layout.drawableHeight > 0 ? static_cast<float>(g_layout.drawableHeight) : 480.0f;
    const float x = finger.x * width;
    const float y = finger.y * height;
    return x >= rect.x && x <= rect.x + rect.width &&
           y >= rect.y && y <= rect.y + rect.height;
}

bool ContainsPoint(const RectGeometry &rect, float x, float y)
{
    return x >= rect.x && x <= rect.x + rect.width &&
           y >= rect.y && y <= rect.y + rect.height;
}

void FingerPixels(const SDL_TouchFingerEvent &finger, float *x, float *y)
{
    const float width = g_layout.drawableWidth > 0 ? static_cast<float>(g_layout.drawableWidth) : 640.0f;
    const float height = g_layout.drawableHeight > 0 ? static_cast<float>(g_layout.drawableHeight) : 480.0f;
    *x = finger.x * width;
    *y = finger.y * height;
}

int SettingsRowAt(float x, float y)
{
    const int rowCount = g_performanceOpen ? 7 : 11;
    for (int row = 0; row < rowCount; ++row)
        if (ContainsPoint(SettingsRowGeometry(row), x, y)) return row;
    return -1;
}

int HitLayoutControl(const SDL_TouchFingerEvent &finger)
{
    if (HitControl(finger, JoystickGeometry(), 1.35f)) return CONTROL_INDEX_JOYSTICK;
    if (HitControl(finger, ShotGeometry(), 1.35f)) return CONTROL_INDEX_SHOOT;
    if (HitControl(finger, BombGeometry(), 1.35f)) return CONTROL_INDEX_BOMB;
    if (HitControl(finger, FocusGeometry(), 1.35f)) return CONTROL_INDEX_FOCUS;
    if (HitControl(finger, MenuGeometry(), 1.45f)) return CONTROL_INDEX_MENU;
    return -1;
}

void ClearTransientTouchState()
{
    g_state.moveFinger = -1;
    g_state.shotFinger = -1;
    g_state.bombFinger = -1;
    g_state.focusFinger = -1;
    g_state.menuFinger = -1;
    g_state.moveDx = g_state.moveDy = 0.0f;
    g_state.dragDeltaX = g_state.dragDeltaY = 0.0f;
    g_state.moveIsDrag = false;
    g_state.buttons = 0;
    g_settingsFinger = -1;
    g_settingsRow = -1;
    g_layoutFinger = -1;
    g_layoutControl = -1;
    g_menuGestureFinger = -1;
    g_menuGestureCancelled = false;
    g_secondaryFinger = -1;
    g_secondaryStartedAt = 0;
    g_secondaryLongPress = false;
    g_activeTouchCount = 0;
    g_settingsNeedsSave = false;
}

void QueuePulse(u16 buttons)
{
    g_pulseButtons |= buttons;
}

void ActivateSettingsRow(int row, float x)
{
    EnsureMobileConfig();
    if (row < 0) return;
    if (g_performanceOpen)
    {
        if (row >= 0 && row <= 2)
        {
            g_config.performanceMode = static_cast<u8>(row);
            ApplyPerformanceMode();
            SaveMobileConfig();
            modern::LogStartup(row == PERFORMANCE_FULL
                                   ? "touch/performance: full"
                                   : (row == PERFORMANCE_BALANCED
                                          ? "touch/performance: balanced"
                                          : "touch/performance: speed"));
        }
        else if (row == 3)
        {
            g_config.showDeveloperButton = !g_config.showDeveloperButton;
            if (!g_config.showDeveloperButton)
                g_developerOpen = false;
            SaveMobileConfig();
            modern::LogStartup(g_config.showDeveloperButton
                                   ? "touch/performance: developer button shown"
                                   : "touch/performance: developer button hidden");
        }
        else if (row == 4)
        {
            ShowCheatCodeDialog();
            modern::LogStartup("cheat-dialog: requested from performance menu");
        }
        else if (row == 5)
        {
            CycleLanguage();
        }
        else if (row == 6)
        {
            g_settingsOpen = false;
            g_performanceOpen = false;
            ClearTransientTouchState();
            SaveMobileConfig();
            modern::LogStartup("touch/performance: closed");
        }
        return;
    }
    const RectGeometry rect = SettingsRowGeometry(row);
    switch (row)
    {
    case 0:
        g_config.controlMode = (g_config.controlMode + 1) % 3;
        break;
    case 1:
        g_config.showButtons = !g_config.showButtons;
        break;
    case 2:
        g_config.buttonScale = 0.65f + ClampFloat((x - rect.x) / rect.width, 0.0f, 1.0f) * 0.80f;
        g_settingsNeedsSave = true;
        break;
    case 3:
        g_config.dragSensitivity = 0.40f + ClampFloat((x - rect.x) / rect.width, 0.0f, 1.0f) * 1.60f;
        g_settingsNeedsSave = true;
        break;
    case 4:
        g_config.opacity = 0.20f + ClampFloat((x - rect.x) / rect.width, 0.0f, 1.0f) * 0.80f;
        g_settingsNeedsSave = true;
        break;
    case 5:
        g_config.dragAutoShoot = !g_config.dragAutoShoot;
        break;
    case 6:
        g_config.shootToggle = !g_config.shootToggle;
        if (!g_config.shootToggle) g_shootLatched = false;
        break;
    case 7:
        g_config.focusToggle = !g_config.focusToggle;
        if (!g_config.focusToggle) g_focusLatched = false;
        break;
    case 8:
        g_config.autoBomb = !g_config.autoBomb;
        break;
    case 9:
        g_layoutBackup = g_config;
        g_layoutEdit = true;
        g_settingsOpen = false;
        g_performanceOpen = false;
        ClearTransientTouchState();
        modern::LogStartup("touch/layout: edit opened");
        return;
    case 10:
        g_settingsOpen = false;
        ClearTransientTouchState();
        SaveMobileConfig();
        modern::LogStartup("touch/settings: closed");
        return;
    default:
        return;
    }
    if (row < 2 || row > 4)
        SaveMobileConfig();
}

bool IsGameplayReady()
{
    return g_Supervisor.curState == SupervisorState_GameManager && g_GameManager.globals != NULL;
}

bool IsStageClearSequenceActive()
{
    // The result screen runs while the supervisor is still in gameplay. The
    // original game allows a manually held Z to speed up the clock tally, but
    // mobile toggle/drag shooting must not hold Z through the entire result.
    return IsGameplayReady() && g_GameManager.flags.unk9 != 0;
}

bool IsGameplayMenuOpen()
{
    return IsGameplayReady() &&
           (g_GameManager.isInGameMenu != 0 || g_GameManager.showRetryMenu != 0);
}

bool IsDialogueActive()
{
    return IsGameplayReady() && g_Gui.IsDialogPresent();
}

void EnsureControllerInput()
{
    if (g_controllerInputReady)
        return;

    g_controllerInputReady = true;
    // CreateWindowExA initializes SDL_INIT_GAMECONTROLLER on iOS.  Explicitly
    // enabling both event streams also covers keyboards and generic HID pads
    // that SDL does not map to a named GameController profile.
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0)
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    if (SDL_WasInit(SDL_INIT_JOYSTICK) == 0)
        SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
    const int deviceCount = SDL_NumJoysticks();
    char message[96];
    SDL_snprintf(message, sizeof(message), "input/controller: ready devices=%d", deviceCount);
    modern::LogStartup(message);
}

u16 ControllerDirectionBits()
{
    u16 result = g_state.controllerButtons & TH_BUTTON_DIRECTION;
    const Sint16 threshold = 8000;
    if (g_state.controllerAxisX <= -threshold)
        result |= TH_BUTTON_LEFT;
    else if (g_state.controllerAxisX >= threshold)
        result |= TH_BUTTON_RIGHT;
    if (g_state.controllerAxisY <= -threshold)
        result |= TH_BUTTON_UP;
    else if (g_state.controllerAxisY >= threshold)
        result |= TH_BUTTON_DOWN;
    return result;
}

void UpdateSecondaryGesture()
{
    if (g_secondaryFinger < 0 || g_secondaryLongPress)
        return;
    if (SDL_GetTicks() - g_secondaryStartedAt >= SECONDARY_LONG_PRESS_MS)
    {
        g_secondaryLongPress = true;
        g_state.buttons |= TH_BUTTON_FOCUS;
        modern::LogStartup("touch/gesture: two-finger long press -> focus");
    }
}

void ClearGameplayTouches()
{
    const u16 keyboardButtons = g_state.keyboardButtons;
    const u16 controllerButtons = g_state.controllerButtons;
    const Sint16 controllerAxisX = g_state.controllerAxisX;
    const Sint16 controllerAxisY = g_state.controllerAxisY;
    const float sensitivity = g_state.sensitivity;
    g_state = TouchState();
    g_state.keyboardButtons = keyboardButtons;
    g_state.controllerButtons = controllerButtons;
    g_state.controllerAxisX = controllerAxisX;
    g_state.controllerAxisY = controllerAxisY;
    g_state.sensitivity = sensitivity;
}

void RefreshDeveloperHud()
{
    g_Gui.flags.lifeDisplayUpdateFrames = 2;
    g_Gui.flags.bombDisplayUpdateFrames = 2;
    g_Gui.flags.powerDisplayUpdateFrames = 2;
    g_Gui.flags.grazeDisplayUpdateFrames = 2;
    g_Gui.flags.pointDisplayUpdateFrames = 2;
    g_Gui.flags.timeDisplayUpdateFrames = 2;
}

void SetMaximumScore()
{
    ZunGlobals *globals = g_GameManager.globals;
    globals->score = 999999999U;
    globals->displayScore = 999999999U;
    globals->displayedHighScore = 999999999U;
    globals->unk0x10 = 0;
}

void SetMaximumItems()
{
    ZunGlobals *globals = g_GameManager.globals;
    globals->graze = 999999;
    globals->grazeInStage = 99999;
    globals->pointItemsCollected = 9999;
    globals->pointItemsCollectedInStage = 9999;
    globals->currentTimeOrbs = 99999;
    globals->totalTimeOrbs = 99999;
    globals->pointItemValue = 999999;
}

void ActivateDeveloperRow(int row)
{
    if (row == 7)
    {
        g_developerOpen = false;
        ClearGameplayTouches();
        modern::LogStartup("developer: panel closed");
        return;
    }

    if (!IsGameplayReady())
    {
        g_developerOpen = false;
        ClearGameplayTouches();
        modern::LogStartup("developer: action ignored outside gameplay");
        return;
    }

    switch (row)
    {
    case 0:
        g_developerInvincible = !g_developerInvincible;
        break;
    case 1:
        SetMaximumScore();
        g_GameManager.UpdateAntiTamper();
        break;
    case 2:
        SetMaximumItems();
        g_GameManager.UpdateAntiTamper();
        break;
    case 3:
        g_GameManager.SetPower(128);
        break;
    case 4:
        g_GameManager.SetLives(8);
        g_GameManager.SetBombCount(8);
        break;
    case 5:
        SetMaximumScore();
        SetMaximumItems();
        g_GameManager.globals->livesRemaining = 8.0f;
        g_GameManager.globals->bombsRemaining = 8.0f;
        g_GameManager.globals->playerPower = 128.0f;
        g_GameManager.UpdateAntiTamper();
        break;
    case 6:
        g_BulletManager.RemoveAllBullets(1);
        break;
    default:
        return;
    }

    RefreshDeveloperHud();
    char message[160];
    SDL_snprintf(message, sizeof(message),
                 "developer: row=%d invincible=%d score=%u graze=%d power=%d lives=%d bombs=%d",
                 row, g_developerInvincible ? 1 : 0, g_GameManager.globals->score,
                 g_GameManager.globals->graze, g_GameManager.GetPower(),
                 g_GameManager.GetLives(), g_GameManager.GetBombsRemaining());
    modern::LogStartup(message);
}

u16 PollSmokeTestButtons()
{
    static bool initialized;
    static Uint32 startedAt;
    static Uint32 lastPulse = static_cast<Uint32>(-1);
    static Uint32 teardownActionAt;
    static int teardownActionStage;
    static bool pauseProbeSent;
    static bool pauseProbeStartSent;
    static bool pauseProbeMoveSent;
    static bool bombTriggered;
    static bool bombVerified;
    static f32 bombUsageBefore;
    static int developerStage;
    static int titleNavigationStep = -1;
    static bool titlePageOpened;
    static Uint32 titlePageOpenedAt;
    static bool replayCancelSent;
    static bool replayReturnLogged;
    static int invincibilityTestStage;
    static bool finalDeathTriggered;
    static bool autoBombTriggered;
    static int campaignLastState = -999;
    static int campaignLastStage = -999;
    static unsigned long campaignStageFrames;
    static unsigned long campaignInputFrames;
    static u32 campaignStageMask;
    static bool campaignSawEnding;
    static bool campaignCompleted;
    static bool campaignBombVerified;
    static int stageClearInputTestStage;
    static Uint32 stageClearInputTestStartedAt;
    static int stageTransitionTestState;
    static unsigned languageSwitchCount;
    static Uint32 languageSwitchAt;
    static bool languageTestPassed;

    const char *enabled = SDL_getenv("TH095_IOS_SMOKE_TEST");
    if (enabled == NULL || enabled[0] == '\0' || enabled[0] == '0')
        return 0;

    if (!initialized)
    {
        initialized = true;
        startedAt = SDL_GetTicks();
        th095::modern::LogStartup("smoke: automatic menu confirmation enabled");
    }

    const Uint32 elapsed = SDL_GetTicks() - startedAt;
    if (elapsed < 2500)
        return 0;

    // Optional simulator stress: tear down a populated text/game scene, then
    // verify the ordinary language-restart path presents its replacement.
    const char *reloadAt = SDL_getenv("TH095_IOS_SMOKE_RELOAD_AT");
    static bool sceneReloadRequested;
    static bool sceneReloadPassed;
    if (reloadAt != NULL && atoi(reloadAt) > 0)
    {
        if (!sceneReloadRequested && elapsed >= static_cast<Uint32>(atoi(reloadAt)) * 1000u)
        {
            sceneReloadRequested = true;
            th095::modern::LogStartup("smoke-scene-language: requesting reload after scene use");
            CycleLanguage();
            return 0;
        }
        if (sceneReloadRequested)
        {
            if (!sceneReloadPassed && !IsLanguageRestartInProgress() &&
                g_Supervisor.curState == SupervisorState_TitleScreen && g_Supervisor.unk294 == 0)
            {
                sceneReloadPassed = true;
                th095::modern::LogStartup("smoke-scene-language: PASS - replacement title presented");
            }
            return 0;
        }
    }

    const bool testRestart = strcmp(enabled, "restart") == 0;
    const bool testTitle = strcmp(enabled, "title") == 0;
    const bool testPause = strcmp(enabled, "pause") == 0;
    const bool testBomb = strcmp(enabled, "bomb") == 0;
    const bool testDeveloper = strcmp(enabled, "developer") == 0;
    const bool testMusicRoom = strcmp(enabled, "musicroom") == 0;
    const bool testReplay = strcmp(enabled, "replay") == 0;
    const bool testResult = strcmp(enabled, "result") == 0;
    const bool testPractice = strcmp(enabled, "practice") == 0;
    const bool testInvincible = strcmp(enabled, "invincible") == 0;
    const bool testRetry = strcmp(enabled, "retry") == 0;
    const bool testAutoBomb = strcmp(enabled, "autobomb") == 0;
    const bool testCampaign = strcmp(enabled, "campaign") == 0;
    const bool testStageClearInput = strcmp(enabled, "stageclear-input") == 0;
    const bool testStageTransition = strcmp(enabled, "stage-transition") == 0;
    const bool testLanguage = strcmp(enabled, "language") == 0;

    // The pause probe owns the initial title-menu input.  Generic smoke
    // confirmation pulses can otherwise land on Replay or Music Room when a
    // save file has unlocked extra entries, making the pause result invalid.
    if (testPause)
    {
        if (elapsed >= 5000 && elapsed < 20000 &&
            g_Supervisor.curState == SupervisorState_TitleScreen &&
            ((elapsed / 1000u) & 1u) == 0u)
        {
            if (!pauseProbeStartSent)
            {
                pauseProbeStartSent = true;
                th095::modern::LogStartup("smoke-pause: starting from title");
            }
            return TH_BUTTON_SHOOT;
        }
        if (elapsed >= 18000 && IsGameplayReady() && !IsGameplayMenuOpen())
        {
            if (!pauseProbeSent)
            {
                pauseProbeSent = true;
                // Enter through the same GameManager flag used by the real
                // MENU edge. The title/stage intro can consume an input edge
                // before GameManager::OnUpdate reaches its pause branch, so
                // the deterministic probe arms the state once gameplay is
                // ready and then lets the normal PauseMenu chain render it.
                g_GameManager.isInGameMenu = 1;
                th095::modern::LogStartup("smoke-pause: opening pause menu");
            }
            return 0;
        }
        if (elapsed >= 22000 && pauseProbeSent && !pauseProbeMoveSent &&
            IsGameplayMenuOpen())
        {
            pauseProbeMoveSent = true;
            th095::modern::LogStartup("smoke-pause: moving selection down");
            return TH_BUTTON_DOWN;
        }
        return 0;
    }

    if (testStageTransition && elapsed >= 35000 && stageTransitionTestState == 0 &&
        IsGameplayReady() && g_GameManager.currentStage == STAGE1 &&
        g_GameManager.unk38 == 0)
    {
        stageTransitionTestState = 1;
        g_GameManager.flags.unk9 = 1;
        g_GameManager.flags.unk5_6 = 2;
        th095::modern::LogStartup(
            "smoke-stage-transition: requested authentic GameManager reinit from stage 1");
        return 0;
    }
    if (testStageTransition && stageTransitionTestState == 1 &&
        IsGameplayReady() && g_GameManager.currentStage == STAGE2 &&
        g_GameManager.unk38 == 0)
    {
        stageTransitionTestState = 2;
        th095::modern::LogStartup(
            "smoke-stage-transition: PASS - stage 2 gameplay became ready");
        return 0;
    }

    if (testLanguage)
    {
        if (elapsed >= 8000 && elapsed - languageSwitchAt >= 8000 &&
            g_Supervisor.curState == SupervisorState_TitleScreen &&
            !IsLanguageRestartInProgress() && g_Supervisor.unk294 == 0)
        {
            if (languageSwitchCount < 6)
            {
                ++languageSwitchCount;
                languageSwitchAt = elapsed;
                char message[128];
                SDL_snprintf(message, sizeof(message), "smoke-language: switch %u/6 from=%s",
                             languageSwitchCount, LanguageName());
                th095::modern::LogStartup(message);
                CycleLanguage();
            }
            else if (!languageTestPassed)
            {
                languageTestPassed = true;
                th095::modern::LogStartup("smoke-language: PASS - six reloads and title frames presented");
            }
        }
        return 0;
    }

    if (testCampaign)
    {
        if (g_Supervisor.curState != campaignLastState)
        {
            campaignLastState = g_Supervisor.curState;
            char message[128];
            SDL_snprintf(message, sizeof(message), "smoke-campaign: supervisor state=%d",
                         campaignLastState);
            th095::modern::LogStartup(message);
        }

        if (g_Supervisor.curState == SupervisorState_Ending)
        {
            if (!campaignSawEnding)
            {
                char message[192];
                SDL_snprintf(message, sizeof(message),
                             "smoke-campaign: ending reached stages=%08x bomb=%d",
                             static_cast<unsigned>(campaignStageMask),
                             campaignBombVerified ? 1 : 0);
                th095::modern::LogStartup(message);
            }
            campaignSawEnding = true;
        }
        else if (campaignSawEnding && g_Supervisor.curState == SupervisorState_TitleScreen &&
                 !campaignCompleted)
        {
            campaignCompleted = true;
            const bool routeComplete =
                (campaignStageMask & (1u << STAGE1)) != 0 &&
                (campaignStageMask & (1u << STAGE2)) != 0 &&
                (campaignStageMask & (1u << STAGE3)) != 0 &&
                (campaignStageMask & (1u << STAGE5)) != 0 &&
                (campaignStageMask & ((1u << STAGE4A) | (1u << STAGE4B))) != 0 &&
                (campaignStageMask & ((1u << STAGE6A) | (1u << STAGE6B))) != 0;
            if (routeComplete && campaignBombVerified)
                th095::modern::LogStartup(
                    "smoke-campaign: COMPLETE - six-stage route and ending returned to title");
            else
            {
                char message[192];
                SDL_snprintf(message, sizeof(message),
                             "smoke-campaign: FAILED - stages=%08x bomb=%d",
                             static_cast<unsigned>(campaignStageMask),
                             campaignBombVerified ? 1 : 0);
                th095::modern::LogStartup(message);
            }
        }

        if (IsGameplayReady())
        {
            if (g_GameManager.currentStage != campaignLastStage)
            {
                campaignLastStage = g_GameManager.currentStage;
                campaignStageFrames = 0;
                if (campaignLastStage >= 0 && campaignLastStage < 32)
                    campaignStageMask |= 1u << campaignLastStage;
                char message[160];
                SDL_snprintf(message, sizeof(message),
                             "smoke-campaign: stage=%d difficulty=%d shot=%d entered",
                             campaignLastStage, g_GameManager.difficulty,
                             static_cast<int>(g_GameManager.shotType));
                th095::modern::LogStartup(message);
                if (!g_developerInvincible)
                    ActivateDeveloperRow(0);
                ActivateDeveloperRow(5);
            }

            ++campaignStageFrames;
            ++campaignInputFrames;
            // GameManager initialization follows the first ready-state signal.
            // Apply the smoke loadout once gameplay has actually advanced.
            if (campaignStageFrames == 60)
                ActivateDeveloperRow(5);
            if (IsStageClearSequenceActive())
                return 0;

            if (bombTriggered && !campaignBombVerified &&
                g_GameManager.globals->bombsUsed > bombUsageBefore)
            {
                campaignBombVerified = true;
                th095::modern::LogStartup("smoke-campaign: bomb verified");
            }
            if (campaignStageFrames == 600)
            {
                bombUsageBefore = g_GameManager.globals->bombsUsed;
                bombTriggered = true;
                th095::modern::LogStartup("smoke-campaign: exercising bomb");
                return TH_BUTTON_BOMB;
            }

            // Release Z for one frame at regular intervals. This keeps normal
            // shooting active while providing fresh confirm edges for stage
            // dialogue and post-boss transitions.
            return (campaignInputFrames % 30u) == 0u ? 0 : TH_BUTTON_SHOOT;
        }

        // Title, ending and result screens require edge-triggered confirms.
        // Real-time pacing prevents unbounded smoke runs from stepping through
        // several menus in one animation frame.
        return ((elapsed / 500u) & 1u) != 0u ? TH_BUTTON_SHOOT : 0;
    }

    if ((testMusicRoom || testReplay || testResult || testPractice) && !titlePageOpened)
    {
        // Follow the actual cursor; locked entries vary with the user's save.
        // Counting downward presses could silently test Result as Music Room.
        if (elapsed < 8000)
            return 0;
        const int cursor = TitleScreen::IosReadyStartMenuCursor();
        const int target = testMusicRoom ? 6 : (testResult ? 5 : (testReplay ? 4 : 3));
        const int step = static_cast<int>(elapsed / 800);
        if (cursor >= 0 && step != titleNavigationStep)
        {
            titleNavigationStep = step;
            if (cursor != target)
                return TH_BUTTON_DOWN;
            titlePageOpened = true;
            titlePageOpenedAt = elapsed;
            char message[112];
            SDL_snprintf(message, sizeof(message), "smoke-%s: confirming actual title cursor=%d", enabled, cursor);
            th095::modern::LogStartup(message);
            return TH_BUTTON_SHOOT;
        }
        return 0;
    }

    if (testMusicRoom)
        return 0;

    if (testReplay)
    {
        if (elapsed - titlePageOpenedAt >= 6000 && !replayCancelSent)
        {
            replayCancelSent = true;
            th095::modern::LogStartup("smoke-replay: requesting title return");
            return TH_BUTTON_BOMB;
        }
        if (replayCancelSent && !replayReturnLogged && TitleScreen::IosReadyStartMenuCursor() >= 0)
        {
            replayReturnLogged = true;
            th095::modern::LogStartup("smoke-replay: main loop responsive after return");
        }
        return 0;
    }

    if (testResult || testPractice)
    {
        const char *name = testResult ? "result" : "practice";
        static bool pageResponsiveLogged;
        if (elapsed >= 18000 && !pageResponsiveLogged)
        {
            pageResponsiveLogged = true;
            char message[96];
            SDL_snprintf(message, sizeof(message), "smoke-%s: page remained responsive", name);
            th095::modern::LogStartup(message);
        }
        return 0;
    }

    if (testStageClearInput && elapsed >= 40000 && stageClearInputTestStage == 0 &&
        IsGameplayReady())
    {
        stageClearInputTestStage = 1;
        stageClearInputTestStartedAt = elapsed;
        g_shootLatched = true;
        g_GameManager.flags.unk9 = 1;
        th095::modern::LogStartup(
            "smoke-stage-clear-input: simulated result with Z toggle active");
        return 0;
    }
    if (testStageClearInput && stageClearInputTestStage == 1)
    {
        if (elapsed - stageClearInputTestStartedAt >= 1500)
        {
            g_GameManager.flags.unk9 = 0;
            g_shootLatched = false;
            stageClearInputTestStage = 2;
            th095::modern::LogStartup("smoke-stage-clear-input: state restored");
        }
        return 0;
    }

    if (testDeveloper && elapsed >= 40000 && developerStage == 0 && IsGameplayReady())
    {
        developerStage = 1;
        g_developerOpen = true;
        ClearGameplayTouches();
        th095::modern::LogStartup("smoke-developer: panel opened");
        return 0;
    }
    if (testDeveloper && elapsed >= 55000 && developerStage == 1)
    {
        developerStage = 2;
        ActivateDeveloperRow(5);
        g_developerOpen = false;
        ClearGameplayTouches();
        th095::modern::LogStartup("smoke-developer: max-all applied");
        return 0;
    }

    if (testInvincible && elapsed >= 40000 && invincibilityTestStage == 0 && IsGameplayReady())
    {
        invincibilityTestStage = 1;
        ActivateDeveloperRow(0);
        th095::modern::LogStartup("smoke-invincible: enabled");
        return 0;
    }
    if (testInvincible && elapsed >= 43000 && invincibilityTestStage == 1)
    {
        invincibilityTestStage = 2;
        const i8 stateBefore = g_Player.playerState;
        g_GameManager.RandomizeAntiTamper();
        g_Player.Die();
        const bool passed = g_Player.playerState == stateBefore && !g_GameManager.IsTampered();
        th095::modern::LogStartup(passed
                                     ? "smoke-invincible: death suppressed and integrity valid"
                                     : "smoke-invincible: FAILED");
        return 0;
    }
    if (testRetry && elapsed >= 40000 && !finalDeathTriggered && IsGameplayReady())
    {
        finalDeathTriggered = true;
        g_GameManager.SetLives(0);
        g_GameManager.SetBombCount(0);
        g_GameManager.RandomizeAntiTamper();
        g_Player.Die();
        th095::modern::LogStartup("smoke-retry: forced final death");
        return 0;
    }
    if (testAutoBomb && elapsed >= 40000 && !autoBombTriggered && IsGameplayReady())
    {
        autoBombTriggered = true;
        g_config.autoBomb = 1;
        g_GameManager.SetBombCount(3);
        const i32 bombsBefore = g_GameManager.GetBombsRemaining();
        g_GameManager.RandomizeAntiTamper();
        g_Player.Die();
        th095::modern::LogStartup(g_GameManager.GetBombsRemaining() < bombsBefore &&
                                     g_Player.playerState != PLAYER_STATE_SPAWNING
                                     ? "smoke-auto-bomb: PASS - miss was intercepted"
                                     : "smoke-auto-bomb: FAILED");
        return 0;
    }
    // Short deterministic pause probe used by simulator validation. The
    // normal restart/title probes intentionally wait a full minute so they
    // exercise teardown; this mode stops after opening the menu, allowing a
    // screenshot to capture localized labels without racing the transition.
    if ((testRestart || testTitle) && elapsed >= 60000)
    {
        if (teardownActionStage == 0)
        {
            teardownActionStage = 1;
            teardownActionAt = elapsed;
            th095::modern::LogStartup(testRestart
                                         ? "smoke-restart: opening pause menu"
                                         : "smoke-title: opening pause menu");
            return TH_BUTTON_MENU;
        }
        if (teardownActionStage == 1 && elapsed - teardownActionAt >= 3000)
        {
            teardownActionStage = 2;
            th095::modern::LogStartup(testRestart
                                         ? "smoke-restart: requesting restart"
                                         : "smoke-title: requesting title");
            return testRestart ? TH_BUTTON_RESET : TH_BUTTON_Q;
        }
        return 0;
    }

    if (testBomb && elapsed >= 40000 && !bombTriggered)
    {
        bombTriggered = true;
        bombUsageBefore = IsGameplayReady() ? g_GameManager.globals->bombsUsed : -1.0f;
        th095::modern::LogStartup("smoke-bomb: requesting player bomb");
        return TH_BUTTON_BOMB;
    }
    if (testBomb && bombTriggered && !bombVerified && IsGameplayReady() &&
        g_GameManager.globals->bombsUsed > bombUsageBefore)
    {
        bombVerified = true;
        th095::modern::LogStartup("smoke-bomb: PASS - bomb usage counter increased");
    }

    // Send a fresh confirmation every 1.5 seconds. This is deliberately
    // slower than the title transitions, so each pulse is observed as a new
    // press by WAS_PRESSED. Once gameplay starts, Z simply becomes shooting.
    const Uint32 pulse = (elapsed - 2500) / 1500;
    if (pulse != lastPulse)
    {
        lastPulse = pulse;
        char message[96];
        SDL_snprintf(message, sizeof(message), "smoke: confirm pulse %u", pulse + 1);
        th095::modern::LogStartup(message);
        return TH_BUTTON_SHOOT;
    }

    // Keep every setup menu edge-triggered, then hold Z once the smoke test is
    // safely inside gameplay so player projectiles exercise the draw chain.
    return elapsed >= 30000 ? TH_BUTTON_SHOOT : 0;
}

void SetDirectional(float dx, float dy)
{
    g_state.buttons &= (u16)~TH_BUTTON_DIRECTION;
    dx *= g_state.sensitivity;
    dy *= g_state.sensitivity;
    g_state.moveDx = dx;
    g_state.moveDy = dy;

    const float width = g_layout.drawableWidth > 0 ? static_cast<float>(g_layout.drawableWidth) : 640.0f;
    const float height = g_layout.drawableHeight > 0 ? static_cast<float>(g_layout.drawableHeight) : 480.0f;
    const float scale = width < height ? width : height;
    const float pixelX = dx * width;
    const float pixelY = dy * height;
    const float length = sqrtf(pixelX * pixelX + pixelY * pixelY);
    if (length < scale * 0.025f)
        return;

    // Divide the stick into eight equal angular sectors. The previous
    // per-axis dead zones left only a narrow corner where diagonals worked.
    const float diagonalThreshold = 0.3826834324f; // sin(22.5 degrees)
    const float normalizedX = pixelX / length;
    const float normalizedY = pixelY / length;
    if (normalizedX <= -diagonalThreshold) g_state.buttons |= TH_BUTTON_LEFT;
    if (normalizedX >= diagonalThreshold) g_state.buttons |= TH_BUTTON_RIGHT;
    if (normalizedY <= -diagonalThreshold) g_state.buttons |= TH_BUTTON_UP;
    if (normalizedY >= diagonalThreshold) g_state.buttons |= TH_BUTTON_DOWN;
}

void AccumulateDragMotion(const SDL_TouchFingerEvent &finger)
{
    const float width = g_layout.drawableWidth > 0 ? static_cast<float>(g_layout.drawableWidth) : 640.0f;
    const float height = g_layout.drawableHeight > 0 ? static_cast<float>(g_layout.drawableHeight) : 480.0f;
    const float pixelDx = (finger.x - g_state.moveX) * width;
    const float pixelDy = (finger.y - g_state.moveY) * height;
    float gameDx;
    float gameDy;
    if (height > width && IsGameplayReady())
    {
        const float viewWidth = g_layout.viewportWidth > 0 ? static_cast<float>(g_layout.viewportWidth) : width;
        const float viewHeight = g_layout.viewportHeight > 0 ? static_cast<float>(g_layout.viewportHeight) : height;
        gameDx = pixelDx * 384.0f / viewWidth;
        gameDy = pixelDy * 448.0f / viewHeight;
    }
    else
    {
        const float viewWidth = g_layout.viewportWidth > 0 ? static_cast<float>(g_layout.viewportWidth) : width;
        const float viewHeight = g_layout.viewportHeight > 0 ? static_cast<float>(g_layout.viewportHeight) : height;
        gameDx = pixelDx * 640.0f / viewWidth;
        gameDy = pixelDy * 480.0f / viewHeight;
    }
    gameDx = ClampFloat(gameDx * g_config.dragSensitivity, -64.0f, 64.0f);
    gameDy = ClampFloat(gameDy * g_config.dragSensitivity, -64.0f, 64.0f);
    g_state.dragDeltaX += gameDx;
    g_state.dragDeltaY += gameDy;
    g_state.moveDx = finger.x - g_state.moveX;
    g_state.moveDy = finger.y - g_state.moveY;
    g_state.moveX = finger.x;
    g_state.moveY = finger.y;
}

void BeginGameplayMove(const SDL_TouchFingerEvent &finger, bool directDrag)
{
    g_state.moveFinger = finger.fingerId;
    g_state.moveX = finger.x;
    g_state.moveY = finger.y;
    g_state.moveDx = g_state.moveDy = 0.0f;
    g_state.moveIsDrag = directDrag;
    if (!directDrag) SetDirectional(0.0f, 0.0f);
}

void BeginMenuGesture(const SDL_TouchFingerEvent &finger)
{
    g_menuGestureFinger = finger.fingerId;
    g_menuGestureStartX = g_menuGestureX = finger.x;
    g_menuGestureStartY = g_menuGestureY = finger.y;
    g_menuGestureCancelled = false;
}

void FinishMenuGesture(const SDL_TouchFingerEvent &finger)
{
    if (finger.fingerId != g_menuGestureFinger) return;
    const float dx = g_menuGestureX - g_menuGestureStartX;
    const float dy = g_menuGestureY - g_menuGestureStartY;
    if (!g_menuGestureCancelled)
    {
        const float absX = fabsf(dx);
        const float absY = fabsf(dy);
        if (absX < 0.035f && absY < 0.035f)
        {
            // In buttonless mode a full-screen tap is the only practical way
            // to advance Japanese dialogue.  The game consumes SHOOT for the
            // next message, while ordinary menus still use SELECTMENU.
            QueuePulse(IsDialogueActive() ? TH_BUTTON_SHOOT : TH_BUTTON_SELECTMENU);
        }
        else
        {
            const bool vertical = absY >= absX;
            const u16 direction = vertical
                                      ? (dy < 0.0f ? TH_BUTTON_UP : TH_BUTTON_DOWN)
                                      : (dx < 0.0f ? TH_BUTTON_LEFT : TH_BUTTON_RIGHT);
            QueuePulse(direction);
        }
    }
    g_menuGestureFinger = -1;
}

void HandleFingerDown(const SDL_TouchFingerEvent &finger)
{
    EnsureMobileConfig();
    ++g_activeTouchCount;
    float px, py;
    FingerPixels(finger, &px, &py);

    if (g_layoutEdit)
    {
        if (HitRect(finger, LayoutToolGeometry(0)))
        {
            g_layoutEdit = false;
            g_config.customized[LayoutOrientation()] = 1;
            ClearTransientTouchState();
            SaveMobileConfig();
            modern::LogStartup("touch/layout: saved");
            return;
        }
        if (HitRect(finger, LayoutToolGeometry(1)))
        {
            SetDefaultLayout(&g_config.layouts[LayoutOrientation()], LayoutOrientation() != 0);
            g_config.customized[LayoutOrientation()] = 0;
            SaveMobileConfig();
            return;
        }
        if (HitRect(finger, LayoutToolGeometry(2)))
        {
            g_config = g_layoutBackup;
            g_layoutEdit = false;
            ClearTransientTouchState();
            modern::LogStartup("touch/layout: cancelled");
            return;
        }
        g_layoutControl = HitLayoutControl(finger);
        if (g_layoutControl >= 0) g_layoutFinger = finger.fingerId;
        return;
    }

    if (g_settingsOpen)
    {
        g_settingsFinger = finger.fingerId;
        g_settingsRow = SettingsRowAt(px, py);
        ActivateSettingsRow(g_settingsRow, px);
        return;
    }

    if (g_developerOpen)
    {
        for (int row = 0; row < 8; ++row)
        {
            if (HitRect(finger, DeveloperRowGeometry(row)))
            {
                ActivateDeveloperRow(row);
                return;
            }
        }
        return;
    }

    if (g_Supervisor.curState == SupervisorState_TitleScreen &&
        HitRect(finger, SettingsButtonGeometry()))
    {
        g_performanceOpen = false;
        g_settingsOpen = true;
        ClearTransientTouchState();
        g_settingsFinger = finger.fingerId;
        modern::LogStartup("touch/settings: opened from title");
        return;
    }

    if (g_Supervisor.curState == SupervisorState_TitleScreen &&
        HitRect(finger, PerformanceButtonGeometry()))
    {
        g_performanceOpen = true;
        g_settingsOpen = true;
        ClearTransientTouchState();
        g_settingsFinger = finger.fingerId;
        modern::LogStartup("touch/performance: opened from title");
        return;
    }

    // Pause and retry menus keep the gameplay supervisor state. Route their
    // touches through the menu gesture mapper before Drag mode can capture
    // the finger as player movement.
    if (IsGameplayMenuOpen())
    {
        if (g_menuGestureFinger >= 0)
        {
            g_menuGestureCancelled = true;
            QueuePulse(TH_BUTTON_RETURNMENU);
            modern::LogStartup("touch/menu: two-finger return requested");
            return;
        }
        BeginMenuGesture(finger);
        return;
    }

    // Dialogues must consume a direct tap before the gameplay move/button
    // hit-tests below.  Otherwise Drag mode treats the tap as a move start and
    // a no-button user has no way to advance the message.
    if (IsDialogueActive())
    {
        if (g_menuGestureFinger >= 0)
        {
            g_menuGestureCancelled = true;
            QueuePulse(TH_BUTTON_BOMB);
        }
        else
        {
            BeginMenuGesture(finger);
        }
        return;
    }

    if (IsGameplayReady() && g_config.showDeveloperButton &&
        HitRect(finger, DeveloperButtonGeometry()))
    {
        g_developerOpen = true;
        ClearGameplayTouches();
        modern::LogStartup("developer: panel opened");
        return;
    }

    if (IsGameplayReady() && g_config.showButtons &&
        g_state.shotFinger < 0 && HitControl(finger, ShotGeometry(), 1.35f))
    {
        if (g_config.shootToggle)
            g_shootLatched = !g_shootLatched;
        else
        {
            g_state.shotFinger = finger.fingerId;
            g_state.buttons |= TH_BUTTON_SHOOT;
        }
        return;
    }
    if (IsGameplayReady() && g_config.showButtons &&
        g_state.bombFinger < 0 && HitControl(finger, BombGeometry(), 1.35f))
    {
        g_state.bombFinger = finger.fingerId;
        g_state.buttons |= TH_BUTTON_BOMB;
        return;
    }
    if (IsGameplayReady() && g_config.showButtons &&
        g_state.focusFinger < 0 && HitControl(finger, FocusGeometry(), 1.35f))
    {
        if (g_config.focusToggle)
            g_focusLatched = !g_focusLatched;
        else
        {
            g_state.focusFinger = finger.fingerId;
            g_state.buttons |= TH_BUTTON_FOCUS;
        }
        return;
    }
    if (IsGameplayReady() && g_config.showButtons &&
        g_state.menuFinger < 0 && HitControl(finger, MenuGeometry(), 1.45f))
    {
        g_state.menuFinger = finger.fingerId;
        QueuePulse(TH_BUTTON_MENU);
        return;
    }

    if (IsGameplayReady())
    {
        if (g_state.moveFinger < 0)
        {
            const bool useDrag = g_config.controlMode == CONTROL_DRAG ||
                                 (g_config.controlMode == CONTROL_HYBRID &&
                                  !HitControl(finger, JoystickGeometry(), 1.55f));
            BeginGameplayMove(finger, useDrag);
            return;
        }
        if (g_secondaryFinger < 0)
        {
            // A second finger is a deferred gesture: a short two-finger tap
            // emits Bomb on release, while a hold past the threshold becomes
            // Focus/S.  Deferring the decision removes the old Bomb+Focus
            // overlap and keeps both actions available in buttonless mode.
            g_secondaryFinger = finger.fingerId;
            g_secondaryStartedAt = SDL_GetTicks();
            g_secondaryLongPress = false;
            return;
        }
        QueuePulse(TH_BUTTON_MENU);
        return;
    }

    if (g_menuGestureFinger >= 0)
    {
        g_menuGestureCancelled = true;
        QueuePulse(TH_BUTTON_RETURNMENU);
        return;
    }
    BeginMenuGesture(finger);
}

void HandleFingerMotion(const SDL_TouchFingerEvent &finger)
{
    float px, py;
    FingerPixels(finger, &px, &py);
    if (g_layoutEdit && finger.fingerId == g_layoutFinger && g_layoutControl >= 0)
    {
        NormalizedLayout &layout = g_config.layouts[LayoutOrientation()];
        layout.x[g_layoutControl] = ClampFloat(finger.x, 0.06f, 0.94f);
        layout.y[g_layoutControl] = ClampFloat(finger.y, 0.08f, 0.94f);
        return;
    }
    if (g_settingsOpen && finger.fingerId == g_settingsFinger &&
        g_settingsRow >= 2 && g_settingsRow <= 4)
    {
        ActivateSettingsRow(g_settingsRow, px);
        return;
    }
    if (finger.fingerId == g_menuGestureFinger)
    {
        g_menuGestureX = finger.x;
        g_menuGestureY = finger.y;
        return;
    }
    if (finger.fingerId == g_secondaryFinger)
    {
        UpdateSecondaryGesture();
        return;
    }
    if (finger.fingerId != g_state.moveFinger) return;
    if (g_state.moveIsDrag) AccumulateDragMotion(finger);
    else SetDirectional(finger.x - g_state.moveX, finger.y - g_state.moveY);
}

void HandleFingerUp(const SDL_TouchFingerEvent &finger)
{
    if (g_activeTouchCount > 0) --g_activeTouchCount;
    if (finger.fingerId == g_layoutFinger)
    {
        g_layoutFinger = -1;
        g_layoutControl = -1;
    }
    if (finger.fingerId == g_settingsFinger)
    {
        if (g_settingsNeedsSave)
        {
            SaveMobileConfig();
            g_settingsNeedsSave = false;
        }
        g_settingsFinger = -1;
        g_settingsRow = -1;
    }
    FinishMenuGesture(finger);
    if (finger.fingerId == g_secondaryFinger)
    {
        UpdateSecondaryGesture();
        if (!g_secondaryLongPress)
        {
            QueuePulse(TH_BUTTON_BOMB);
            modern::LogStartup("touch/gesture: two-finger tap -> bomb");
        }
        g_secondaryFinger = -1;
        g_secondaryStartedAt = 0;
        g_secondaryLongPress = false;
        g_state.buttons &= (u16)~TH_BUTTON_FOCUS;
    }
    if (finger.fingerId == g_state.moveFinger)
    {
        g_state.moveFinger = -1;
        g_state.buttons &= (u16)~TH_BUTTON_DIRECTION;
        if (g_config.controlMode == CONTROL_DRAG)
            g_state.buttons &= (u16)~TH_BUTTON_SHOOT;
        g_state.moveDx = 0.0f;
        g_state.moveDy = 0.0f;
        g_state.moveIsDrag = false;
    }
    if (finger.fingerId == g_state.shotFinger)
    {
        g_state.shotFinger = -1;
        g_state.buttons &= (u16)~TH_BUTTON_SHOOT;
    }
    if (finger.fingerId == g_state.bombFinger)
    {
        g_state.bombFinger = -1;
        g_state.buttons &= (u16)~TH_BUTTON_BOMB;
    }
    if (finger.fingerId == g_state.focusFinger)
    {
        g_state.focusFinger = -1;
        g_state.buttons &= (u16)~TH_BUTTON_FOCUS;
    }
    if (finger.fingerId == g_state.menuFinger)
    {
        g_state.menuFinger = -1;
    }
}

void DrawCircle(float x, float y, float radius, GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    const int segments = 32;
    glColor4ub(red, green, blue, alpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= segments; ++i)
    {
        const float angle = static_cast<float>(i) * 6.28318530718f / static_cast<float>(segments);
        glVertex2f(x + cosf(angle) * radius, y + sinf(angle) * radius);
    }
    glEnd();
}

void DrawGlyph(char glyph, float x, float y, float radius)
{
    EnsureMobileConfig();
    const float left = x - radius * 0.34f;
    const float right = x + radius * 0.34f;
    const float top = y - radius * 0.38f;
    const float bottom = y + radius * 0.38f;
    const float middle = y;
    glColor4ub(255, 255, 255, static_cast<GLubyte>(235.0f * g_config.opacity));
    glLineWidth(radius * 0.10f < 2.0f ? 2.0f : radius * 0.10f);
    glBegin(GL_LINES);
    if (glyph == 'Z')
    {
        glVertex2f(left, top); glVertex2f(right, top);
        glVertex2f(right, top); glVertex2f(left, bottom);
        glVertex2f(left, bottom); glVertex2f(right, bottom);
    }
    else if (glyph == 'X')
    {
        glVertex2f(left, top); glVertex2f(right, bottom);
        glVertex2f(right, top); glVertex2f(left, bottom);
    }
    else if (glyph == 'S')
    {
        glVertex2f(right, top); glVertex2f(left, top);
        glVertex2f(left, top); glVertex2f(left, middle);
        glVertex2f(left, middle); glVertex2f(right, middle);
        glVertex2f(right, middle); glVertex2f(right, bottom);
        glVertex2f(right, bottom); glVertex2f(left, bottom);
    }
    glEnd();
}

void DrawMenuGlyph(float x, float y, float radius)
{
    EnsureMobileConfig();
    glColor4ub(255, 255, 255, static_cast<GLubyte>(235.0f * g_config.opacity));
    glLineWidth(radius * 0.10f < 2.0f ? 2.0f : radius * 0.10f);
    glBegin(GL_LINES);
    for (int row = -1; row <= 1; ++row)
    {
        glVertex2f(x - radius * 0.38f, y + row * radius * 0.24f);
        glVertex2f(x + radius * 0.38f, y + row * radius * 0.24f);
    }
    glEnd();
}

void DrawActionButton(const ControlGeometry &geometry, bool pressed, char glyph)
{
    EnsureMobileConfig();
    const GLubyte outerAlpha = static_cast<GLubyte>((pressed ? 220.0f : 150.0f) * g_config.opacity);
    const GLubyte innerAlpha = static_cast<GLubyte>((pressed ? 210.0f : 120.0f) * g_config.opacity);
    DrawCircle(geometry.x, geometry.y, geometry.radius,
               pressed ? 242 : 34, pressed ? 112 : 42, pressed ? 132 : 54,
               outerAlpha);
    DrawCircle(geometry.x, geometry.y, geometry.radius * 0.82f,
               pressed ? 190 : 92, pressed ? 38 : 20, pressed ? 64 : 38,
               innerAlpha);
    if (glyph == 'M')
        DrawMenuGlyph(geometry.x, geometry.y, geometry.radius);
    else
        DrawGlyph(glyph, geometry.x, geometry.y, geometry.radius);
}

void DrawRect(const RectGeometry &rect, GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    glColor4ub(red, green, blue, alpha);
    glBegin(GL_TRIANGLES);
    glVertex2f(rect.x, rect.y);
    glVertex2f(rect.x + rect.width, rect.y);
    glVertex2f(rect.x, rect.y + rect.height);
    glVertex2f(rect.x, rect.y + rect.height);
    glVertex2f(rect.x + rect.width, rect.y);
    glVertex2f(rect.x + rect.width, rect.y + rect.height);
    glEnd();
}

void DrawRectOutline(const RectGeometry &rect, float thickness,
                     GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    RectGeometry top = {rect.x, rect.y, rect.width, thickness};
    RectGeometry bottom = {rect.x, rect.y + rect.height - thickness, rect.width, thickness};
    RectGeometry left = {rect.x, rect.y, thickness, rect.height};
    RectGeometry right = {rect.x + rect.width - thickness, rect.y, thickness, rect.height};
    DrawRect(top, red, green, blue, alpha);
    DrawRect(bottom, red, green, blue, alpha);
    DrawRect(left, red, green, blue, alpha);
    DrawRect(right, red, green, blue, alpha);
}

const u8 *TextGlyph(char glyph)
{
    static const u8 digits[10][7] = {
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
        {6,8,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
        {14,17,17,15,1,2,12}};
    static const u8 letters[26][7] = {
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{15,16,16,16,16,16,15},
        {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
        {15,16,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
        {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,25,21,19,19,17},{14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}};
    static const u8 dash[7] = {0,0,0,31,0,0,0};
    static const u8 slash[7] = {1,2,2,4,8,8,16};
    static const u8 colon[7] = {0,12,12,0,12,12,0};
    static const u8 question[7] = {14,17,1,2,4,0,4};
    if (glyph >= '0' && glyph <= '9') return digits[glyph - '0'];
    if (glyph >= 'a' && glyph <= 'z') glyph = static_cast<char>(glyph - 'a' + 'A');
    if (glyph >= 'A' && glyph <= 'Z') return letters[glyph - 'A'];
    if (glyph == '-') return dash;
    if (glyph == '/') return slash;
    if (glyph == ':') return colon;
    return question;
}

void DrawText(const char *text, float x, float y, float pixel,
              GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    glColor4ub(red, green, blue, alpha);
    glBegin(GL_TRIANGLES);
    for (const char *cursor = text; *cursor != '\0'; ++cursor)
    {
        if (*cursor == ' ')
        {
            x += pixel * 6.0f;
            continue;
        }
        const u8 *glyph = TextGlyph(*cursor);
        for (int row = 0; row < 7; ++row)
        {
            for (int column = 0; column < 5; ++column)
            {
                if ((glyph[row] & (1 << (4 - column))) == 0)
                    continue;
                const float x0 = x + column * pixel;
                const float y0 = y + row * pixel;
                const float x1 = x0 + pixel;
                const float y1 = y0 + pixel;
                glVertex2f(x0, y0);
                glVertex2f(x1, y0);
                glVertex2f(x0, y1);
                glVertex2f(x0, y1);
                glVertex2f(x1, y0);
                glVertex2f(x1, y1);
            }
        }
        x += pixel * 6.0f;
    }
    glEnd();
}

float FitTextPixel(const char *text, float requested, float availableWidth)
{
    const size_t length = strlen(text);
    if (length == 0)
        return requested;
    const float maximum = availableWidth / (static_cast<float>(length) * 6.0f);
    return requested < maximum ? requested : maximum;
}

void DrawDeveloperButton()
{
    const RectGeometry button = DeveloperButtonGeometry();
    DrawRect(button, 88, 20, 62, 205);
    DrawRectOutline(button, button.height * 0.055f, 244, 172, 216, 225);
    const float pixel = FitTextPixel("DEV", button.height * 0.105f, button.width * 0.72f);
    const float textWidth = pixel * 18.0f;
    DrawText("DEV", button.x + (button.width - textWidth) * 0.5f,
             button.y + (button.height - pixel * 7.0f) * 0.5f,
             pixel, 255, 232, 248, 250);
}

void DrawDeveloperPanel()
{
    static const char *labels[8] = {
        "INVINCIBLE", "MAX SCORE", "MAX ITEMS", "MAX POWER",
        "FULL STOCK", "MAX ALL", "CLEAR BULLETS", "CLOSE"};
    const RectGeometry panel = DeveloperPanelGeometry();
    DrawRect(panel, 18, 8, 30, 244);
    DrawRectOutline(panel, panel.width * 0.006f, 190, 112, 174, 238);

    RectGeometry title = {panel.x + panel.width * 0.025f, panel.y + panel.height * 0.025f,
                          panel.width * 0.95f, panel.height * 0.095f};
    DrawRect(title, 64, 15, 62, 238);
    const char *titleText = "IMPERISHABLE NIGHT DEV";
    const float titlePixel = FitTextPixel(titleText, title.height * 0.105f, title.width * 0.90f);
    DrawText(titleText, title.x + title.width * 0.05f,
             title.y + (title.height - titlePixel * 7.0f) * 0.5f,
             titlePixel, 255, 210, 239, 255);

    for (int row = 0; row < 8; ++row)
    {
        const RectGeometry line = DeveloperRowGeometry(row);
        DrawRect(line, row == 7 ? 91 : 50, row == 7 ? 14 : 20,
                 row == 7 ? 43 : 69, 230);
        DrawRectOutline(line, line.height * 0.035f, 216, 128, 194, 205);
        const float pixel = FitTextPixel(labels[row], line.height * 0.105f, line.width * 0.72f);
        DrawText(labels[row], line.x + line.width * 0.055f,
                 line.y + (line.height - pixel * 7.0f) * 0.5f,
                 pixel, 255, 226, 246, 250);
        if (row == 0)
        {
            const char *value = g_developerInvincible ? "ON" : "OFF";
            const float valuePixel = FitTextPixel(value, pixel, line.width * 0.16f);
            const float valueWidth = strlen(value) * valuePixel * 6.0f;
            DrawText(value, line.x + line.width - valueWidth - line.width * 0.055f,
                     line.y + (line.height - valuePixel * 7.0f) * 0.5f,
                     valuePixel, g_developerInvincible ? 142 : 255,
                     g_developerInvincible ? 255 : 172, 190, 255);
        }
    }
}

void DrawSettingsButton()
{
    const RectGeometry button = SettingsButtonGeometry();
    DrawRect(button, 38, 22, 58, 215);
    DrawRectOutline(button, button.height * 0.055f, 202, 144, 222, 230);
    const float pixel = FitTextPixel("CFG", button.height * 0.105f, button.width * 0.72f);
    DrawText("CFG", button.x + button.width * 0.22f,
             button.y + (button.height - pixel * 7.0f) * 0.5f,
             pixel, 255, 236, 255, 250);
}

void DrawPerformanceButton()
{
    const RectGeometry button = PerformanceButtonGeometry();
    DrawRect(button, 29, 36, 55, 215);
    DrawRectOutline(button, button.height * 0.055f, 140, 191, 218, 230);
    const float pixel = FitTextPixel("PERF", button.height * 0.105f, button.width * 0.72f);
    DrawText("PERF", button.x + button.width * 0.14f,
             button.y + (button.height - pixel * 7.0f) * 0.5f,
             pixel, 225, 245, 255, 250);
}

void DrawSlider(const RectGeometry &row, float normalized)
{
    normalized = ClampFloat(normalized, 0.0f, 1.0f);
    RectGeometry track = {row.x + row.width * 0.55f, row.y + row.height * 0.38f,
                          row.width * 0.39f, row.height * 0.24f};
    DrawRect(track, 42, 30, 50, 235);
    RectGeometry fill = track;
    fill.width *= normalized;
    DrawRect(fill, 224, 116, 194, 245);
    DrawRectOutline(track, row.height * 0.025f, 246, 205, 236, 220);
}

void DrawSettingsPanel()
{
    EnsureMobileConfig();
    static const char *labels[11] = {
        "CONTROL", "BUTTONS", "BUTTON SIZE", "DRAG SENS", "OPACITY",
        "DRAG AUTO Z", "Z TOGGLE", "S TOGGLE", "AUTO BOMB", "EDIT LAYOUT", "CLOSE"};
    static const char *performanceLabels[7] = {
        "FULL QUALITY", "BALANCED", "SPEED", "SHOW DEV BUTTON", "CHEAT CODE", "LANGUAGE", "CLOSE"};
    const RectGeometry panel = SettingsPanelGeometry();
    DrawRect(panel, 17, 9, 29, 247);
    DrawRectOutline(panel, panel.width * 0.005f, 192, 119, 181, 242);

    RectGeometry title = {panel.x + panel.width * 0.025f, panel.y + panel.height * 0.022f,
                          panel.width * 0.95f, panel.height * 0.092f};
    DrawRect(title, 61, 20, 64, 242);
    const char *panelTitle = g_performanceOpen ? "PERFORMANCE" : "MOBILE CONTROL";
    const float titlePixel = FitTextPixel(panelTitle, title.height * 0.105f, title.width * 0.88f);
    DrawText(panelTitle, title.x + title.width * 0.06f,
             title.y + (title.height - titlePixel * 7.0f) * 0.5f,
             titlePixel, 255, 222, 247, 255);

    const int rowCount = g_performanceOpen ? 7 : 11;
    for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex)
    {
        const RectGeometry row = SettingsRowGeometry(rowIndex);
        const bool command = rowIndex == rowCount - 1 || (!g_performanceOpen && rowIndex == 9);
        DrawRect(row, command ? 75 : 39, command ? 18 : 20, command ? 50 : 55, 230);
        DrawRectOutline(row, row.height * 0.025f, 184, 108, 171, 195);
        const char *label = g_performanceOpen ? performanceLabels[rowIndex] : labels[rowIndex];
        const float pixel = FitTextPixel(label, row.height * 0.103f, row.width * 0.46f);
        DrawText(label, row.x + row.width * 0.035f,
                 row.y + (row.height - pixel * 7.0f) * 0.5f,
                 pixel, 255, 229, 248, 250);

        const char *value = NULL;
        char numeric[24];
        if (g_performanceOpen && rowIndex < 3)
            value = g_config.performanceMode == rowIndex ? "ACTIVE" : "SELECT";
        else if (g_performanceOpen && rowIndex == 3)
            value = g_config.showDeveloperButton ? "ON" : "OFF";
        else if (g_performanceOpen && rowIndex == 4)
            value = "OPEN";
        else if (g_performanceOpen && rowIndex == 5)
            value = LanguageName();
        else if (g_performanceOpen && rowIndex == 6)
            value = "OPEN";
        else if (!g_performanceOpen && rowIndex == 0)
            value = g_config.controlMode == CONTROL_JOYSTICK ? "JOYSTICK" :
                    (g_config.controlMode == CONTROL_DRAG ? "DRAG" : "HYBRID");
        else if (!g_performanceOpen && rowIndex == 1) value = g_config.showButtons ? "ON" : "OFF";
        else if (!g_performanceOpen && rowIndex == 2)
        {
            SDL_snprintf(numeric, sizeof(numeric), "%d%%", static_cast<int>(g_config.buttonScale * 100.0f));
            value = numeric;
            DrawSlider(row, (g_config.buttonScale - 0.65f) / 0.80f);
        }
        else if (!g_performanceOpen && rowIndex == 3)
        {
            SDL_snprintf(numeric, sizeof(numeric), "%d%%", static_cast<int>(g_config.dragSensitivity * 100.0f));
            value = numeric;
            DrawSlider(row, (g_config.dragSensitivity - 0.40f) / 1.60f);
        }
        else if (!g_performanceOpen && rowIndex == 4)
        {
            SDL_snprintf(numeric, sizeof(numeric), "%d%%", static_cast<int>(g_config.opacity * 100.0f));
            value = numeric;
            DrawSlider(row, (g_config.opacity - 0.20f) / 0.80f);
        }
        else if (!g_performanceOpen && rowIndex == 5) value = g_config.dragAutoShoot ? "ON" : "OFF";
        else if (!g_performanceOpen && rowIndex == 6) value = g_config.shootToggle ? "ON" : "OFF";
        else if (!g_performanceOpen && rowIndex == 7) value = g_config.focusToggle ? "ON" : "OFF";
        else if (!g_performanceOpen && rowIndex == 8) value = g_config.autoBomb ? "ON" : "OFF";

        if (value != NULL && (g_performanceOpen ||
                             (rowIndex != 2 && rowIndex != 3 && rowIndex != 4)))
        {
            const float valuePixel = FitTextPixel(value, pixel, row.width * 0.38f);
            const float valueWidth = strlen(value) * valuePixel * 6.0f;
            DrawText(value, row.x + row.width - valueWidth - row.width * 0.035f,
                     row.y + (row.height - valuePixel * 7.0f) * 0.5f,
                     valuePixel, 240, 181, 226, 255);
        }
    }
}

void DrawLayoutEditor()
{
    static const char *tools[3] = {"SAVE", "RESET", "CANCEL"};
    for (int index = 0; index < 3; ++index)
    {
        const RectGeometry tool = LayoutToolGeometry(index);
        DrawRect(tool, index == 2 ? 91 : 44, index == 2 ? 18 : 25,
                 index == 2 ? 48 : 62, 235);
        DrawRectOutline(tool, tool.height * 0.04f, 229, 154, 211, 230);
        const float pixel = FitTextPixel(tools[index], tool.height * 0.105f, tool.width * 0.82f);
        const float textWidth = strlen(tools[index]) * pixel * 6.0f;
        DrawText(tools[index], tool.x + (tool.width - textWidth) * 0.5f,
                 tool.y + (tool.height - pixel * 7.0f) * 0.5f,
                 pixel, 255, 231, 250, 255);
    }

    const ControlGeometry joystick = JoystickGeometry();
    DrawCircle(joystick.x, joystick.y, joystick.radius, 40, 48, 58, 185);
    DrawCircle(joystick.x, joystick.y, joystick.radius * 0.42f, 226, 170, 214, 220);
    DrawActionButton(ShotGeometry(), false, 'Z');
    DrawActionButton(BombGeometry(), false, 'X');
    DrawActionButton(FocusGeometry(), false, 'S');
    DrawActionButton(MenuGeometry(), false, 'M');
}

u16 KeyboardButtonsForKey(SDL_Keycode key)
{
    switch (key)
    {
    case SDLK_UP:
    case SDLK_w:
    case SDLK_KP_8:
        return TH_BUTTON_UP;
    case SDLK_KP_7:
        return TH_BUTTON_UP_LEFT;
    case SDLK_KP_9:
        return TH_BUTTON_UP_RIGHT;
    case SDLK_DOWN:
    case SDLK_s:
    case SDLK_KP_2:
        return TH_BUTTON_DOWN | TH_BUTTON_S;
    case SDLK_KP_1:
        return TH_BUTTON_DOWN_LEFT;
    case SDLK_KP_3:
        return TH_BUTTON_DOWN_RIGHT;
    case SDLK_LEFT:
    case SDLK_a:
    case SDLK_KP_4:
        return TH_BUTTON_LEFT;
    case SDLK_RIGHT:
    case SDLK_KP_6:
        return TH_BUTTON_RIGHT;
    case SDLK_d:
        // D is the original score-screen cheat key.  Keep it available while
        // also allowing WASD users to use D as the right direction.
        return TH_BUTTON_RIGHT | TH_BUTTON_D;
    case SDLK_z:
    case SDLK_SPACE:
        return TH_BUTTON_SHOOT;
    case SDLK_x:
    case SDLK_c:
        return TH_BUTTON_BOMB;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
    case SDLK_LCTRL:
    case SDLK_RCTRL:
    case SDLK_f:
        return TH_BUTTON_FOCUS;
    case SDLK_ESCAPE:
        return TH_BUTTON_MENU;
    case SDLK_p:
        return TH_BUTTON_HOME;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        return TH_BUTTON_ENTER;
    case SDLK_TAB:
        return TH_BUTTON_SKIP;
    case SDLK_q:
        return TH_BUTTON_Q;
    case SDLK_r:
        return TH_BUTTON_RESET;
    case SDLK_HOME:
        return TH_BUTTON_HOME;
    default:
        return 0;
    }
}

u16 ControllerButtonsForButton(Uint8 button)
{
    switch (button)
    {
    case SDL_CONTROLLER_BUTTON_A:
        return TH_BUTTON_SHOOT;
    case SDL_CONTROLLER_BUTTON_B:
    case SDL_CONTROLLER_BUTTON_X:
        return TH_BUTTON_BOMB;
    case SDL_CONTROLLER_BUTTON_Y:
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        return TH_BUTTON_FOCUS;
    case SDL_CONTROLLER_BUTTON_START:
        return TH_BUTTON_MENU;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        return TH_BUTTON_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return TH_BUTTON_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return TH_BUTTON_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return TH_BUTTON_RIGHT;
    default:
        return 0;
    }
}

u16 JoystickButtonsForButton(Uint8 button)
{
    // These indices match the usual HID/XInput layout.  The named
    // GameController path above is preferred when SDL has a mapping.
    switch (button)
    {
    case 0:
        return TH_BUTTON_SHOOT;
    case 1:
    case 2:
        return TH_BUTTON_BOMB;
    case 3:
    case 4:
        return TH_BUTTON_FOCUS;
    case 7:
        return TH_BUTTON_MENU;
    case 8:
        return TH_BUTTON_UP;
    case 9:
        return TH_BUTTON_DOWN;
    case 10:
        return TH_BUTTON_LEFT;
    case 11:
        return TH_BUTTON_RIGHT;
    default:
        return 0;
    }
}
}

IosLanguage GetLanguage()
{
    EnsureMobileConfig();
    return static_cast<IosLanguage>(g_config.language);
}

const char *LanguageName()
{
    EnsureMobileConfig();
    // The controls overlay intentionally uses its fixed ASCII bitmap font.
    // Translated game text is rendered through the explicit UTF-8 GDI path.
    static const char *names[] = {"Japanese", "Simplified Chinese", "English"};
    return names[g_config.language];
}

void CycleLanguage()
{
    EnsureMobileConfig();
    if (g_languageRestartActive) return;
    // Commit at the frame boundary, after the old-language calc callbacks.
    g_pendingLanguage = (g_config.language + 1) % 3;
    g_languageRestartRequested = true;
    g_languageRestartActive = true;
    modern::LogStartup(g_pendingLanguage == IOS_LANGUAGE_JP
                           ? "language: Japanese"
                           : (g_pendingLanguage == IOS_LANGUAGE_ZH
                                  ? "language: Simplified Chinese"
                                  : "language: English"));
}

u32 LanguageRevision()
{
    return g_languageRevision;
}

bool ConsumeLanguageRestartRequest()
{
    const bool requested = g_languageRestartRequested;
    g_languageRestartRequested = false;
    if (requested && g_pendingLanguage >= 0)
    {
        g_config.language = static_cast<u8>(g_pendingLanguage);
        g_pendingLanguage = -1;
        ++g_languageRevision;
        SaveMobileConfig();
    }
    return requested;
}

bool IsLanguageRestartInProgress()
{
    return g_languageRestartActive;
}

void CompleteLanguageRestart()
{
    g_languageRestartActive = false;
}

const char *PauseMenuText(int index)
{
    EnsureMobileConfig();
    // The retail pause labels are already English. Preserve their exact atlas
    // pixels in Japanese and English modes; only Chinese needs replacement.
    return g_config.language == IOS_LANGUAGE_ZH && index >= 0 && index < 10
               ? g_pauseMenuText[IOS_LANGUAGE_ZH][index]
               : NULL;
}

const char *RetryMenuText(int index)
{
    EnsureMobileConfig();
    return g_config.language == IOS_LANGUAGE_ZH && index >= 0 && index < 3
               ? g_retryMenuText[IOS_LANGUAGE_ZH][index]
               : NULL;
}

bool ApplyIosCheatCode(const char *code)
{
    if (code == NULL || strcmp(code, "ymgjdsh") != 0)
    {
        modern::LogStartup("cheat: rejected code");
        return false;
    }

    // Mirror the original all-clear flag layout: bit 14 exposes Extra and
    // bit 15 exposes Spell Practice.  Setting every stage bit also makes all
    // character teams and stage starts immediately available.
    for (i32 shot = 0; shot < SHOT_ALL + 1; ++shot)
    {
        for (i32 difficulty = 0; difficulty < MAX_DIFFICULTIES; ++difficulty)
        {
            g_GameManager.clrdData[shot].base.magic = CLRD_MAGIC;
            g_GameManager.clrdData[shot].base.unkLen = sizeof(Clrd);
            g_GameManager.clrdData[shot].base.th8kLen = sizeof(Clrd);
            g_GameManager.clrdData[shot].base.version = CLRD_VERSION;
            g_GameManager.clrdData[shot].shotNumber = static_cast<u8>(shot);
            g_GameManager.clrdData[shot].difficultiesClearedWithoutRetries[difficulty] = 0xffff;
            g_GameManager.clrdData[shot].difficultiesClearedWithRetries[difficulty] = 0xffff;
        }
    }

    for (i32 spell = 0; spell < SPELLCARD_COUNT_SPELLCARDS; ++spell)
    {
        if (g_GameManager.catkData[spell].base.magic == CATK_MAGIC)
        {
            for (i32 shot = 0; shot < SHOT_ALL + 1; ++shot)
            {
                g_GameManager.catkData[spell].inGameHistory.attempts[shot] = 1;
                g_GameManager.catkData[spell].inGameHistory.captures[shot] = 1;
                g_GameManager.catkData[spell].spellPracticeHistory.attempts[shot] = 1;
                g_GameManager.catkData[spell].spellPracticeHistory.captures[shot] = 1;
            }
            g_GameManager.catkData2[spell] = g_GameManager.catkData[spell];
        }
    }

    for (i32 lastWord = 0; lastWord < SPELLCARD_COUNT_LAST_WORD_SPELLCARDS; ++lastWord)
    {
        g_GameManager.flsp.unlockedLastWordSpellCards[lastWord] =
            static_cast<BYTE>(SPELLCARD_LAST_WORD_START + lastWord);
    }
    for (i32 bgm = 0; bgm < 32; ++bgm)
        g_GameManager.plst.bgmUnlocked[bgm] = 1;
    for (i32 difficulty = 0; difficulty < MAX_DIFFICULTIES; ++difficulty)
    {
        g_GameManager.plst.playDataByDifficulty[difficulty].attemptsTotal = 1;
        g_GameManager.plst.playDataByDifficulty[difficulty].clears = 1;
    }
    g_GameManager.plst.playDataTotals.attemptsTotal = MAX_DIFFICULTIES;
    g_GameManager.plst.playDataTotals.clears = MAX_DIFFICULTIES;
    g_GameManager.flags.isExtraUnlocked = TRUE;
    g_GameManager.flags.isSpellPracticeUnlocked = TRUE;
    g_GameManager.flags.isExtraUnlockedWithAllTeams = TRUE;

    // Reuse the game's own score serializer so existing high scores and replay
    // metadata are preserved while the updated unlock arrays are encrypted and
    // compressed in the normal score.dat format.
    ResultScreen *writer = new ResultScreen();
    writer->scoreDat = ScoreDat::OpenScore("score.dat");
    if (writer->scoreDat == NULL)
    {
        delete writer;
        modern::LogStartup("cheat: score.dat open failed");
        return false;
    }
    writer->lsnm.base.magic = LSNM_MAGIC;
    writer->lsnm.base.version = LSNM_VERSION;
    writer->lsnm.base.unkLen = sizeof(Lsnm);
    writer->lsnm.base.th8kLen = sizeof(Lsnm);
    strcpy(writer->lsnm.name, "        ");
    ScoreDat::ParseLSNM(writer->scoreDat, &writer->lsnm);
    for (i32 difficulty = 0; difficulty < MAX_DIFFICULTIES; ++difficulty)
    {
        for (i32 shot = 0; shot < SHOT_ALL; ++shot)
            ScoreDat::GetHighScore(writer->scoreDat, &writer->scores[difficulty][shot],
                                   shot, difficulty, NULL);
    }
    ResultScreen::WriteScore(writer);
    for (i32 difficulty = 0; difficulty < MAX_DIFFICULTIES; ++difficulty)
    {
        for (i32 shot = 0; shot < SHOT_ALL; ++shot)
            writer->FreeScore(difficulty, shot);
    }
    delete writer;
    modern::LogStartup("cheat: full unlock applied and score.dat persisted");
    return true;
}

void SetPresentationLayout(const PresentationLayout &layout)
{
    g_layout = layout;
}

PresentationLayout GetPresentationLayout()
{
    return g_layout;
}

void SetPresentationShake(f32 x, f32 y)
{
    g_presentationShakeX = x;
    g_presentationShakeY = y;
}

void GetPresentationShake(f32 *x, f32 *y)
{
    if (x != NULL) *x = g_presentationShakeX;
    if (y != NULL) *y = g_presentationShakeY;
}

void ProcessEvent(const SDL_Event &event)
{
    // SDL can report a hardware Escape press as SDL_QUIT on some iOS
    // keyboard/HID combinations.  SDL_QUIT normally becomes WM_CLOSE and
    // terminates the process, but Escape is a game-menu command here.
    if (event.type == SDL_QUIT)
    {
        QueuePulse(TH_BUTTON_MENU);
        modern::LogStartup("input: converted SDL_QUIT to game menu");
        return;
    }
    if (event.type == SDL_APP_WILLENTERBACKGROUND || event.type == SDL_APP_DIDENTERBACKGROUND)
    {
        if (!g_appInBackground)
        {
            g_appInBackground = true;
            g_SoundPlayer.Pause();
            g_SoundPlayer.ProcessQueues();
            if (IsGameplayReady() && g_GameManager.isInGameMenu == 0 && !g_GameManager.showRetryMenu)
                g_backgroundPausePending = true;
            ClearTransientTouchState();
            g_shootLatched = false;
            g_focusLatched = false;
            modern::LogStartup("lifecycle: background entered; audio paused");
        }
    }
    else if (event.type == SDL_APP_WILLENTERFOREGROUND || event.type == SDL_APP_DIDENTERFOREGROUND)
    {
        if (g_appInBackground)
        {
            g_appInBackground = false;
            if (!IsGameplayReady())
            {
                g_SoundPlayer.UnPause();
                g_SoundPlayer.ProcessQueues();
                modern::LogStartup("lifecycle: foreground entered; menu audio resumed");
            }
            else
            {
                modern::LogStartup("lifecycle: foreground entered; gameplay remains paused");
            }
        }
    }
    else if (event.type == SDL_FINGERDOWN)
        HandleFingerDown(event.tfinger);
    else if (event.type == SDL_FINGERMOTION)
        HandleFingerMotion(event.tfinger);
    else if (event.type == SDL_FINGERUP)
        HandleFingerUp(event.tfinger);
    else if (event.type == SDL_KEYDOWN && event.key.repeat == 0)
    {
        if (g_developerOpen && event.key.keysym.sym == SDLK_ESCAPE)
        {
            g_developerOpen = false;
            ClearGameplayTouches();
            modern::LogStartup("developer: panel closed by keyboard");
            return;
        }
        g_state.keyboardButtons |= KeyboardButtonsForKey(event.key.keysym.sym);
    }
    else if (event.type == SDL_KEYUP)
    {
        g_state.keyboardButtons &= (u16)~KeyboardButtonsForKey(event.key.keysym.sym);
    }
    else if (event.type == SDL_CONTROLLERBUTTONDOWN)
    {
        const u16 mapped = ControllerButtonsForButton(event.cbutton.button);
        if (mapped != 0)
            g_state.controllerButtons |= mapped;
        else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK)
            QueuePulse(TH_BUTTON_RETURNMENU);
    }
    else if (event.type == SDL_CONTROLLERBUTTONUP)
    {
        g_state.controllerButtons &= (u16)~ControllerButtonsForButton(event.cbutton.button);
    }
    else if (event.type == SDL_JOYBUTTONDOWN)
    {
        const u16 mapped = JoystickButtonsForButton(event.jbutton.button);
        if (mapped != 0)
            g_state.controllerButtons |= mapped;
        else if (event.jbutton.button == 6)
            QueuePulse(TH_BUTTON_RETURNMENU);
    }
    else if (event.type == SDL_JOYBUTTONUP)
    {
        g_state.controllerButtons &= (u16)~JoystickButtonsForButton(event.jbutton.button);
    }
    else if (event.type == SDL_CONTROLLERAXISMOTION)
    {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
            g_state.controllerAxisX = event.caxis.value;
        else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
            g_state.controllerAxisY = event.caxis.value;
    }
    else if (event.type == SDL_JOYAXISMOTION)
    {
        if (event.jaxis.axis == 0)
            g_state.controllerAxisX = event.jaxis.value;
        else if (event.jaxis.axis == 1)
            g_state.controllerAxisY = event.jaxis.value;
    }
    else if (event.type == SDL_JOYHATMOTION)
    {
        g_state.controllerButtons &= (u16)~TH_BUTTON_DIRECTION;
        if ((event.jhat.value & SDL_HAT_UP) != 0)
            g_state.controllerButtons |= TH_BUTTON_UP;
        if ((event.jhat.value & SDL_HAT_DOWN) != 0)
            g_state.controllerButtons |= TH_BUTTON_DOWN;
        if ((event.jhat.value & SDL_HAT_LEFT) != 0)
            g_state.controllerButtons |= TH_BUTTON_LEFT;
        if ((event.jhat.value & SDL_HAT_RIGHT) != 0)
            g_state.controllerButtons |= TH_BUTTON_RIGHT;
    }
    else if (event.type == SDL_CONTROLLERDEVICEREMOVED || event.type == SDL_JOYDEVICEREMOVED)
    {
        g_state.controllerButtons = 0;
        g_state.controllerAxisX = 0;
        g_state.controllerAxisY = 0;
        modern::LogStartup("input/controller: device removed; state cleared");
    }
    else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
        ResetTouchState();
}

u16 PollButtons()
{
    EnsureMobileConfig();
    EnsureControllerInput();
    SDL_Event event;
    while (SDL_PollEvent(&event))
        ProcessEvent(event);

    UpdateSecondaryGesture();

    const bool dialogueActive = IsDialogueActive();
    static bool dialogueWasActive;
    if (dialogueActive && !dialogueWasActive)
    {
        // A latched/drag-generated Z is a level input. Dialogue waits require
        // a fresh rising edge, so carrying the level into the first line makes
        // every later tap look as if Z never got released. Drop active touch
        // holds on entry and suspend automatic fire until the dialogue ends.
        ClearGameplayTouches();
        modern::LogStartup(g_shootLatched
                               ? "dialogue/input: automatic Z suspended; toggle remains armed"
                               : "dialogue/input: gameplay touches released");
    }
    else if (!dialogueActive && dialogueWasActive)
    {
        modern::LogStartup("dialogue/input: automatic controls restored");
    }
    dialogueWasActive = dialogueActive;

    if (g_backgroundPausePending && IsGameplayReady())
    {
        QueuePulse(TH_BUTTON_MENU);
        g_backgroundPausePending = false;
        modern::LogStartup("lifecycle: pause menu requested");
    }

    const bool stageClearActive = IsStageClearSequenceActive();
    static bool stageClearWasActive;
    static Uint32 stageClearStartedAt;
    static int stageClearStage = -1;
    if (stageClearActive && !stageClearWasActive)
    {
        stageClearStartedAt = SDL_GetTicks();
        stageClearStage = g_GameManager.currentStage;
        char message[128];
        SDL_snprintf(message, sizeof(message),
                     "stage-clear/input: stage=%d automatic Z released", stageClearStage);
        modern::LogStartup(message);
    }
    else if (!stageClearActive && stageClearWasActive)
    {
        char message[160];
        SDL_snprintf(message, sizeof(message),
                     "stage-clear/input: stage=%d ended after %u ms",
                     stageClearStage,
                     static_cast<unsigned int>(SDL_GetTicks() - stageClearStartedAt));
        modern::LogStartup(message);
        stageClearStage = -1;
    }
    stageClearWasActive = stageClearActive;

    const u16 smokeButtons = PollSmokeTestButtons();
    const bool overlayOpen = g_developerOpen || g_settingsOpen || g_layoutEdit;
    u16 result = overlayOpen
                     ? 0
                     : static_cast<u16>(g_state.buttons | g_state.keyboardButtons |
                                        g_state.controllerButtons | ControllerDirectionBits());
    if (!overlayOpen)
    {
        result |= smokeButtons;
        result |= g_pulseButtons;
        if (g_shootLatched && !stageClearActive && !dialogueActive)
            result |= TH_BUTTON_SHOOT;
        if (g_focusLatched) result |= TH_BUTTON_FOCUS;
        if (g_config.dragAutoShoot && !stageClearActive && !dialogueActive &&
            IsGameplayReady() && !IsGameplayMenuOpen() &&
            g_state.moveFinger >= 0 && g_state.moveIsDrag &&
            (g_config.controlMode == CONTROL_DRAG || !g_config.showButtons))
            result |= TH_BUTTON_SHOOT;

        // ReplayManager replaces g_CurFrameInput with the recorded stream
        // after this poll. Keep live touch/keyboard gameplay bits out of the
        // frame so a failed or delayed replay callback cannot leave the user
        // in control of the player. MENU remains available for pause/exit.
        if (g_GameManager.flags.isReplay && IsGameplayReady() && !IsGameplayMenuOpen())
        {
            result &= TH_BUTTON_MENU;
            static bool replayInputGateLogged;
            if (!replayInputGateLogged)
            {
                replayInputGateLogged = true;
                modern::LogStartup("replay/input: live gameplay input gated");
            }
        }

    }
    static bool stageClearInputGateVerified;
    const char *smokeTest = SDL_getenv("TH095_IOS_SMOKE_TEST");
    if (!stageClearInputGateVerified && stageClearActive && g_shootLatched &&
        smokeTest != NULL && strcmp(smokeTest, "stageclear-input") == 0)
    {
        stageClearInputGateVerified = true;
        modern::LogStartup((result & TH_BUTTON_SHOOT) == 0
                               ? "smoke-stage-clear-input: PASS - automatic Z suppressed"
                               : "smoke-stage-clear-input: FAILED - Z remained active");
    }
    g_pulseButtons = 0;
    return result;
}

void ResetTouchState()
{
    ClearTransientTouchState();
    g_state.keyboardButtons = 0;
    g_state.controllerButtons = 0;
    g_state.controllerAxisX = 0;
    g_state.controllerAxisY = 0;
    g_pulseButtons = 0;
    g_shootLatched = false;
    g_focusLatched = false;
    g_developerOpen = false;
    g_settingsOpen = false;
    g_performanceOpen = false;
    g_layoutEdit = false;
}

bool IsDeveloperInvincible()
{
    return g_developerInvincible;
}

bool IsAutoBombEnabled()
{
    EnsureMobileConfig();
    return g_config.autoBomb != 0;
}

bool ConsumeDragDelta(f32 *dx, f32 *dy)
{
    if (dx == NULL || dy == NULL)
        return false;
    *dx = g_state.dragDeltaX;
    *dy = g_state.dragDeltaY;
    g_state.dragDeltaX = 0.0f;
    g_state.dragDeltaY = 0.0f;
    return *dx != 0.0f || *dy != 0.0f;
}

void DrawVirtualControls()
{
    if (g_layout.drawableWidth <= 0 || g_layout.drawableHeight <= 0)
        return;

    GLint previousProgram = 0;
    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousTexture = 0;
    GLint previousArrayBuffer = 0;
    GLint previousElementBuffer = 0;
    GLint previousViewport[4] = {0, 0, 0, 0};
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean depthMask = GL_TRUE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &previousElementBuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);

    glViewport(0, 0, g_layout.drawableWidth, g_layout.drawableHeight);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);

    EnsureMobileConfig();

    if (g_developerOpen && !IsGameplayReady())
    {
        g_developerOpen = false;
        ClearGameplayTouches();
    }

    if (g_layoutEdit)
    {
        DrawLayoutEditor();
    }
    else if (g_settingsOpen)
    {
        DrawSettingsPanel();
    }
    else if (g_developerOpen)
    {
        DrawDeveloperPanel();
    }
    else if (IsGameplayReady())
    {
        u16 pressed = g_state.buttons | g_state.keyboardButtons |
                      g_state.controllerButtons | ControllerDirectionBits();
        if (g_shootLatched) pressed |= TH_BUTTON_SHOOT;
        if (g_focusLatched) pressed |= TH_BUTTON_FOCUS;
        if (g_config.controlMode != CONTROL_DRAG)
        {
            const ControlGeometry joystick = JoystickGeometry();
            const GLubyte outerAlpha = static_cast<GLubyte>(130.0f * g_config.opacity);
            const GLubyte innerAlpha = static_cast<GLubyte>(80.0f * g_config.opacity);
            DrawCircle(joystick.x, joystick.y, joystick.radius, 28, 34, 42, outerAlpha);
            DrawCircle(joystick.x, joystick.y, joystick.radius * 0.78f, 70, 78, 90, innerAlpha);
            const float maxOffset = joystick.radius * 0.42f;
            float dx = g_state.moveDx * static_cast<float>(g_layout.drawableWidth);
            float dy = g_state.moveDy * static_cast<float>(g_layout.drawableHeight);
            const float length = sqrtf(dx * dx + dy * dy);
            if (length > maxOffset && length > 0.0f)
            {
                dx = dx * maxOffset / length;
                dy = dy * maxOffset / length;
            }
            DrawCircle(joystick.x + dx, joystick.y + dy, joystick.radius * 0.39f,
                       230, 235, 242,
                       static_cast<GLubyte>((g_state.moveFinger >= 0 ? 210.0f : 145.0f) *
                                            g_config.opacity));
        }

        if (g_config.showButtons)
        {
            DrawActionButton(ShotGeometry(), (pressed & TH_BUTTON_SHOOT) != 0, 'Z');
            DrawActionButton(BombGeometry(), (pressed & TH_BUTTON_BOMB) != 0, 'X');
            DrawActionButton(FocusGeometry(), (pressed & TH_BUTTON_FOCUS) != 0, 'S');
            DrawActionButton(MenuGeometry(), (pressed & TH_BUTTON_MENU) != 0, 'M');
        }
        if (g_config.showDeveloperButton)
            DrawDeveloperButton();
    }
    else if (g_Supervisor.curState == SupervisorState_TitleScreen)
    {
        DrawSettingsButton();
        DrawPerformanceButton();
    }

    glDepthMask(depthMask);
    if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (scissorEnabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousArrayBuffer));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(previousElementBuffer));
    glUseProgram(static_cast<GLuint>(previousProgram));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
}

} // namespace ios
} // namespace modern
} // namespace th095
