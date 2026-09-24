#include "ios_compat.hpp"
#ifdef TH095_IOS
#include "ios_touch.hpp"
#endif

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#ifdef TH095_IOS
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <mach-o/dyld.h>
#endif
#include <dinput.h>
#include <dsound.h>

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#ifndef TH095_IOS
#include <fontconfig/fontconfig.h>
#endif
#include <glob.h>
#include <limits.h>
#include <math.h>
#include <map>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

namespace
{
struct IosPointerRegistry
{
    IosPointerRegistry()
    {
        pthread_mutex_init(&this->mutex, NULL);
        this->pointers.push_back(0);
    }

    ~IosPointerRegistry()
    {
        pthread_mutex_destroy(&this->mutex);
    }

    pthread_mutex_t mutex;
    std::map<uintptr_t, DWORD> handles;
    std::vector<uintptr_t> pointers;
};

IosPointerRegistry &GetIosPointerRegistry()
{
    static IosPointerRegistry registry;
    return registry;
}
} // namespace

DWORD TH095IosStorePointer(uintptr_t pointerValue)
{
    if (pointerValue == 0)
        return 0;

    IosPointerRegistry &registry = GetIosPointerRegistry();
    pthread_mutex_lock(&registry.mutex);
    std::map<uintptr_t, DWORD>::const_iterator existing = registry.handles.find(pointerValue);
    if (existing != registry.handles.end())
    {
        const DWORD handle = existing->second;
        pthread_mutex_unlock(&registry.mutex);
        return handle;
    }

    const DWORD handle = static_cast<DWORD>(registry.pointers.size());
    registry.pointers.push_back(pointerValue);
    registry.handles[pointerValue] = handle;
    pthread_mutex_unlock(&registry.mutex);
    return handle;
}

uintptr_t TH095IosLoadPointer(DWORD handle)
{
    if (handle == 0)
        return 0;

    IosPointerRegistry &registry = GetIosPointerRegistry();
    pthread_mutex_lock(&registry.mutex);
    const uintptr_t pointerValue = handle < registry.pointers.size() ? registry.pointers[handle] : 0;
    pthread_mutex_unlock(&registry.mutex);
    return pointerValue;
}

void TH095IosForgetPointer(uintptr_t pointerValue)
{
    if (pointerValue == 0)
        return;

    IosPointerRegistry &registry = GetIosPointerRegistry();
    pthread_mutex_lock(&registry.mutex);
    std::map<uintptr_t, DWORD>::iterator existing = registry.handles.find(pointerValue);
    if (existing != registry.handles.end())
    {
        const DWORD handle = existing->second;
        if (handle < registry.pointers.size())
            registry.pointers[handle] = 0;
        registry.handles.erase(existing);
    }
    pthread_mutex_unlock(&registry.mutex);
}

namespace
{
enum HandleKind { HANDLE_FILE, HANDLE_THREAD, HANDLE_EVENT, HANDLE_MUTEX, HANDLE_FIND };

struct LinuxHandle
{
    explicit LinuxHandle(HandleKind value) : kind(value) {}
    virtual ~LinuxHandle() {}
    HandleKind kind;
};

struct FileHandle : LinuxHandle
{
    explicit FileHandle(int value) : LinuxHandle(HANDLE_FILE), fd(value) {}
    ~FileHandle() { if (fd >= 0) close(fd); }
    int fd;
};

struct ThreadHandle : LinuxHandle
{
    ThreadHandle() : LinuxHandle(HANDLE_THREAD), finished(false), joined(false), result(0), id(0) {}
    pthread_t thread;
    volatile bool finished;
    bool joined;
    DWORD result;
    DWORD id;
    LPTHREAD_START_ROUTINE start;
    LPVOID parameter;
};

struct EventHandle : LinuxHandle
{
    EventHandle(bool manualReset, bool initial)
        : LinuxHandle(HANDLE_EVENT), manual(manualReset), signaled(initial)
    {
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&condition, NULL);
    }
    ~EventHandle()
    {
        pthread_cond_destroy(&condition);
        pthread_mutex_destroy(&mutex);
    }
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool manual;
    bool signaled;
};

struct MutexHandle : LinuxHandle
{
    MutexHandle() : LinuxHandle(HANDLE_MUTEX) { pthread_mutex_init(&mutex, NULL); }
    ~MutexHandle() { pthread_mutex_destroy(&mutex); }
    pthread_mutex_t mutex;
};

struct FindHandle : LinuxHandle
{
    FindHandle() : LinuxHandle(HANDLE_FIND), index(0) {}
    std::vector<std::string> paths;
    size_t index;
};

std::map<uintptr_t, bool> g_liveCompatHandles;

void RegisterCompatHandle(LinuxHandle *handle)
{
    if (handle != NULL)
        g_liveCompatHandles[reinterpret_cast<uintptr_t>(handle)] = true;
}

bool IsLiveCompatHandle(HANDLE raw)
{
    return raw != NULL &&
           g_liveCompatHandles.find(reinterpret_cast<uintptr_t>(raw)) !=
               g_liveCompatHandles.end();
}

struct GdiObject
{
    enum Kind { BITMAP, FONT } kind;
    virtual ~GdiObject() {}
};

struct GdiBitmap : GdiObject
{
    GdiBitmap(int width_, int height_, int bits_)
        : width(width_), height(height_ < 0 ? -height_ : height_), bits(bits_)
    {
        kind = BITMAP;
        pitch = ((width * bits + 31) / 32) * 4;
        pixels.resize(pitch * height);
    }
    int width, height, bits, pitch;
    DWORD redMask = 0x7c00, greenMask = 0x03e0, blueMask = 0x001f;
    std::vector<BYTE> pixels;
};

struct GdiFont : GdiObject
{
    GdiFont() : font(NULL), utf8(false) { kind = FONT; }
    GdiFont(TTF_Font *font_, bool utf8_) : font(font_), utf8(utf8_) { kind = FONT; }
    // Font faces are owned by RasterFontCache; GDI handles only borrow them.
    ~GdiFont() {}
    TTF_Font *font;
    bool utf8;
};

struct GdiDc
{
    GdiDc() : bitmap(NULL), font(&stockFont), color(0xffffffff) {}
    GdiBitmap *bitmap;
    GdiFont stockFont;
    GdiFont *font;
    COLORREF color;
};

DWORD g_lastError;
WNDPROC g_windowProcedure;
SDL_Window *g_window;
std::map<DWORD, std::vector<MSG> > g_threadMessages;
// Win32 GDI treats handles as opaque values and makes duplicate cleanup a
// harmless failure.  Keep the same property for the portable C++ objects:
// reconstructed shutdown paths can release the text fonts after a partial
// initialization or more than once.
std::map<uintptr_t, bool> g_liveGdiObjects;
pthread_mutex_t g_messageMutex = PTHREAD_MUTEX_INITIALIZER;
__thread bool g_gdiTextUtf8 = false;
// Loading callbacks and the draw thread may both use SDL_ttf. Cached faces
// contain mutable glyph state; keep raster operations and cache edits atomic.
pthread_mutex_t g_textRasterMutex = PTHREAD_MUTEX_INITIALIZER;
struct TextRasterLock
{
    TextRasterLock() { pthread_mutex_lock(&g_textRasterMutex); }
    ~TextRasterLock() { pthread_mutex_unlock(&g_textRasterMutex); }
};

std::string ExecutableSiblingPath(const char *filename)
{
    char path[PATH_MAX + 1];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count <= 0 || count > PATH_MAX)
        return std::string();
    path[count] = '\0';
    char *separator = strrchr(path, '/');
    if (separator == NULL)
        return std::string();
    separator[1] = '\0';
    return std::string(path) + filename;
}

void SetApplicationIcon(SDL_Window *window)
{
    if (window == NULL)
        return;
    const std::string iconPath = ExecutableSiblingPath("TH095-modern.png");
    if (iconPath.empty())
        return;
    SDL_Surface *icon = IMG_Load(iconPath.c_str());
    if (icon == NULL)
        return;
    SDL_SetWindowIcon(window, icon);
    SDL_FreeSurface(icon);
}

DWORD CurrentThreadIdImpl()
{
    return static_cast<DWORD>(reinterpret_cast<uintptr_t>(pthread_self()));
}

std::string ConvertCp932ToUtf8(const char *text, size_t length)
{
    if (text == NULL || length == 0) return std::string();
#ifdef TH095_IOS
    // SDL's built-in iconv on iOS only guarantees Unicode encodings. The
    // original game data is CP932, so asking SDL_iconv for CP932 can silently
    // return the source bytes and SDL_ttf then renders Japanese as boxes.
    CFStringRef decoded = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(text),
        static_cast<CFIndex>(length), kCFStringEncodingDOSJapanese, false);
    if (decoded == NULL)
    {
        decoded = CFStringCreateWithBytes(
            kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(text),
            static_cast<CFIndex>(length), kCFStringEncodingShiftJIS, false);
    }
    if (decoded != NULL)
    {
        const CFIndex utf8Size =
            CFStringGetMaximumSizeForEncoding(CFStringGetLength(decoded), kCFStringEncodingUTF8) + 1;
        std::vector<char> output(static_cast<size_t>(utf8Size), 0);
        const Boolean converted = CFStringGetCString(
            decoded, &output[0], utf8Size, kCFStringEncodingUTF8);
        CFRelease(decoded);
        if (converted)
            return std::string(&output[0]);
    }
