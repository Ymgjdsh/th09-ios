#include "modern/windows_runtime.hpp"

#include <execinfo.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <windows.h>
#include <SDL.h>
#include "ios_touch.hpp"
#include "Gui.hpp"
#include "GameManager.hpp"
#include "Spellcard.hpp"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPTSTR, int);

namespace th095
{
#if defined(TH095_MODERN_PORT) && !defined(TH095_MATCH_EXACT) && !defined(DIFFBUILD)
// These globals are fixed-address data in the Windows image.  The portable
// runtime keeps them as normal process objects; startup code publishes the
// live sub-owners into the same fields before gameplay begins.
GameManager g_GameManager;
Gui g_Gui = {};

GameManager::GameManager()
{
    memset(this, 0, sizeof(*this));
}

i32 Gui::IsDialoguePresent()
{
    return this->impl != NULL && this->impl->message.msgFile != NULL;
}
#endif
// The reconstructed source exposes this counter as an external global.  The
// original Windows executable owns it in a fixed data segment; the portable
// iOS lane provides normal process storage instead.
#if defined(TH095_IOS)
i32 g_LastSpellCount = 0;
#endif

struct AnmVm;
struct Effect;

int __fastcall EffectRandomSplashInit(AnmVm *);
int __fastcall EffectRandomSplashUpdate(AnmVm *);
int __fastcall EffectRandomSplashBigInit(AnmVm *);
int __fastcall EffectOrbitInit(AnmVm *);
int __fastcall EffectOrbitUpdate(AnmVm *);

int __fastcall FUN_0040e040(AnmVm *);
int __fastcall FUN_0040e120(AnmVm *);
int __fastcall FUN_0040e200(AnmVm *);
int __fastcall FUN_0040e2d0(AnmVm *);
int __fastcall FUN_00410bb0(AnmVm *);
int __fastcall FUN_004114e0(AnmVm *);
int __fastcall FUN_00411720(AnmVm *);
int __fastcall FUN_00411a80(AnmVm *);
int __fastcall FUN_00413070(AnmVm *);

int __fastcall FUN_00426280(Effect *);
int __fastcall FUN_004264f0(Effect *);
int __fastcall FUN_00426720(Effect *);
int __fastcall FUN_00426990(Effect *);
int __fastcall FUN_00426b20(Effect *);
int __fastcall FUN_00426bb0(Effect *);
int __fastcall FUN_00426c40(Effect *);
int __fastcall FUN_00426c90(Effect *);
int __fastcall FUN_00426d70(Effect *);
int __fastcall FUN_00426e70(Effect *);
int __fastcall FUN_004270c0(Effect *);
int __fastcall FUN_004271a0(Effect *);
int __fastcall FUN_00427250(Effect *);
int __fastcall FUN_00427260(Effect *);
int __fastcall FUN_004272e0(Effect *);
int __fastcall FUN_00427970(Effect *);
int __fastcall FUN_00427990(Effect *);
int __fastcall FUN_004279d0(Effect *);
int __fastcall FUN_00427a60(Effect *);
int __fastcall FUN_00427ae0(Effect *);
int __fastcall FUN_00427b50(Effect *);

// This retail table entry points at an AnmVm member. On the 32-bit Linux ABI
// its code entry receives `this` as the first stack argument, matching the
// reconstructed effect callback invocation.
extern "C" int AnmVmUpdate0040eb50(AnmVm *) asm("_ZN4TH0955AnmVm12FUN_0040eb50Ev");

namespace modern
{
namespace
{
int g_argumentCount;
char **g_arguments;
volatile sig_atomic_t g_reportingCrash;
#ifdef TH095_IOS
char g_diagnosticPath[PATH_MAX];
#endif

#ifdef TH095_IOS
void WriteEarlyDiagnostic(const char *line)
{
    const char *homePath = getenv("HOME");
    if (homePath == NULL || homePath[0] == '\0')
        return;

    char documentsPath[PATH_MAX];
    snprintf(documentsPath, sizeof(documentsPath), "%s/Documents", homePath);
    mkdir(documentsPath, 0755);

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/TH095-startup.log", documentsPath);
    int file = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file >= 0)
    {
        const char *text = line != NULL ? line : "<null>";
        write(file, text, strlen(text));
        write(file, "\n", 1);
        fsync(file);
        close(file);
    }