#endif
    SDL_iconv_t converter = SDL_iconv_open("UTF-8", "CP932");
    if (converter == reinterpret_cast<SDL_iconv_t>(-1)) return std::string(text, length);

    std::vector<char> output(length * 4 + 8, 0);
    const char *input = text;
    char *destination = &output[0];
    size_t inputLeft = length;
    size_t outputLeft = output.size() - 1;
    if (SDL_iconv(converter, &input, &inputLeft, &destination, &outputLeft) == static_cast<size_t>(-1))
    {
        SDL_iconv_close(converter);
        return std::string(text, length);
    }
    SDL_iconv_close(converter);
    return std::string(&output[0], destination - &output[0]);
}

const char *ResolveJapaneseFont(bool localizedUtf8)
{
    static std::string paths[2];
    static bool resolved[2] = {false, false};
#ifdef TH095_IOS
    const int fontIndex = localizedUtf8 ? 1 : 0;
#else
    const int fontIndex = 0;
#endif
    std::string &path = paths[fontIndex];
    if (resolved[fontIndex]) return path.empty() ? NULL : path.c_str();
    resolved[fontIndex] = true;
    path.clear();

    const char *overridePath = getenv(fontIndex == 0 ? "TH095_FONT" : "TH095_FONT_CJK");
    if (overridePath != NULL && access(overridePath, R_OK) == 0)
    {
        path = overridePath;
        return path.c_str();
    }

#ifdef TH095_IOS
    const char *basePath = SDL_GetBasePath();
    if (basePath != NULL)
    {
        const char *localizedNames[] = {"NotoSansSC-VF.ttf", "NotoSansSC-Regular.ttf", "PingFang.ttc"};
        const char *japaneseNames[] = {"msgothic.ttc"};
        const char *const *names = fontIndex == 0 ? japaneseNames : localizedNames;
        const size_t nameCount = fontIndex == 0 ? 1 : 3;
        for (size_t index = 0; index < nameCount; ++index)
        {
            path = std::string(basePath) + names[index];
            if (access(path.c_str(), R_OK) == 0)
            {
                th095::modern::LogStartup(fontIndex == 0
                                             ? "font: bundled Japanese font ready"
                                             : "font: bundled CJK font ready");
                return path.c_str();
            }
        }
    }
    // Ask CoreText for an installed Japanese face instead of relying on a
    // Windows font being bundled or an OS-specific font-directory spelling.
    if (fontIndex == 0)
    {
        CTFontRef systemFont = CTFontCreateWithName(CFSTR("HiraginoSans-W3"), 16, NULL);
        if (systemFont != NULL)
        {
            CFTypeRef location = CTFontCopyAttribute(systemFont, kCTFontURLAttribute);
            UInt8 systemPath[PATH_MAX];
            if (location != NULL && CFGetTypeID(location) == CFURLGetTypeID() &&
                CFURLGetFileSystemRepresentation(static_cast<CFURLRef>(location), true,
                                                systemPath, sizeof(systemPath)) &&
                access(reinterpret_cast<const char *>(systemPath), R_OK) == 0)
                path = reinterpret_cast<const char *>(systemPath);
            else
                path.clear();
            if (location != NULL) CFRelease(location);
            CFRelease(systemFont);
            if (!path.empty())
            {
                th095::modern::LogStartup("font: system Japanese font ready");
                return path.c_str();
            }
        }
    }
    // A stock iOS installation exposes PingFang as a readable system font;
    // use it when a private CJK asset was not bundled with the build.
    if (fontIndex != 0)
    {
        const char *systemNames[] = {"/System/Library/Fonts/PingFang.ttc",
                                     "/System/Library/Fonts/CoreUI/SFUI.ttf"};
        for (size_t index = 0; index < sizeof(systemNames) / sizeof(systemNames[0]); ++index)
        {
            if (access(systemNames[index], R_OK) == 0)
            {
                path = systemNames[index];
                th095::modern::LogStartup("font: system CJK fallback ready");
                return path.c_str();
            }
        }
    }
    th095::modern::LogStartup(fontIndex == 0
                                 ? "font: ERROR bundled msgothic.ttc is missing"
                                 : "font: ERROR no CJK font available; localized glyphs may be missing");
    return NULL;
#else
    if (!FcInit()) return NULL;
    FcPattern *pattern = FcPatternCreate();
    if (pattern == NULL) return NULL;
    FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8 *>("VL Gothic"));
    FcPatternAddString(pattern, FC_LANG, reinterpret_cast<const FcChar8 *>("ja"));
    FcPatternAddInteger(pattern, FC_SPACING, FC_MONO);
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result = FcResultNoMatch;
    FcPattern *match = FcFontMatch(NULL, pattern, &result);
    FcPatternDestroy(pattern);
    if (match == NULL) return NULL;
    FcChar8 *file = NULL;
    if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file != NULL)
        path = reinterpret_cast<const char *>(file);
    FcPatternDestroy(match);
    return path.empty() ? NULL : path.c_str();
#endif
}

void PutGdiTextPixel(GdiBitmap *bitmap, int x, int y, COLORREF color, BYTE coverage)
{
    if (bitmap == NULL || x < 0 || y < 0 || x >= bitmap->width || y >= bitmap->height || coverage == 0) return;
    const int red = color & 0xff;
    const int green = (color >> 8) & 0xff;
    const int blue = (color >> 16) & 0xff;
    BYTE *pixel = &bitmap->pixels[y * bitmap->pitch + x * bitmap->bits / 8];
    if (bitmap->bits == 16)
    {
        uint16_t packed;
        memcpy(&packed, pixel, sizeof(packed));
        // BI_BITFIELDS defines the channel widths: TH095 requests ARGB4444,
        // while other callers may request RGB555 or RGB565.
        const auto blend = [packed, coverage](DWORD mask, int value) -> DWORD {
            if (!mask) return 0;
            unsigned shift = 0;
            while (((mask >> shift) & 1) == 0) ++shift;
            const DWORD maximum = mask >> shift;
            const int previous = ((packed & mask) >> shift) * 255 / maximum;
            const int mixed = (previous * (255 - coverage) + value * coverage) / 255;
            return ((mixed * maximum / 255) << shift) & mask;
        };
        packed = static_cast<uint16_t>(blend(bitmap->redMask, red) |
            blend(bitmap->greenMask, green) | blend(bitmap->blueMask, blue));
        memcpy(pixel, &packed, sizeof(packed));
    }
    else if (bitmap->bits == 32)
    {
        pixel[0] = static_cast<BYTE>((pixel[0] * (255 - coverage) + blue * coverage) / 255);
        pixel[1] = static_cast<BYTE>((pixel[1] * (255 - coverage) + green * coverage) / 255);
        pixel[2] = static_cast<BYTE>((pixel[2] * (255 - coverage) + red * coverage) / 255);
        // GDI writes zero to the reserved high byte. TH095 seeds that byte
        // with 255 and inverts it after rasterization to obtain glyph alpha.
        pixel[3] = static_cast<BYTE>(pixel[3] * (255 - coverage) / 255);
    }
}

void *ThreadTrampoline(void *opaque)
{
    ThreadHandle *handle = static_cast<ThreadHandle *>(opaque);
    handle->id = CurrentThreadIdImpl();
    handle->result = handle->start(handle->parameter);
    handle->finished = true;
    return NULL;
}

bool FillFindData(FindHandle *handle, WIN32_FIND_DATAA *data)
{
    if (handle->index >= handle->paths.size())
        return false;
    memset(data, 0, sizeof(*data));
    const std::string &path = handle->paths[handle->index++];
    const char *name = strrchr(path.c_str(), '/');
    strncpy(data->cFileName, name != NULL ? name + 1 : path.c_str(), MAX_PATH - 1);
    struct stat info;
    if (stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode))
        data->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
    return true;
}

void PumpSdlEvents(MSG *message, bool *hasMessage)
{
    *hasMessage = false;
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
        return;
    SDL_Event event;
    if (!SDL_PollEvent(&event))
        return;
#ifdef TH095_IOS
    th095::modern::ios::ProcessEvent(event);
    // ProcessEvent converts iOS SDL_QUIT into a one-frame menu pulse.  Do not
    // translate it to WM_CLOSE, which would set receivedCloseMsg and exit to
    // the iOS desktop instead of opening the in-game menu.
    if (event.type == SDL_QUIT)
        return;
#endif
    memset(message, 0, sizeof(*message));
    message->hwnd = reinterpret_cast<HWND>(g_window);
    if (event.type == SDL_QUIT)
        message->message = WM_CLOSE;
    else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
    {
        message->message = WM_ACTIVATEAPP;
        message->wParam = TRUE;
    }
    else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
    {
        message->message = WM_ACTIVATEAPP;
        message->wParam = FALSE;
    }
    else
        message->message = 0;
    *hasMessage = true;
}

void FillKeyboard(BYTE *state, bool directInput)
{
    memset(state, 0, 256);
    SDL_PumpEvents();
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
#define MAP_KEY(win, sdl) state[(win)] = keys[(sdl)] ? 0x80 : 0
    if (directInput)
    {
        MAP_KEY(DIK_UP, SDL_SCANCODE_UP); MAP_KEY(DIK_DOWN, SDL_SCANCODE_DOWN);
        MAP_KEY(DIK_LEFT, SDL_SCANCODE_LEFT); MAP_KEY(DIK_RIGHT, SDL_SCANCODE_RIGHT);
        MAP_KEY(DIK_NUMPAD1, SDL_SCANCODE_KP_1); MAP_KEY(DIK_NUMPAD2, SDL_SCANCODE_KP_2);
        MAP_KEY(DIK_NUMPAD3, SDL_SCANCODE_KP_3); MAP_KEY(DIK_NUMPAD4, SDL_SCANCODE_KP_4);
        MAP_KEY(DIK_NUMPAD6, SDL_SCANCODE_KP_6); MAP_KEY(DIK_NUMPAD7, SDL_SCANCODE_KP_7);
        MAP_KEY(DIK_NUMPAD8, SDL_SCANCODE_KP_8); MAP_KEY(DIK_NUMPAD9, SDL_SCANCODE_KP_9);
        MAP_KEY(DIK_HOME, SDL_SCANCODE_HOME); MAP_KEY(DIK_P, SDL_SCANCODE_P);
        MAP_KEY(DIK_D, SDL_SCANCODE_D); MAP_KEY(DIK_Z, SDL_SCANCODE_Z); MAP_KEY(DIK_X, SDL_SCANCODE_X);
        MAP_KEY(DIK_LSHIFT, SDL_SCANCODE_LSHIFT); MAP_KEY(DIK_RSHIFT, SDL_SCANCODE_RSHIFT);
        MAP_KEY(DIK_ESCAPE, SDL_SCANCODE_ESCAPE); MAP_KEY(DIK_LCONTROL, SDL_SCANCODE_LCTRL);
        MAP_KEY(DIK_RCONTROL, SDL_SCANCODE_RCTRL); MAP_KEY(DIK_Q, SDL_SCANCODE_Q);
        MAP_KEY(DIK_S, SDL_SCANCODE_S); MAP_KEY(DIK_R, SDL_SCANCODE_R); MAP_KEY(DIK_RETURN, SDL_SCANCODE_RETURN);
    }
    else
    {
        MAP_KEY(VK_UP, SDL_SCANCODE_UP); MAP_KEY(VK_DOWN, SDL_SCANCODE_DOWN);
        MAP_KEY(VK_LEFT, SDL_SCANCODE_LEFT); MAP_KEY(VK_RIGHT, SDL_SCANCODE_RIGHT);
        MAP_KEY(VK_NUMPAD1, SDL_SCANCODE_KP_1); MAP_KEY(VK_NUMPAD2, SDL_SCANCODE_KP_2);
        MAP_KEY(VK_NUMPAD3, SDL_SCANCODE_KP_3); MAP_KEY(VK_NUMPAD4, SDL_SCANCODE_KP_4);
        MAP_KEY(VK_NUMPAD6, SDL_SCANCODE_KP_6); MAP_KEY(VK_NUMPAD7, SDL_SCANCODE_KP_7);
        MAP_KEY(VK_NUMPAD8, SDL_SCANCODE_KP_8); MAP_KEY(VK_NUMPAD9, SDL_SCANCODE_KP_9);
        MAP_KEY(VK_HOME, SDL_SCANCODE_HOME); MAP_KEY('P', SDL_SCANCODE_P); MAP_KEY('D', SDL_SCANCODE_D);
        MAP_KEY('Z', SDL_SCANCODE_Z); MAP_KEY('X', SDL_SCANCODE_X); MAP_KEY(VK_SHIFT, SDL_SCANCODE_LSHIFT);
        MAP_KEY(VK_ESCAPE, SDL_SCANCODE_ESCAPE); MAP_KEY(VK_CONTROL, SDL_SCANCODE_LCTRL);
        MAP_KEY('Q', SDL_SCANCODE_Q); MAP_KEY('S', SDL_SCANCODE_S); MAP_KEY('R', SDL_SCANCODE_R);
        MAP_KEY(VK_RETURN, SDL_SCANCODE_RETURN);
    }
#undef MAP_KEY
#ifdef TH095_IOS
    const u16 touch = th095::modern::ios::PollButtons();
#define MAP_TOUCH(button, dik, vk) if (touch & th095::button) state[directInput ? (dik) : (vk)] = 0x80
    MAP_TOUCH(TH_BUTTON_SHOOT, DIK_Z, 'Z');
    MAP_TOUCH(TH_BUTTON_BOMB, DIK_X, 'X');
    MAP_TOUCH(TH_BUTTON_FOCUS, DIK_LSHIFT, VK_SHIFT);
    MAP_TOUCH(TH_BUTTON_MENU, DIK_ESCAPE, VK_ESCAPE);
    MAP_TOUCH(TH_BUTTON_ENTER, DIK_RETURN, VK_RETURN);
    MAP_TOUCH(TH_BUTTON_UP, DIK_UP, VK_UP);
    MAP_TOUCH(TH_BUTTON_DOWN, DIK_DOWN, VK_DOWN);
    MAP_TOUCH(TH_BUTTON_LEFT, DIK_LEFT, VK_LEFT);
    MAP_TOUCH(TH_BUTTON_RIGHT, DIK_RIGHT, VK_RIGHT);
    MAP_TOUCH(TH_BUTTON_S, DIK_S, 'S');
#undef MAP_TOUCH
#endif
}

bool IsAbsolutePath(const char *path)
{
    return path != NULL && path[0] == '/';
}

std::string IosUserDataPath(const char *path)
{
    if (path == NULL || IsAbsolutePath(path))
        return path != NULL ? std::string(path) : std::string();

    const char *homePath = getenv("HOME");
    if (homePath == NULL || homePath[0] == '\0')
        return std::string(path);

    while (path[0] == '.' && (path[1] == '/' || path[1] == '\\'))
        path += 2;

    std::string result(homePath);
    result += "/Documents/";
    for (; *path != '\0'; ++path)
        result += *path == '\\' ? '/' : *path;
    return result;
}

void EnsureParentDirectories(const std::string &path)
{
    // Every redirected path is rooted below the app's Documents directory.
    // Create intermediate directories so replay and snapshot writes work on a
    // clean install without requiring a separate Win32 mkdir compatibility API.
    for (std::string::size_type slash = path.find('/', 1);
         slash != std::string::npos;
         slash = path.find('/', slash + 1))
    {
        const std::string directory = path.substr(0, slash);
        if (!directory.empty())
            mkdir(directory.c_str(), 0755);
    }
}

int OpenIosFile(const char *path, int flags, mode_t mode, bool writing)
{
    if (path == NULL || path[0] == '\0')
    {
        errno = EINVAL;
        return -1;
    }

    if (writing && !IsAbsolutePath(path))
    {
        const std::string userPath = IosUserDataPath(path);
        EnsureParentDirectories(userPath);
        if (strcmp(path, "TH095.cfg") == 0)
            th095::modern::LogStartup("ios-files: config write redirected to Documents");
        return open(userPath.c_str(), flags, mode);
    }

    // Writable game state is kept in Documents.  Prefer that copy when it
    // already exists so a stale score.dat accidentally bundled with the app
    // cannot mask a player's unlock progress.  Other resources continue to
    // use the bundle-first lookup below.
    if (!writing && !IsAbsolutePath(path) &&
        (strcmp(path, "score.dat") == 0 || strcmp(path, "TH095.cfg") == 0 ||
         strcmp(path, "mobile.cfg") == 0))
    {
        const std::string userPath = IosUserDataPath(path);
        const int userFd = open(userPath.c_str(), flags, mode);
        if (userFd >= 0)
        {
            if (strcmp(path, "score.dat") == 0)
                th095::modern::LogStartup("ios-files: score.dat read from Documents");
            return userFd;
        }
    }

    int fd = open(path, flags, mode);
    if (fd >= 0 || writing || IsAbsolutePath(path))
        return fd;

    const std::string userPath = IosUserDataPath(path);
    fd = open(userPath.c_str(), flags, mode);
    if (fd >= 0 && strcmp(path, "TH095.cfg") == 0)
        th095::modern::LogStartup("ios-files: config read from Documents");
    return fd;
}
} // namespace

extern "C" SDL_Window *TH095_linux_get_window() { return g_window; }

BOOL TH095IosSetGdiTextUtf8(BOOL enabled)
{
    const BOOL previous = g_gdiTextUtf8 ? TRUE : FALSE;
    g_gdiTextUtf8 = enabled != FALSE;
    return previous;
}

BOOL TH095IosGetGdiTextUtf8()
{
    return g_gdiTextUtf8 ? TRUE : FALSE;
}

const char *TH095IosSystemFontPath()
{
    const TextRasterLock lock;
    return ResolveJapaneseFont(false);
}