    // Keep a second marker in the location shown by older file managers.
    char preferencesPath[PATH_MAX];
    snprintf(preferencesPath, sizeof(preferencesPath), "%s/Library/Preferences", homePath);
    mkdir(preferencesPath, 0755);
    snprintf(path, sizeof(path), "%s/TH095-startup.log", preferencesPath);
    file = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file >= 0)
    {
        const char *text = line != NULL ? line : "<null>";
        write(file, text, strlen(text));
        write(file, "\n", 1);
        fsync(file);
        close(file);
    }
}

// This runs before main and before SDL initialization, catching loader/static
// initialization failures that cannot be handled by the normal signal setup.
__attribute__((constructor)) static void EarlyStartupMarker()
{
    WriteEarlyDiagnostic("constructor: process loaded");
}
#endif

void WriteDiagnosticLine(const char *line)
{
#ifdef TH095_IOS
    const char *path = g_diagnosticPath[0] != '\0' ? g_diagnosticPath : "TH095-startup.log";
#else
    const char *path = "TH095-startup.log";
#endif
    FILE *file = fopen(path, "ab");
    if (file == NULL)
        return;
    fprintf(file, "%s\n", line != NULL ? line : "<null>");
    fclose(file);
}

void WriteCrashLine(int file, const char *line)
{
    if (line != NULL) write(file, line, strlen(line));
}

void ReportFatalSignal(int signalNumber, siginfo_t *signalInfo, void *)
{
    if (g_reportingCrash)
        _exit(128 + signalNumber);
    g_reportingCrash = 1;

#ifdef TH095_IOS
    char crashPath[PATH_MAX];
    if (g_diagnosticPath[0] != '\0')
        snprintf(crashPath, sizeof(crashPath), "%s.crash", g_diagnosticPath);
    else
        snprintf(crashPath, sizeof(crashPath), "modern-crash.txt");
    int file = open(crashPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
#else
    int file = open("modern-crash.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif
    if (file >= 0)
    {
        char line[160];
        snprintf(line, sizeof(line), "signal=%d fault-address=%p pid=%ld\n", signalNumber,
                 signalInfo != NULL ? signalInfo->si_addr : NULL, static_cast<long>(getpid()));
        WriteCrashLine(file, line);

        void *frames[64];
        int frameCount = backtrace(frames, sizeof(frames) / sizeof(frames[0]));
        backtrace_symbols_fd(frames, frameCount, file);
        fsync(file);
        close(file);
    }

    signal(signalNumber, SIG_DFL);
    raise(signalNumber);
    _exit(128 + signalNumber);
}

void InstallSignalHandler(int signalNumber)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = ReportFatalSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigaction(signalNumber, &action, NULL);
}

struct SpellPracticeMusic
{
    int32_t lastSpell;
    int32_t track;
    const char *path;
    int32_t visible;
    int32_t alternate;
};

struct ModernEffectTemplate
{
    int32_t scriptIdx;
    uintptr_t update;
    uintptr_t initialize;
};

uintptr_t CodeAddress(int (__fastcall *callback)(AnmVm *))
{
    return reinterpret_cast<uintptr_t>(callback);
}

uintptr_t CodeAddress(int (__fastcall *callback)(Effect *))
{
    return reinterpret_cast<uintptr_t>(callback);
}

void CopyTargetString(uintptr_t address, const char *value)
{
    memcpy(reinterpret_cast<void *>(address), value, strlen(value) + 1);
}

void InitializeTargetData()
{
#ifdef TH095_IOS
    g_LastSpellCount = 43;
    WriteDiagnosticLine("runtime-data: portable effect/music tables ready");
    return;
#else
    static const ModernEffectTemplate effectTemplates[66] = {
        {28, 0, 0}, {29, 0, 0}, {30, 0, 0},
        {31, CodeAddress(EffectRandomSplashUpdate), CodeAddress(EffectRandomSplashBigInit)},
        {36, CodeAddress(EffectRandomSplashUpdate), CodeAddress(EffectRandomSplashInit)},
        {37, CodeAddress(EffectRandomSplashUpdate), CodeAddress(EffectRandomSplashInit)},
        {38, CodeAddress(EffectRandomSplashUpdate), CodeAddress(EffectRandomSplashInit)},
        {39, CodeAddress(EffectRandomSplashUpdate), CodeAddress(EffectRandomSplashInit)},
        {40, CodeAddress(EffectRandomSplashUpdate), CodeAddress(EffectRandomSplashInit)},
        {41, CodeAddress(EffectRandomSplashUpdate), CodeAddress(EffectRandomSplashInit)},
        {42, CodeAddress(EffectRandomSplashUpdate), CodeAddress(EffectRandomSplashInit)},
        {43, CodeAddress(EffectRandomSplashUpdate), CodeAddress(EffectRandomSplashInit)},
        {44, 0, 0},
        {45, CodeAddress(EffectOrbitUpdate), CodeAddress(EffectOrbitInit)},
        {45, CodeAddress(EffectOrbitUpdate), CodeAddress(EffectOrbitInit)},
        {45, CodeAddress(EffectOrbitUpdate), CodeAddress(EffectOrbitInit)},
        {0, 0, 0},
        {32, CodeAddress(FUN_00426bb0), CodeAddress(FUN_00426b20)},
        {33, CodeAddress(FUN_00426c90), CodeAddress(FUN_00426b20)},
        {51, CodeAddress(FUN_00426d70), CodeAddress(FUN_00426e70)},
        {56, 0, 0},
        {52, CodeAddress(FUN_004271a0), CodeAddress(FUN_004270c0)},
        {54, CodeAddress(FUN_00426c40), 0},
        {104, CodeAddress(FUN_00427250), 0},
        {104, CodeAddress(FUN_00427250), 0},
        {35, 0, 0},
        {53, CodeAddress(FUN_004271a0), CodeAddress(FUN_004270c0)},
        {34, CodeAddress(FUN_00426bb0), CodeAddress(FUN_00426b20)},
        {57, 0, 0}, {58, 0, 0}, {59, 0, 0}, {60, 0, 0},
        {48, 0, 0}, {49, 0, 0}, {50, 0, 0},
        {88, CodeAddress(FUN_00427990), CodeAddress(FUN_004272e0)},
        {88, CodeAddress(FUN_004114e0), CodeAddress(FUN_00411720)},
        {92, CodeAddress(FUN_004114e0), CodeAddress(FUN_00411a80)},
        {71, 0, 0},
        {76, CodeAddress(FUN_00427990), CodeAddress(FUN_004272e0)},
        {81, CodeAddress(FUN_004279d0), CodeAddress(FUN_004272e0)},
        {82, CodeAddress(AnmVmUpdate0040eb50), CodeAddress(FUN_004272e0)},
        {83, CodeAddress(FUN_0040e040), CodeAddress(FUN_004272e0)},
        {83, CodeAddress(FUN_0040e120), CodeAddress(FUN_004272e0)},
        {83, CodeAddress(FUN_0040e200), CodeAddress(FUN_004272e0)},
        {83, CodeAddress(FUN_0040e2d0), CodeAddress(FUN_004272e0)},
        {84, CodeAddress(FUN_00410bb0), CodeAddress(FUN_004272e0)},
        {72, 0, 0},
        {85, CodeAddress(FUN_00413070), CodeAddress(FUN_004272e0)},
        {86, CodeAddress(FUN_00427990), CodeAddress(FUN_004272e0)},
        {80, CodeAddress(FUN_00427a60), CodeAddress(FUN_004272e0)},
        {73, CodeAddress(FUN_004264f0), CodeAddress(FUN_00426280)},
        {77, CodeAddress(FUN_00427990), CodeAddress(FUN_004272e0)},
        {88, CodeAddress(FUN_00427ae0), CodeAddress(FUN_004272e0)},
        {88, CodeAddress(FUN_00427ae0), CodeAddress(FUN_004272e0)},
        {87, CodeAddress(FUN_004279d0), CodeAddress(FUN_004272e0)},
        {96, CodeAddress(FUN_004279d0), CodeAddress(FUN_00427970)},
        {55, 0, 0},
        {100, CodeAddress(FUN_004279d0), CodeAddress(FUN_00427970)},
        {78, CodeAddress(FUN_00427990), CodeAddress(FUN_004272e0)},
        {102, 0, CodeAddress(FUN_00427260)},
        {103, 0, CodeAddress(FUN_00427260)},
        {75, 0, 0},
        {74, CodeAddress(FUN_00426990), CodeAddress(FUN_00426720)},
        {77, CodeAddress(FUN_00427b50), CodeAddress(FUN_004272e0)},
        {98, CodeAddress(FUN_004279d0), CodeAddress(FUN_00427970)},
    };
    static const int32_t stageScoreTables[9] = {
        1000000, 1500000, 2000000, 2500000, 2500000, 3000000, 4000000, 6000000, 6660000,
    };
    static const uint32_t messageTextColors[12][4] = {
        {0x00e8f0ff, 0x00f0e8ff, 0x00ffe8f0, 0x00ffe8f0},
        {0x00e8f0ff, 0x00f0e8ff, 0x00ffe8f0, 0x00ffe8f0},
        {0x00e8f0ff, 0x00f0e8ff, 0x00ffe8f0, 0x00ffe8f0},
        {0x00e8f0ff, 0x00f0e8ff, 0x00ffe8f0, 0x00ffe8f0},
        {0x00e8f0ff, 0x00f0e8ff, 0x00ffe8f0, 0x00ffe8f0},
        {0x00e8f0ff, 0x00f0e8ff, 0x00ffe8f0, 0x00ffe8f0},
        {0x00e8f0ff, 0x00f0e8ff, 0x00ffe8f0, 0x00ffe8f0},
        {0x00e8f0ff, 0x00f0e8ff, 0x00ffe8f0, 0x00ffe8f0},
        {0x00e8f0ff, 0x00f0e8ff, 0x00ffe8f0, 0x00ffe8f0},
        {0x00e8f0ff, 0x00f0e8ff, 0x00ffe8f0, 0x00ffe8f0},
        {0x00e8f0ff, 0x00f0e8ff, 0x00ffe8f0, 0x00ffe8f0},
        {0x00e8f0ff, 0x00f0e8ff, 0x00ffe8f0, 0x00ffe8f0},
    };
    static const int32_t stageMusicContexts[9][3] = {
        {1, 2, 0}, {3, 4, 0}, {5, 6, 0}, {7, 8, 0}, {7, 9, 0},
        {10, 11, 0}, {12, 13, 15}, {12, 14, 15}, {16, 17, 0},
    };
    static const SpellPracticeMusic spellPracticeMusic[] = {
        {1, 1, "TH095_00.mid", 0, 0}, {12, 2, "TH095_03.mid", 1, 0},
        {16, 3, "TH095_04.mid", 0, 0}, {31, 4, "TH095_05.mid", 1, 0},
        {35, 5, "TH095_06.mid", 0, 0}, {53, 6, "TH095_07.mid", 1, 0},
        {76, 8, "TH095_09.mid", 1, 0}, {99, 9, "TH095_10.mid", 1, 0},
        {118, 11, "TH095_12.mid", 1, 0}, {122, 12, "TH095_13.mid", 0, 0},
        {142, 13, "TH095_14.mid", 1, 0}, {146, 15, "TH095_13b.mid", 2, 1},
        {150, 12, "TH095_13.mid", 0, 0}, {170, 14, "TH095_15.mid", 1, 0},
        {190, 15, "TH095_13b.mid", 2, 1}, {193, 16, "TH095_18.mid", 0, 0},
        {204, 17, "TH095_19.mid", 1, 0}, {222, 20, "TH095_20.mid", 2, 0},
        {-1, 0, "", 0, 0},
    };

    *reinterpret_cast<int32_t *>(0x004c6c3c) = 43;
    memcpy(reinterpret_cast<void *>(0x004c6d30), effectTemplates, sizeof(effectTemplates));
    memcpy(reinterpret_cast<void *>(0x004c7158), stageScoreTables, sizeof(stageScoreTables));
    memcpy(reinterpret_cast<void *>(0x004c7180), messageTextColors, sizeof(messageTextColors));
    memcpy(reinterpret_cast<void *>(0x004c7240), stageMusicContexts, sizeof(stageMusicContexts));
    memcpy(reinterpret_cast<void *>(0x004c7670), spellPracticeMusic, sizeof(spellPracticeMusic));

    CopyTargetString(0x004b4ca0, "etama.anm");
    CopyTargetString(0x004b5820, "replay/th8_00.rpy");
    CopyTargetString(0x004b5834, "error: spell card initialization failed\n");
    CopyTargetString(0x004b5864, "error: 2D initialization failed\n");
    CopyTargetString(0x004b588c, "error: effect initialization failed\n");
    CopyTargetString(0x004b58b8, "error: enemy initialization failed\n");
    CopyTargetString(0x004b58dc, "error: bullet initialization failed\n");
    CopyTargetString(0x004b5904, "error: background initialization failed\n");
    CopyTargetString(0x004b5930, "error: player initialization failed\n");
#endif
}
}

bool ConfigureDataDirectory()
{
    const char *directory = NULL;
    for (int index = 1; index < g_argumentCount; ++index)
    {
        if (strcmp(g_arguments[index], "--data-dir") == 0)
        {
            if (++index >= g_argumentCount)
            {
                fprintf(stderr, "TH095-modern: --data-dir requires a directory path\n");
                return false;
            }
            directory = g_arguments[index];
        }
        else if (strncmp(g_arguments[index], "--data-dir=", 11) == 0)
        {
            directory = g_arguments[index] + 11;
        }
    }

#ifdef TH095_IOS
    // Keep diagnostics in Documents so they can be extracted through
    // iTunes/File Sharing or a device file manager when syslog is unavailable.
    const char *homePath = getenv("HOME");
    if (homePath != NULL && homePath[0] != '\0')
    {
        char documentsPath[PATH_MAX];
        snprintf(documentsPath, sizeof(documentsPath), "%s/Documents", homePath);
        mkdir(documentsPath, 0755);
        snprintf(g_diagnosticPath, sizeof(g_diagnosticPath), "%s/startup.log", documentsPath);
    }
    else
    {
        char *prefPath = SDL_GetPrefPath("com.TH095.sdl2.ios", "TH095");
        if (prefPath != NULL)
        {
            snprintf(g_diagnosticPath, sizeof(g_diagnosticPath), "%sstartup.log", prefPath);
            SDL_free(prefPath);
        }
    }
    WriteDiagnosticLine("configure: begin");
    WriteDiagnosticLine("build: TH095 iOS 0.1.1 (2), minimum iOS 14.0");
    if (directory != NULL && (directory[0] == '\0' || chdir(directory) != 0))
    {
        fprintf(stderr, "TH095-modern: unable to enter data directory: %s\n", directory);
        WriteDiagnosticLine("configure: explicit data directory failed");
        return false;
    }
    if (directory == NULL)
    {
        char *basePath = SDL_GetBasePath();
        if (basePath != NULL)
        {
            chdir(basePath);
            SDL_free(basePath);
        }
    }
    WriteDiagnosticLine("configure: bundle directory selected");
#else
    if (directory != NULL && (directory[0] == '\0' || chdir(directory) != 0))
    {
        fprintf(stderr, "TH095-modern: unable to enter data directory: %s\n", directory);
        return false;
    }
#endif

    struct stat info;
    if (stat("th095.dat", &info) != 0 || !S_ISREG(info.st_mode))
    {
        fprintf(stderr, "TH095-modern: selected directory does not contain th095.dat\n");
        WriteDiagnosticLine("configure: th095.dat missing");
        return false;
    }
    WriteDiagnosticLine("configure: data files found");
    unlink("modern-files.txt");
    unlink("modern-crash.txt");
    unlink("modern-render.txt");
    return true;
}

void InstallCrashReporter()
{
    InitializeTargetData();
#ifdef TH095_IOS
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
    WriteDiagnosticLine("crash-reporter: signal handlers disabled for sanitizer build");
    return;
#endif
#endif
    const char *disableCrashHandler = getenv("TH095_DISABLE_CRASH_HANDLER");
    if (disableCrashHandler != NULL && disableCrashHandler[0] != '\0' &&
        disableCrashHandler[0] != '0')
    {
        WriteDiagnosticLine("crash-reporter: signal handlers disabled for sanitizer");
        return;
    }
#endif
    InstallSignalHandler(SIGSEGV);
    InstallSignalHandler(SIGABRT);
    InstallSignalHandler(SIGFPE);
    InstallSignalHandler(SIGILL);
    InstallSignalHandler(SIGBUS);
}

void LogStartup(const char *phase)
{
    WriteDiagnosticLine(phase);
}

void LogArchiveRequest(const char *path)
{
#ifdef TH095_IOS
    char archivePath[PATH_MAX];
    if (g_diagnosticPath[0] != '\0')
        snprintf(archivePath, sizeof(archivePath), "%s-files", g_diagnosticPath);
    else
        snprintf(archivePath, sizeof(archivePath), "modern-files.txt");
    FILE *file = fopen(archivePath, "ab");
#else
    FILE *file = fopen("modern-files.txt", "ab");
#endif
    if (file == NULL)
        return;
    fprintf(file, "thread=%08lx path=%s\n", (unsigned long)GetCurrentThreadId(), path != NULL ? path : "<null>");
    fclose(file);
}

void SetArguments(int argc, char **argv)
{
    g_argumentCount = argc;
    g_arguments = argv;
}
} // namespace modern
} // namespace th095

// SDL maps this entry point to SDL_main on iOS. SDL2main owns the real process
// entry point and starts UIApplication before invoking the game.
int main(int argc, char **argv)
{
    th095::modern::SetArguments(argc, argv);
    return WinMain(NULL, NULL, NULL, 0);
}