namespace
{
void BlendRasterPixel(BYTE *pixel, BYTE red, BYTE green, BYTE blue, BYTE alpha)
{
    if (pixel == NULL || alpha == 0) return;
    const unsigned int sourceAlpha = alpha;
    const unsigned int destinationAlpha = pixel[3];
    const unsigned int outputAlpha = sourceAlpha +
        (destinationAlpha * (255u - sourceAlpha) + 127u) / 255u;
    if (outputAlpha == 0) return;
    pixel[0] = static_cast<BYTE>((red * sourceAlpha +
                                  pixel[0] * destinationAlpha * (255u - sourceAlpha) / 255u +
                                  outputAlpha / 2u) / outputAlpha);
    pixel[1] = static_cast<BYTE>((green * sourceAlpha +
                                  pixel[1] * destinationAlpha * (255u - sourceAlpha) / 255u +
                                  outputAlpha / 2u) / outputAlpha);
    pixel[2] = static_cast<BYTE>((blue * sourceAlpha +
                                  pixel[2] * destinationAlpha * (255u - sourceAlpha) / 255u +
                                  outputAlpha / 2u) / outputAlpha);
    pixel[3] = static_cast<BYTE>(outputAlpha);
}

void DrawRasterGlyph(SDL_Surface *glyph, int x, int y, COLORREF color,
                     BYTE *destination, int destinationPitch, int width, int height)
{
    if (glyph == NULL || destination == NULL) return;
    const BYTE red = static_cast<BYTE>(color & 0xff);
    const BYTE green = static_cast<BYTE>((color >> 8) & 0xff);
    const BYTE blue = static_cast<BYTE>((color >> 16) & 0xff);
    const BYTE colorAlpha = static_cast<BYTE>((color >> 24) & 0xff);
    const BYTE effectiveAlpha = colorAlpha == 0 ? 255 : colorAlpha;
    if (SDL_MUSTLOCK(glyph)) SDL_LockSurface(glyph);
    for (int row = 0; row < glyph->h; ++row)
    {
        const int destinationY = y + row;
        if (destinationY < 0 || destinationY >= height) continue;
        const BYTE *source = static_cast<const BYTE *>(glyph->pixels) + row * glyph->pitch;
        for (int column = 0; column < glyph->w; ++column)
        {
            const int destinationX = x + column;
            if (destinationX < 0 || destinationX >= width) continue;
            const BYTE coverage = source[column * 4 + 3];
            const BYTE alpha = static_cast<BYTE>((coverage * effectiveAlpha + 127u) / 255u);
            BlendRasterPixel(destination + destinationY * destinationPitch + destinationX * 4,
                             red, green, blue, alpha);
        }
    }
    if (SDL_MUSTLOCK(glyph)) SDL_UnlockSurface(glyph);
}

struct RasterFontCache
{
    ~RasterFontCache()
    {
        if (!TTF_WasInit()) return;
        for (std::map<std::pair<std::string, int>, TTF_Font *>::iterator it = fonts.begin();
             it != fonts.end(); ++it)
            TTF_CloseFont(it->second);
    }

    std::map<std::pair<std::string, int>, TTF_Font *> fonts;
};

TTF_Font *GetCachedRasterFont(const char *path, int pixelSize, int style = TTF_STYLE_NORMAL)
{
    static RasterFontCache cache;
    const std::pair<std::string, int> key(path != NULL ? path : "", pixelSize * 16 + style);
    std::map<std::pair<std::string, int>, TTF_Font *>::iterator existing = cache.fonts.find(key);
    if (existing != cache.fonts.end())
        return existing->second;

    TTF_Font *font = TTF_OpenFont(path, pixelSize);
    if (font == NULL)
        return NULL;
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    TTF_SetFontStyle(font, style);
    cache.fonts[key] = font;
    char diagnostic[128];
    SDL_snprintf(diagnostic, sizeof(diagnostic), "text/font-cache: opened size=%d style=%d faces=%lu",
                 pixelSize, style, static_cast<unsigned long>(cache.fonts.size()));
    th095::modern::LogStartup(diagnostic);
    return font;
}

struct GdiGlyphCache
{
    GdiGlyphCache() : font(NULL), glyph(NULL) {}
    ~GdiGlyphCache() { if (glyph != NULL) SDL_FreeSurface(glyph); }
    TTF_Font *font;
    std::string text;
    SDL_Surface *glyph;
};

SDL_Surface *GetGdiGlyph(TTF_Font *font, const std::string &text)
{
    // TextHelper draws the same string five times for its outline. Rasterize
    // once, then reuse coverage for the different positions and colors.
    static GdiGlyphCache cache;
    if (cache.font == font && cache.text == text && cache.glyph != NULL)
        return cache.glyph;
    if (cache.glyph != NULL) SDL_FreeSurface(cache.glyph);
    cache.glyph = NULL;
    cache.font = font;
    cache.text = text;
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *rendered = TTF_RenderUTF8_Blended(font, text.c_str(), white);
    if (rendered != NULL)
    {
        cache.glyph = SDL_ConvertSurfaceFormat(rendered, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(rendered);
    }
    return cache.glyph;
}
} // namespace

BOOL TH095IosRasterizeTextUtf8(const char *text, int pixelSize, int width, int height,
                              COLORREF textColor, COLORREF shadowColor,
                              BYTE *destination, int destinationPitch)
{
    const TextRasterLock lock;
    if (text == NULL || text[0] == '\0' || pixelSize <= 0 || width <= 0 || height <= 0 ||
        destination == NULL || destinationPitch < width * 4)
        return FALSE;
    const char *fontPath = ResolveJapaneseFont(true);
    if (fontPath == NULL)
    {
        th095::modern::LogStartup("text/raster: no UTF-8 font available");
        return FALSE;
    }
    if (!TTF_WasInit() && TTF_Init() != 0)
    {
        th095::modern::LogStartup("text/raster: SDL_ttf initialization failed");
        return FALSE;
    }
    TTF_Font *font = GetCachedRasterFont(fontPath, pixelSize);
    if (font == NULL)
    {
        char message[256];
        SDL_snprintf(message, sizeof(message), "text/raster: font open failed: %s", TTF_GetError());
        th095::modern::LogStartup(message);
        return FALSE;
    }
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *rendered = TTF_RenderUTF8_Blended(font, text, white);
    if (rendered == NULL)
    {
        th095::modern::LogStartup("text/raster: UTF-8 glyph rasterization failed");
        return FALSE;
    }
    SDL_Surface *glyph = SDL_ConvertSurfaceFormat(rendered, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(rendered);
    if (glyph == NULL)
    {
        th095::modern::LogStartup("text/raster: RGBA conversion failed");
        return FALSE;
    }

    // The original renderer uses a four-sided outline. Render the shadow at
    // the same scaled offsets, then place the glyph at the logical origin.
    const int outline = pixelSize >= 24 ? 2 : 1;
    const int x = 4;
    const int y = 2;
    DrawRasterGlyph(glyph, x - outline, y, shadowColor, destination, destinationPitch, width, height);
    DrawRasterGlyph(glyph, x + outline, y, shadowColor, destination, destinationPitch, width, height);
    DrawRasterGlyph(glyph, x, y - outline, shadowColor, destination, destinationPitch, width, height);
    DrawRasterGlyph(glyph, x, y + outline, shadowColor, destination, destinationPitch, width, height);
    DrawRasterGlyph(glyph, x, y, textColor, destination, destinationPitch, width, height);
    static int rasterDiagnostics;
    if (SDL_getenv("TH095_IOS_TEXT_DIAGNOSTICS") != NULL && rasterDiagnostics < 12)
    {
        char message[192];
        SDL_snprintf(message, sizeof(message), "text/raster: success font=%d glyph=%dx%d text=%s",
                     pixelSize, glyph->w, glyph->h, text);
        th095::modern::LogStartup(message);
        ++rasterDiagnostics;
    }
    SDL_FreeSurface(glyph);
    return TRUE;
}

extern "C" {
HANDLE CreateFileA(LPCSTR path, DWORD access, DWORD, LPVOID, DWORD disposition, DWORD, HANDLE)
{
    int flags = (access & (GENERIC_WRITE | FILE_APPEND_DATA)) ? O_WRONLY : O_RDONLY;
    if ((access & GENERIC_READ) && (access & GENERIC_WRITE)) flags = O_RDWR;
    if (access & FILE_APPEND_DATA) flags |= O_APPEND;
    if (disposition == CREATE_ALWAYS) flags |= O_CREAT | O_TRUNC;
    if (disposition == OPEN_ALWAYS) flags |= O_CREAT;
    const bool writing = (access & (GENERIC_WRITE | FILE_APPEND_DATA)) != 0 ||
                         disposition == CREATE_ALWAYS || disposition == OPEN_ALWAYS;
    int fd = OpenIosFile(path, flags, 0666, writing);
    if (fd < 0) { g_lastError = errno; return INVALID_HANDLE_VALUE; }
    FileHandle *handle = new FileHandle(fd);
    RegisterCompatHandle(handle);
    return handle;
}

HANDLE CreateFileW(LPCWSTR path, DWORD access, DWORD share, LPVOID security, DWORD disposition, DWORD attrs, HANDLE templ)
{
    char converted[PATH_MAX];
    if (wcstombs(converted, path, sizeof(converted) - 1) == static_cast<size_t>(-1))
        return INVALID_HANDLE_VALUE;
    converted[sizeof(converted) - 1] = 0;
    return CreateFileA(converted, access, share, security, disposition, attrs, templ);
}

BOOL ReadFile(HANDLE raw, LPVOID data, DWORD size, LPDWORD readSize, LPVOID)
{
    if (raw == INVALID_HANDLE_VALUE || raw == NULL) return FALSE;
    ssize_t result = read(static_cast<FileHandle *>(raw)->fd, data, size);
    if (readSize != NULL) *readSize = result < 0 ? 0 : static_cast<DWORD>(result);
    return result >= 0;
}

BOOL WriteFile(HANDLE raw, LPCVOID data, DWORD size, LPDWORD written, LPVOID)
{
    if (raw == INVALID_HANDLE_VALUE || raw == NULL) return FALSE;
    ssize_t result = write(static_cast<FileHandle *>(raw)->fd, data, size);
    if (written != NULL) *written = result < 0 ? 0 : static_cast<DWORD>(result);
    return result >= 0;
}

DWORD SetFilePointer(HANDLE raw, LONG offset, LONG *, DWORD origin)
{
    int whence = origin == FILE_BEGIN ? SEEK_SET : origin == FILE_CURRENT ? SEEK_CUR : SEEK_END;
    off_t result = lseek(static_cast<FileHandle *>(raw)->fd, offset, whence);
    return result < 0 ? static_cast<DWORD>(-1) : static_cast<DWORD>(result);
}

DWORD GetFileSize(HANDLE raw, LPDWORD high)
{
    struct stat info;
    if (fstat(static_cast<FileHandle *>(raw)->fd, &info) != 0) return static_cast<DWORD>(-1);
    if (high != NULL) *high = static_cast<DWORD>(static_cast<unsigned long long>(info.st_size) >> 32);
    return static_cast<DWORD>(info.st_size);
}

BOOL CloseHandle(HANDLE raw)
{
    if (raw == NULL || raw == INVALID_HANDLE_VALUE || !IsLiveCompatHandle(raw)) return FALSE;
    LinuxHandle *handle = static_cast<LinuxHandle *>(raw);
    if (handle->kind == HANDLE_THREAD)
    {
        ThreadHandle *thread = static_cast<ThreadHandle *>(handle);
        if (!thread->finished) return TRUE;
        if (!thread->joined) pthread_join(thread->thread, NULL);
    }
    g_liveCompatHandles.erase(reinterpret_cast<uintptr_t>(raw));
    delete handle;
    return TRUE;
}

BOOL FlushFileBuffers(HANDLE raw) { return fsync(static_cast<FileHandle *>(raw)->fd) == 0; }
BOOL DeleteFileA(LPCSTR path)
{
    if (path == NULL)
        return FALSE;
    if (unlink(path) == 0)
        return TRUE;
    if (IsAbsolutePath(path))
        return FALSE;
    return unlink(IosUserDataPath(path).c_str()) == 0;
}

DWORD GetFileAttributesW(LPCWSTR path)
{
    char converted[PATH_MAX];
    if (wcstombs(converted, path, sizeof(converted) - 1) == static_cast<size_t>(-1)) return INVALID_FILE_ATTRIBUTES;
    struct stat info;
    if (stat(converted, &info) != 0)
    {
        if (IsAbsolutePath(converted) || stat(IosUserDataPath(converted).c_str(), &info) != 0)
            return INVALID_FILE_ATTRIBUTES;
    }
    return S_ISDIR(info.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}

BOOL SetCurrentDirectoryW(LPCWSTR path)
{
    char converted[PATH_MAX];
    if (wcstombs(converted, path, sizeof(converted) - 1) == static_cast<size_t>(-1)) return FALSE;
    return chdir(converted) == 0;
}

HANDLE FindFirstFileA(LPCSTR pattern, WIN32_FIND_DATAA *data)
{
    glob_t result;
    int globResult = glob(pattern, 0, NULL, &result);
    if (globResult != 0 && !IsAbsolutePath(pattern))
    {
        const std::string userPattern = IosUserDataPath(pattern);
        globResult = glob(userPattern.c_str(), 0, NULL, &result);
    }
    if (globResult != 0) return INVALID_HANDLE_VALUE;
    FindHandle *handle = new FindHandle();
    RegisterCompatHandle(handle);
    for (size_t i = 0; i < result.gl_pathc; ++i) handle->paths.push_back(result.gl_pathv[i]);
    globfree(&result);
    if (!FillFindData(handle, data)) { delete handle; return INVALID_HANDLE_VALUE; }
    return handle;
}

BOOL FindNextFileA(HANDLE raw, WIN32_FIND_DATAA *data) { return FillFindData(static_cast<FindHandle *>(raw), data); }
BOOL FindClose(HANDLE raw)
{
    if (raw == NULL || raw == INVALID_HANDLE_VALUE || !IsLiveCompatHandle(raw))
        return FALSE;
    LinuxHandle *handle = static_cast<LinuxHandle *>(raw);
    if (handle->kind != HANDLE_FIND)
        return FALSE;
    g_liveCompatHandles.erase(reinterpret_cast<uintptr_t>(raw));
    delete static_cast<FindHandle *>(handle);
    return TRUE;
}
void Sleep(DWORD milliseconds) { usleep(static_cast<useconds_t>(milliseconds) * 1000); }

DWORD timeGetTime(void)
{
    struct timeval value; gettimeofday(&value, NULL);
    return static_cast<DWORD>(value.tv_sec * 1000ULL + value.tv_usec / 1000);
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER *value) { value->QuadPart = 1000000; return TRUE; }
BOOL QueryPerformanceCounter(LARGE_INTEGER *value)
{
    struct timeval time; gettimeofday(&time, NULL);
    value->QuadPart = time.tv_sec * 1000000LL + time.tv_usec; return TRUE;
}
DWORD GetCurrentThreadId(void) { return CurrentThreadIdImpl(); }

HANDLE CreateThread(LPVOID, size_t, LPTHREAD_START_ROUTINE start, LPVOID parameter, DWORD, LPDWORD id)
{
    ThreadHandle *handle = new ThreadHandle(); handle->start = start; handle->parameter = parameter;
    if (pthread_create(&handle->thread, NULL, ThreadTrampoline, handle) != 0) { delete handle; return NULL; }
    while (handle->id == 0) sched_yield();
    if (id != NULL) *id = handle->id;
    RegisterCompatHandle(handle);
    return handle;
}

BOOL PostThreadMessageA(DWORD id, UINT message, WPARAM wparam, LPARAM lparam)
{
    MSG value; memset(&value, 0, sizeof(value)); value.message = message; value.wParam = wparam; value.lParam = lparam;
    pthread_mutex_lock(&g_messageMutex); g_threadMessages[id].push_back(value); pthread_mutex_unlock(&g_messageMutex);
    return TRUE;
}

DWORD WaitForSingleObject(HANDLE raw, DWORD timeout)
{
    if (!IsLiveCompatHandle(raw))
        return WAIT_OBJECT_0;
    LinuxHandle *base = static_cast<LinuxHandle *>(raw);
    DWORD start = timeGetTime();
    for (;;)
    {
        if (base->kind == HANDLE_THREAD && static_cast<ThreadHandle *>(base)->finished) return WAIT_OBJECT_0;
        if (base->kind == HANDLE_EVENT)
        {
            EventHandle *event = static_cast<EventHandle *>(base);
            pthread_mutex_lock(&event->mutex);
            if (event->signaled)
            {
                if (!event->manual) event->signaled = false;
                pthread_mutex_unlock(&event->mutex); return WAIT_OBJECT_0;
            }
            pthread_mutex_unlock(&event->mutex);
        }
        if (timeout != INFINITE && timeGetTime() - start >= timeout) return WAIT_TIMEOUT;
        usleep(1000);
    }
}

DWORD MsgWaitForMultipleObjects(DWORD count, const HANDLE *handles, BOOL, DWORD timeout, DWORD)
{
    DWORD start = timeGetTime();
    for (;;)
    {
        for (DWORD i = 0; i < count; ++i) if (WaitForSingleObject(handles[i], 0) == WAIT_OBJECT_0) return i;
        pthread_mutex_lock(&g_messageMutex);
        bool hasMessages = !g_threadMessages[CurrentThreadIdImpl()].empty();
        pthread_mutex_unlock(&g_messageMutex);
        if (hasMessages) return count;
        if (timeout != INFINITE && timeGetTime() - start >= timeout) return WAIT_TIMEOUT;
        usleep(1000);
    }
}

HANDLE CreateEventA(LPVOID, BOOL manual, BOOL initial, LPCSTR)
{
    EventHandle *event = new EventHandle(manual != FALSE, initial != FALSE);
    RegisterCompatHandle(event);
    return event;
}
BOOL SetEvent(HANDLE raw)
{
    EventHandle *event = static_cast<EventHandle *>(raw); pthread_mutex_lock(&event->mutex);
    event->signaled = true; pthread_cond_broadcast(&event->condition); pthread_mutex_unlock(&event->mutex); return TRUE;
}
UINT_PTR SetTimer(HWND, UINT_PTR id, UINT, void *) { return id != 0 ? id : 1; }
BOOL KillTimer(HWND, UINT_PTR) { return TRUE; }
HANDLE CreateMutexA(LPVOID, BOOL, LPCSTR)
{
    g_lastError = 0;
    MutexHandle *mutex = new MutexHandle();
    RegisterCompatHandle(mutex);
    return mutex;
}
DWORD GetLastError(void) { return g_lastError; }
void InitializeCriticalSection(CRITICAL_SECTION *value)
{
    // Win32 critical sections are recursive. A default pthread mutex is not,
    // which can permanently stall the game when one compatibility operation
    // re-enters a subsystem lock on the same thread.
    pthread_mutexattr_t attributes;
    pthread_mutexattr_init(&attributes);
    pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(value, &attributes);
    pthread_mutexattr_destroy(&attributes);
}
void DeleteCriticalSection(CRITICAL_SECTION *value) { pthread_mutex_destroy(value); }
void EnterCriticalSection(CRITICAL_SECTION *value) { pthread_mutex_lock(value); }
void LeaveCriticalSection(CRITICAL_SECTION *value) { pthread_mutex_unlock(value); }

BOOL PeekMessageA(MSG *message, HWND, UINT, UINT, UINT)
{
    pthread_mutex_lock(&g_messageMutex);
    std::vector<MSG> &queue = g_threadMessages[CurrentThreadIdImpl()];
    if (!queue.empty()) { *message = queue.front(); queue.erase(queue.begin()); pthread_mutex_unlock(&g_messageMutex); return TRUE; }
    pthread_mutex_unlock(&g_messageMutex);
    bool hasMessage; PumpSdlEvents(message, &hasMessage); return hasMessage;
}
BOOL TranslateMessage(const MSG *) { return TRUE; }
LRESULT DispatchMessageA(const MSG *message) { return g_windowProcedure != NULL ? g_windowProcedure(message->hwnd, message->message, message->wParam, message->lParam) : 0; }
LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM) { return 0; }
BOOL RegisterClassA(const WNDCLASSA *value) { g_windowProcedure = value->lpfnWndProc; return TRUE; }

HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR title, DWORD style, int, int, int width, int height, HWND, HANDLE, HINSTANCE, LPVOID)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
    { fprintf(stderr, "TH095-modern: SDL_Init failed: %s\n", SDL_GetError()); return NULL; }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#ifdef TH095_IOS
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
#ifdef TH095_IOS
    // UIKit owns the top-level window.  SDL's UIKit backend sizes a normal
    // resizable view to the display and handles rotation; asking it to enter
    // desktop fullscreen here can leave a landscape drawable inside a
    // portrait view on iPadOS.
    SDL_SetHint(SDL_HINT_ORIENTATIONS,
                "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
    // The game renders a 640x480 source. Avoid a 3x Retina presentation
    // surface by default: the simulator otherwise software-rasterizes
    // millions of output pixels per frame without adding source detail.
    flags |= SDL_WINDOW_RESIZABLE;
#if defined(__arm64__) || defined(__aarch64__)
    // Device displays retain Retina sharpness; touch dimensions are scaled
    // from UIKit points independently of the drawable's pixel density.
    flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
    width = 640;
    height = 480;
#else
    if (style == WS_OVERLAPPEDWINDOW) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    else { width = 640; height = 480; }
#endif
    g_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
    if (g_window == NULL) fprintf(stderr, "TH095-modern: SDL_CreateWindow failed: %s\n", SDL_GetError());
    else
    {
#ifdef TH095_IOS
        int windowWidth = 0, windowHeight = 0, drawableWidth = 0, drawableHeight = 0;
        SDL_GetWindowSize(g_window, &windowWidth, &windowHeight);
        SDL_GL_GetDrawableSize(g_window, &drawableWidth, &drawableHeight);
        char windowMessage[192];
        SDL_snprintf(windowMessage, sizeof(windowMessage),
                     "ios: window=%dx%d drawable=%dx%d flags=0x%08x",
                     windowWidth, windowHeight, drawableWidth, drawableHeight,
                     static_cast<unsigned>(flags));
        th095::modern::LogStartup(windowMessage);
#endif
        SetApplicationIcon(g_window);
    }
    return reinterpret_cast<HWND>(g_window);
}

BOOL DestroyWindow(HWND)
{
    if (g_window != NULL)
        SDL_DestroyWindow(g_window);
    g_window = NULL;
#ifdef TH095_IOS
    // Keep SDL's UIKit host alive while the language switch rebuilds the
    // renderer in this process. SDL_Quit tears down the global view host and
    // leaves the subsequent window black on iOS.
    return TRUE;
#else
    SDL_Quit();
    return TRUE;
#endif
}
BOOL ShowWindow(HWND, int) { return TRUE; }
BOOL MoveWindow(HWND, int, int, int, int, BOOL) { return TRUE; }
int ShowCursor(BOOL show) { return SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE); }
HCURSOR SetCursor(HCURSOR value) { return value; }
HCURSOR LoadCursorA(HINSTANCE, LPCSTR) { return reinterpret_cast<HCURSOR>(1); }
HGDIOBJ GetStockObject(int) { return NULL; }
int GetSystemMetrics(int metric) { return metric == SM_CYCAPTION ? 24 : 4; }
BOOL SystemParametersInfoA(UINT, UINT, PVOID value, UINT) { if (value != NULL) *static_cast<BOOL *>(value) = FALSE; return TRUE; }
HWND GetForegroundWindow(void) { return reinterpret_cast<HWND>(g_window); }
DWORD GetWindowThreadProcessId(HWND, LPDWORD process) { if (process != NULL) *process = getpid(); return GetCurrentThreadId(); }
BOOL AttachThreadInput(DWORD, DWORD, BOOL) { return TRUE; }
HWND SetActiveWindow(HWND window) { if (g_window != NULL) SDL_RaiseWindow(g_window); return window; }
LONG GetWindowLongA(HWND, int) { return 0; }
BOOL WINNLSEnableIME(HWND, BOOL) { return TRUE; }
BOOL GetKeyboardState(BYTE *state) { FillKeyboard(state, false); return TRUE; }
BOOL SetKeyboardState(const BYTE *) { return TRUE; }
int MessageBoxA(HWND, LPCSTR text, LPCSTR title, UINT) { fprintf(stderr, "%s: %s\n", title ? title : "TH095", text ? text : ""); return 0; }
int MessageBoxW(HWND, LPCWSTR text, LPCWSTR title, UINT) { fwprintf(stderr, L"%ls: %ls\n", title ? title : L"TH095", text ? text : L""); return 0; }

DWORD GetModuleFileNameA(HMODULE, LPSTR buffer, DWORD size)
{
#ifdef TH095_IOS
    if (buffer == NULL || size == 0) return 0;
    buffer[0] = 0;
    char executablePath[PATH_MAX];
    uint32_t capacity = sizeof(executablePath);
    if (_NSGetExecutablePath(executablePath, &capacity) != 0) return 0;
    const size_t length = strlen(executablePath);
    const size_t copied = length < size ? length : size - 1;
    memcpy(buffer, executablePath, copied);
    buffer[copied] = 0;
    return length < size ? static_cast<DWORD>(copied) : size;
#else
    ssize_t count = readlink("/proc/self/exe", buffer, size - 1); if (count < 0) return 0;
    buffer[count] = 0; return static_cast<DWORD>(count);
#endif
}
DWORD GetConsoleTitleA(LPSTR buffer, DWORD size) { if (size) buffer[0] = 0; return 0; }
void GetStartupInfoA(STARTUPINFOA *value) { DWORD size = value->cb; memset(value, 0, size); value->cb = size; }
int MultiByteToWideChar(UINT, DWORD, LPCSTR source, int sourceSize, LPWSTR dest, int destSize)
{
    size_t result = mbstowcs(dest, source, destSize); return result == static_cast<size_t>(-1) ? 0 : static_cast<int>(result + (sourceSize < 0));
}
DWORD FormatMessageA(DWORD flags, LPCVOID, DWORD error, DWORD, LPSTR buffer, DWORD size, va_list *)
{
    const char *message = strerror(error); if (flags & FORMAT_MESSAGE_ALLOCATE_BUFFER) *reinterpret_cast<char **>(buffer) = strdup(message);
    else if (size) { strncpy(buffer, message, size - 1); buffer[size - 1] = 0; } return strlen(message);
}
LPVOID LocalFree(LPVOID value) { free(value); return NULL; }
HGLOBAL GlobalAlloc(UINT, size_t size) { return calloc(1, size); }
HGLOBAL GlobalFree(HGLOBAL value) { free(value); return NULL; }
HMODULE LoadLibraryA(LPCSTR path) { return dlopen(path, RTLD_NOW); }
void *GetProcAddress(HMODULE module, LPCSTR name) { return dlsym(module, name); }
HDC CreateCompatibleDC(HDC) { return new GdiDc(); }
BOOL DeleteDC(HDC value) { delete static_cast<GdiDc *>(value); return TRUE; }
HGDIOBJ SelectObject(HDC dcRaw, HGDIOBJ objectRaw)
{
    if (dcRaw == NULL || objectRaw == NULL) return NULL;
    GdiDc *dc = static_cast<GdiDc *>(dcRaw);
    GdiObject *object = static_cast<GdiObject *>(objectRaw);
    if (object->kind == GdiObject::BITMAP)
    {
        GdiBitmap *old = dc->bitmap;
        dc->bitmap = static_cast<GdiBitmap *>(object);
        return old;
    }
    GdiFont *old = dc->font;
    dc->font = static_cast<GdiFont *>(object);
    return old;
}
BOOL DeleteObject(HGDIOBJ value)
{
    if (value == NULL)
        return TRUE;
    const uintptr_t address = reinterpret_cast<uintptr_t>(value);
    std::map<uintptr_t, bool>::iterator live = g_liveGdiObjects.find(address);
    if (live == g_liveGdiObjects.end())
        return TRUE;
    g_liveGdiObjects.erase(live);
    delete static_cast<GdiObject *>(value);
    return TRUE;
}
int SetBkMode(HDC, int mode) { return mode; }
COLORREF SetTextColor(HDC raw, COLORREF color) { GdiDc *dc = static_cast<GdiDc *>(raw); COLORREF old = dc->color; dc->color = color; return old; }
HFONT CreateFontA(int height, int, int, int, int weight, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR)
{
    const TextRasterLock lock;
    const bool localizedUtf8 = g_gdiTextUtf8;
    const char *path = ResolveJapaneseFont(localizedUtf8);
    if (path == NULL)
    {
        GdiFont *font = new GdiFont(NULL, localizedUtf8);
        g_liveGdiObjects[reinterpret_cast<uintptr_t>(font)] = true;
        return font;
    }
    if (!TTF_WasInit() && TTF_Init() != 0)
    {
        fprintf(stderr, "TH095-modern: SDL_ttf initialization failed: %s\n", TTF_GetError());
        GdiFont *font = new GdiFont(NULL, localizedUtf8);
        g_liveGdiObjects[reinterpret_cast<uintptr_t>(font)] = true;
        return font;
    }
    TTF_Font *font = GetCachedRasterFont(path, height < 0 ? -height : height,
                                        weight >= FW_SEMIBOLD ? TTF_STYLE_BOLD : TTF_STYLE_NORMAL);
    if (font == NULL)
    {
        fprintf(stderr, "TH095-modern: unable to load Japanese font %s: %s\n", path, TTF_GetError());
        GdiFont *font = new GdiFont(NULL, localizedUtf8);
        g_liveGdiObjects[reinterpret_cast<uintptr_t>(font)] = true;
        return font;
    }
    GdiFont *result = new GdiFont(font, localizedUtf8);
    g_liveGdiObjects[reinterpret_cast<uintptr_t>(result)] = true;
    return result;
}
HBITMAP CreateDIBSection(HDC, const void *infoRaw, UINT, VOID **pixels, HANDLE, DWORD)
{
    const BITMAPINFO *info = static_cast<const BITMAPINFO *>(infoRaw); GdiBitmap *bitmap = new GdiBitmap(info->bmiHeader.biWidth, info->bmiHeader.biHeight, info->bmiHeader.biBitCount);
    if (info->bmiHeader.biCompression == BI_BITFIELDS && bitmap->bits == 16)
    {
        const DWORD *masks = reinterpret_cast<const DWORD *>(
            static_cast<const BYTE *>(infoRaw) + sizeof(BITMAPINFOHEADER));
        bitmap->redMask = masks[0];
        bitmap->greenMask = masks[1];
        bitmap->blueMask = masks[2];
    }
    g_liveGdiObjects[reinterpret_cast<uintptr_t>(bitmap)] = true;
    *pixels = bitmap->pixels.empty() ? NULL : &bitmap->pixels[0]; return bitmap;
}
BOOL TextOutA(HDC dcRaw, int x, int y, LPCSTR text, int length)
{
    const TextRasterLock lock;
    if (dcRaw == NULL || text == NULL || length <= 0) return FALSE;
    GdiDc *dc = static_cast<GdiDc *>(dcRaw);
    if (dc->bitmap == NULL || dc->font == NULL || dc->font->font == NULL) return FALSE;
    const std::string utf8 = dc->font->utf8
                                 ? std::string(text, static_cast<size_t>(length))
                                 : ConvertCp932ToUtf8(text, static_cast<size_t>(length));
    SDL_Surface *glyph = GetGdiGlyph(dc->font->font, utf8);
    if (glyph == NULL) return FALSE;
    if (SDL_MUSTLOCK(glyph)) SDL_LockSurface(glyph);
    for (int row = 0; row < glyph->h; ++row)
    {
        const BYTE *source = static_cast<const BYTE *>(glyph->pixels) + row * glyph->pitch;
        for (int column = 0; column < glyph->w; ++column)
            PutGdiTextPixel(dc->bitmap, x + column, y + row, dc->color, source[column * 4 + 3]);
    }
    if (SDL_MUSTLOCK(glyph)) SDL_UnlockSurface(glyph);
    return TRUE;
}
HRESULT CoInitialize(LPVOID) { return S_OK; }
void CoUninitialize(void) {}
HRESULT CoCreateInstance(REFGUID, LPVOID, DWORD, REFIID, LPVOID *) { return E_NOTIMPL; }
} // extern C

extern const GUID CLSID_ShellLink = {0};
extern const GUID IID_IShellLink = {1};
extern const GUID IID_IPersistFile = {2};
const GUID GUID_NULL = {0};
const GUID IID_IDirectSoundNotify = {3};
const GUID IID_IDirectInput8A = {4};
const GUID GUID_SysKeyboard = {5};
const GUID DIPROP_RANGE = {6};
const DIDATAFORMAT c_dfDIKeyboard = {sizeof(DIDATAFORMAT)};
const DIDATAFORMAT c_dfDIJoystick = {sizeof(DIDATAFORMAT)};

MMRESULT timeGetDevCaps(TIMECAPS *caps, UINT) { caps->wPeriodMin = 1; caps->wPeriodMax = 1000; return 0; }
MMRESULT timeBeginPeriod(UINT) { return 0; }
MMRESULT timeEndPeriod(UINT) { return 0; }
UINT timeSetEvent(UINT, UINT, LPTIMECALLBACK, DWORD_PTR, UINT) { static UINT id = 1; return id++; }
MMRESULT timeKillEvent(UINT) { return 0; }
MMRESULT midiOutOpen(HMIDIOUT *handle, UINT, DWORD_PTR, DWORD_PTR, DWORD) { *handle = reinterpret_cast<HMIDIOUT>(1); return 0; }
MMRESULT midiOutClose(HMIDIOUT) { return 0; }
MMRESULT midiOutReset(HMIDIOUT) { return 0; }
MMRESULT midiOutPrepareHeader(HMIDIOUT, LPMIDIHDR, UINT) { return 0; }
MMRESULT midiOutUnprepareHeader(HMIDIOUT, LPMIDIHDR, UINT) { return 0; }
MMRESULT midiOutLongMsg(HMIDIOUT, LPMIDIHDR header, UINT) { header->dwFlags |= 1; return 0; }
MMRESULT midiOutShortMsg(HMIDIOUT, DWORD) { return 0; }
MMRESULT joyGetPosEx(UINT, JOYINFOEX *) { return 1; }
MMRESULT joyGetDevCapsA(UINT_PTR, JOYCAPSA *caps, UINT) { memset(caps, 0, sizeof(*caps)); caps->wXmax = caps->wYmax = 65535; return 1; }

class LinuxInputDevice : public IDirectInputDevice8A
{
  public:
    explicit LinuxInputDevice(bool keyboard_) : refs(1), keyboard(keyboard_) {}
    ULONG Release() { if (--refs == 0) { delete this; return 0; } return refs; }
    HRESULT GetCapabilities(DIDEVCAPS *caps) { caps->dwAxes = keyboard ? 0 : 2; caps->dwButtons = keyboard ? 0 : 32; return S_OK; }
    HRESULT EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA, LPVOID, DWORD) { return S_OK; }
    HRESULT GetDeviceState(DWORD size, LPVOID data)
    {
        if (keyboard && size >= 256) FillKeyboard(static_cast<BYTE *>(data), true);
        else memset(data, 0, size); return S_OK;
    }
    HRESULT SetDataFormat(const DIDATAFORMAT *) { return S_OK; }
    HRESULT SetCooperativeLevel(HWND, DWORD) { return S_OK; }
    HRESULT SetProperty(REFGUID, const DIPROPHEADER *) { return S_OK; }
    HRESULT Acquire() { return S_OK; }
    HRESULT Unacquire() { return S_OK; }
    HRESULT Poll() { return S_OK; }
  private:
    ULONG refs; bool keyboard;
};

class LinuxDirectInput : public IDirectInput8A
{
  public:
    LinuxDirectInput() : refs(1) {}
    ULONG Release() { if (--refs == 0) { delete this; return 0; } return refs; }
    HRESULT CreateDevice(REFGUID guid, IDirectInputDevice8A **device, LPVOID)
    { *device = new LinuxInputDevice(guid.Data1 == GUID_SysKeyboard.Data1); return S_OK; }
    HRESULT EnumDevices(DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD) { return S_OK; }
  private: ULONG refs;
};

HRESULT DirectInput8Create(HINSTANCE, DWORD, REFIID, LPVOID *out, LPVOID) { *out = new LinuxDirectInput(); return S_OK; }

class LinuxSoundBuffer;

SDL_AudioDeviceID g_audioDevice;
std::vector<LinuxSoundBuffer *> g_soundBuffers;

void LockAudio()
{
    if (g_audioDevice != 0) SDL_LockAudioDevice(g_audioDevice);
}

void UnlockAudio()
{
    if (g_audioDevice != 0) SDL_UnlockAudioDevice(g_audioDevice);
}

class LinuxSoundNotify : public IDirectSoundNotify
{
  public:
    explicit LinuxSoundNotify(LinuxSoundBuffer *buffer_) : refs(1), buffer(buffer_) {}
    ULONG Release() { if (--refs == 0) { delete this; return 0; } return refs; }
    HRESULT SetNotificationPositions(DWORD count, const DSBPOSITIONNOTIFY *positions);
  private: ULONG refs; LinuxSoundBuffer *buffer;
};

class LinuxSoundBuffer : public IDirectSoundBuffer
{
  public:
    explicit LinuxSoundBuffer(const DSBUFFERDESC *desc)
        : refs(1), playing(false), looping(false), position(0), cursorFrame(0.0), volume(0), pan(0),
          hasFormat(false), locked(false)
    {
        memset(&format, 0, sizeof(format));
        if (desc != NULL)
        {
            bytes.resize(desc->dwBufferBytes);
            if (desc->lpwfxFormat != NULL) { format = *desc->lpwfxFormat; hasFormat = true; }
        }
        LockAudio(); g_soundBuffers.push_back(this); UnlockAudio();
    }
    LinuxSoundBuffer(const LinuxSoundBuffer &other)
        : refs(1), bytes(other.bytes), playing(false), looping(false), position(0), cursorFrame(0.0),
          volume(other.volume), pan(other.pan), format(other.format), hasFormat(other.hasFormat), locked(false)
    { LockAudio(); g_soundBuffers.push_back(this); UnlockAudio(); }
    ~LinuxSoundBuffer()
    {
        LockAudio();
        for (std::vector<LinuxSoundBuffer *>::iterator it = g_soundBuffers.begin(); it != g_soundBuffers.end(); ++it)
            if (*it == this) { g_soundBuffers.erase(it); break; }
        UnlockAudio();
    }
    ULONG Release() { if (--refs == 0) { delete this; return 0; } return refs; }
    HRESULT QueryInterface(REFIID, void **out) { *out = new LinuxSoundNotify(this); return S_OK; }
    HRESULT GetCurrentPosition(LPDWORD play, LPDWORD write)
    {
        LockAudio();
        if (play) *play = position;
        if (write) *write = position;
        UnlockAudio();
        return S_OK;
    }
    HRESULT GetStatus(LPDWORD status)
    { LockAudio(); *status = playing ? DSBSTATUS_PLAYING : 0; UnlockAudio(); return S_OK; }
    HRESULT Initialize(void *, const DSBUFFERDESC *) { return S_OK; }
    HRESULT Lock(DWORD offset, DWORD length, LPVOID *first, LPDWORD firstSize, LPVOID *second, LPDWORD secondSize, DWORD)
    {
        LockAudio(); locked = true;
        if (bytes.empty()) bytes.resize(length ? length : 1); offset %= bytes.size(); if (length == 0 || length > bytes.size()) length = bytes.size();
        DWORD contiguous = static_cast<DWORD>(bytes.size() - offset); if (contiguous > length) contiguous = length;
        *first = &bytes[offset]; *firstSize = contiguous; if (second) *second = length > contiguous ? &bytes[0] : NULL; if (secondSize) *secondSize = length - contiguous; return S_OK;
    }
    HRESULT Play(DWORD, DWORD, DWORD flags)
    { LockAudio(); playing = true; looping = (flags & DSBPLAY_LOOPING) != 0; UnlockAudio(); return S_OK; }
    HRESULT SetCurrentPosition(DWORD value)
    {
        LockAudio();
        position = bytes.empty() ? 0 : value % bytes.size();
        cursorFrame = FrameBytes() != 0 ? static_cast<double>(position / FrameBytes()) : 0.0;
        UnlockAudio();
        return S_OK;
    }
    HRESULT SetFormat(const WAVEFORMATEX *value)
    { if (value != NULL) { LockAudio(); format = *value; hasFormat = true; UnlockAudio(); } return S_OK; }
    HRESULT SetVolume(LONG value) { LockAudio(); volume = value; UnlockAudio(); return S_OK; }
    HRESULT SetPan(LONG value) { LockAudio(); pan = value; UnlockAudio(); return S_OK; }
    HRESULT Stop() { LockAudio(); playing = false; UnlockAudio(); return S_OK; }
    HRESULT Unlock(LPVOID, DWORD, LPVOID, DWORD)
    { if (locked) { locked = false; UnlockAudio(); } return S_OK; }
    HRESULT Restore() { return S_OK; }
    void SetNotifications(DWORD count, const DSBPOSITIONNOTIFY *positions)
    {
        LockAudio();
        if (count == 0)
            notifications.clear();
        else
            notifications.assign(positions, positions + count);
        UnlockAudio();
    }
    void Mix(Sint16 *output, int outputFrames)
    {
        const DWORD frameBytes = FrameBytes();
        if (!playing || !hasFormat || bytes.empty() || frameBytes == 0 || format.wFormatTag != WAVE_FORMAT_PCM)
            return;
        const DWORD sourceFrames = static_cast<DWORD>(bytes.size() / frameBytes);
        if (sourceFrames == 0) return;
        const double step = static_cast<double>(format.nSamplesPerSec) / 44100.0;
        const float gain = volume <= DSBVOLUME_MIN ? 0.0f : powf(10.0f, static_cast<float>(volume) / 2000.0f);
        const float panValue = pan < -10000 ? -1.0f : pan > 10000 ? 1.0f : static_cast<float>(pan) / 10000.0f;
        const float leftGain = gain * (panValue > 0.0f ? 1.0f - panValue : 1.0f);
        const float rightGain = gain * (panValue < 0.0f ? 1.0f + panValue : 1.0f);
        const DWORD oldPosition = position;
        bool wrapped = false;

        for (int frame = 0; frame < outputFrames && playing; ++frame)
        {
            DWORD sourceFrame = static_cast<DWORD>(cursorFrame);
            if (sourceFrame >= sourceFrames)
            {
                if (!looping) { playing = false; break; }
                cursorFrame -= sourceFrames; sourceFrame = static_cast<DWORD>(cursorFrame); wrapped = true;
            }
            const BYTE *source = &bytes[sourceFrame * frameBytes];
            int left, right;
            if (format.wBitsPerSample == 8)
            {
                left = (static_cast<int>(source[0]) - 128) << 8;
                right = format.nChannels > 1 ? (static_cast<int>(source[1]) - 128) << 8 : left;
            }
            else if (format.wBitsPerSample == 16)
            {
                INT16 leftSample, rightSample;
                memcpy(&leftSample, source, sizeof(leftSample));
                if (format.nChannels > 1) memcpy(&rightSample, source + sizeof(INT16), sizeof(rightSample));
                else rightSample = leftSample;
                left = leftSample; right = rightSample;
            }
            else
                break;

            int mixedLeft = output[frame * 2] + static_cast<int>(left * leftGain);
            int mixedRight = output[frame * 2 + 1] + static_cast<int>(right * rightGain);
            if (mixedLeft < -32768) mixedLeft = -32768; else if (mixedLeft > 32767) mixedLeft = 32767;
            if (mixedRight < -32768) mixedRight = -32768; else if (mixedRight > 32767) mixedRight = 32767;
            output[frame * 2] = static_cast<Sint16>(mixedLeft);
            output[frame * 2 + 1] = static_cast<Sint16>(mixedRight);
            cursorFrame += step;
        }

        if (cursorFrame >= sourceFrames)
        {
            if (looping) { cursorFrame = fmod(cursorFrame, static_cast<double>(sourceFrames)); wrapped = true; }
            else { cursorFrame = sourceFrames; playing = false; }
        }
        position = static_cast<DWORD>(cursorFrame) * frameBytes;
        for (size_t index = 0; index < notifications.size(); ++index)
        {
            const DWORD offset = notifications[index].dwOffset;
            if ((!wrapped && oldPosition <= offset && position > offset) ||
                (wrapped && (offset >= oldPosition || offset < position)))
                SetEvent(notifications[index].hEventNotify);
        }
    }
  private:
    DWORD FrameBytes() const
    { return hasFormat && format.nBlockAlign != 0 ? format.nBlockAlign : 0; }
    ULONG refs;
    std::vector<BYTE> bytes;
    bool playing, looping;
    DWORD position;
    double cursorFrame;
    LONG volume, pan;
    WAVEFORMATEX format;
    bool hasFormat, locked;
    std::vector<DSBPOSITIONNOTIFY> notifications;
};

HRESULT LinuxSoundNotify::SetNotificationPositions(DWORD count, const DSBPOSITIONNOTIFY *positions)
{ if (buffer == NULL || (count != 0 && positions == NULL)) return E_INVALIDARG; buffer->SetNotifications(count, positions); return S_OK; }

void AudioCallback(void *, Uint8 *stream, int length)
{
    memset(stream, 0, length);
    Sint16 *output = reinterpret_cast<Sint16 *>(stream);
    const int frames = length / (sizeof(Sint16) * 2);
    for (size_t index = 0; index < g_soundBuffers.size(); ++index)
        g_soundBuffers[index]->Mix(output, frames);
}

void EnsureAudio()
{
    if (g_audioDevice != 0) return;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    { fprintf(stderr, "TH095-modern: SDL audio initialization failed: %s\n", SDL_GetError()); return; }
    SDL_AudioSpec requested, obtained;
    memset(&requested, 0, sizeof(requested));
    requested.freq = 44100; requested.format = AUDIO_S16SYS; requested.channels = 2;
    requested.samples = 1024; requested.callback = AudioCallback;
    g_audioDevice = SDL_OpenAudioDevice(NULL, 0, &requested, &obtained, 0);
    if (g_audioDevice == 0)
    { fprintf(stderr, "TH095-modern: SDL audio device unavailable: %s\n", SDL_GetError()); return; }
    SDL_PauseAudioDevice(g_audioDevice, 0);
}

void ShutdownAudio()
{
    if (g_audioDevice == 0) return;
    SDL_CloseAudioDevice(g_audioDevice); g_audioDevice = 0;
}

class LinuxDirectSound : public IDirectSound8
{
  public:
    LinuxDirectSound() : refs(1) { EnsureAudio(); }
    ~LinuxDirectSound() { ShutdownAudio(); }
    ULONG Release() { if (--refs == 0) { delete this; return 0; } return refs; }
    HRESULT CreateSoundBuffer(const DSBUFFERDESC *desc, IDirectSoundBuffer **out, LPVOID) { *out = new LinuxSoundBuffer(desc); return S_OK; }
    HRESULT DuplicateSoundBuffer(IDirectSoundBuffer *source, IDirectSoundBuffer **out) { *out = new LinuxSoundBuffer(*static_cast<LinuxSoundBuffer *>(source)); return S_OK; }
    HRESULT SetCooperativeLevel(HWND, DWORD) { return S_OK; }
  private: ULONG refs;
};

HRESULT DirectSoundCreate8(const GUID *, LPDIRECTSOUND8 *out, LPVOID) { *out = new LinuxDirectSound(); return S_OK; }
