#include "d3d8_internal.hpp"
#include "GameManager.hpp"
#include "Gui.hpp"
#include "Supervisor.hpp"
#include "GameplayGlobals.hpp"
#include "PhotoGameTask.hpp"

#include <SDL.h>
#include "ios_gl_legacy.hpp"
#include "ios_touch.hpp"

#include <math.h>
#include <new>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

namespace
{
class LinuxTexture;

typedef void (APIENTRY *GenFramebuffersFunction)(GLsizei, GLuint *);
typedef void (APIENTRY *BindFramebufferFunction)(GLenum, GLuint);
typedef void (APIENTRY *FramebufferTexture2DFunction)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum (APIENTRY *CheckFramebufferStatusFunction)(GLenum);
typedef void (APIENTRY *DeleteFramebuffersFunction)(GLsizei, const GLuint *);
typedef void (APIENTRY *GenRenderbuffersFunction)(GLsizei, GLuint *);
typedef void (APIENTRY *BindRenderbufferFunction)(GLenum, GLuint);
typedef void (APIENTRY *RenderbufferStorageFunction)(GLenum, GLenum, GLsizei, GLsizei);
typedef void (APIENTRY *FramebufferRenderbufferFunction)(GLenum, GLenum, GLenum, GLuint);
typedef void (APIENTRY *DeleteRenderbuffersFunction)(GLsizei, const GLuint *);
typedef void (APIENTRY *FogCoordfFunction)(GLfloat);

struct FramebufferApi
{
    FramebufferApi()
        : genFramebuffers(NULL), bindFramebuffer(NULL), framebufferTexture2D(NULL),
          checkFramebufferStatus(NULL), deleteFramebuffers(NULL), genRenderbuffers(NULL),
          bindRenderbuffer(NULL), renderbufferStorage(NULL), framebufferRenderbuffer(NULL),
          deleteRenderbuffers(NULL)
    {
    }

    void *Load(const char *coreName, const char *extensionName)
    {
        void *procedure = SDL_GL_GetProcAddress(coreName);
        return procedure != NULL ? procedure : SDL_GL_GetProcAddress(extensionName);
    }

    bool Initialize()
    {
        genFramebuffers = reinterpret_cast<GenFramebuffersFunction>(
            Load("glGenFramebuffers", "glGenFramebuffersEXT"));
        bindFramebuffer = reinterpret_cast<BindFramebufferFunction>(
            Load("glBindFramebuffer", "glBindFramebufferEXT"));
        framebufferTexture2D = reinterpret_cast<FramebufferTexture2DFunction>(
            Load("glFramebufferTexture2D", "glFramebufferTexture2DEXT"));
        checkFramebufferStatus = reinterpret_cast<CheckFramebufferStatusFunction>(
            Load("glCheckFramebufferStatus", "glCheckFramebufferStatusEXT"));
        deleteFramebuffers = reinterpret_cast<DeleteFramebuffersFunction>(
            Load("glDeleteFramebuffers", "glDeleteFramebuffersEXT"));
        genRenderbuffers = reinterpret_cast<GenRenderbuffersFunction>(
            Load("glGenRenderbuffers", "glGenRenderbuffersEXT"));
        bindRenderbuffer = reinterpret_cast<BindRenderbufferFunction>(
            Load("glBindRenderbuffer", "glBindRenderbufferEXT"));
        renderbufferStorage = reinterpret_cast<RenderbufferStorageFunction>(
            Load("glRenderbufferStorage", "glRenderbufferStorageEXT"));
        framebufferRenderbuffer = reinterpret_cast<FramebufferRenderbufferFunction>(
            Load("glFramebufferRenderbuffer", "glFramebufferRenderbufferEXT"));
        deleteRenderbuffers = reinterpret_cast<DeleteRenderbuffersFunction>(
            Load("glDeleteRenderbuffers", "glDeleteRenderbuffersEXT"));
        return genFramebuffers != NULL && bindFramebuffer != NULL && framebufferTexture2D != NULL &&
               checkFramebufferStatus != NULL && deleteFramebuffers != NULL &&
               genRenderbuffers != NULL && bindRenderbuffer != NULL &&
               renderbufferStorage != NULL && framebufferRenderbuffer != NULL &&
               deleteRenderbuffers != NULL;
    }

    GenFramebuffersFunction genFramebuffers;
    BindFramebufferFunction bindFramebuffer;
    FramebufferTexture2DFunction framebufferTexture2D;
    CheckFramebufferStatusFunction checkFramebufferStatus;
    DeleteFramebuffersFunction deleteFramebuffers;
    GenRenderbuffersFunction genRenderbuffers;
    BindRenderbufferFunction bindRenderbuffer;
    RenderbufferStorageFunction renderbufferStorage;
    FramebufferRenderbufferFunction framebufferRenderbuffer;
    DeleteRenderbuffersFunction deleteRenderbuffers;
};

FramebufferApi g_framebufferApi;
FogCoordfFunction g_fogCoordf;

UINT BytesPerPixel(D3DFORMAT format)
{
    switch (format)
    {
    case D3DFMT_R8G8B8: return 3;
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4: return 2;
    default: return 4;
    }
}

void DecodePixel(const BYTE *source, D3DFORMAT format, BYTE *rgba)
{
    WORD pixel;
    switch (format)
    {
    case D3DFMT_R8G8B8:
        rgba[0] = source[2]; rgba[1] = source[1]; rgba[2] = source[0]; rgba[3] = 255; break;
    case D3DFMT_R5G6B5:
        memcpy(&pixel, source, sizeof(pixel));
        rgba[0] = static_cast<BYTE>(((pixel >> 11) & 31) * 255 / 31);
        rgba[1] = static_cast<BYTE>(((pixel >> 5) & 63) * 255 / 63);
        rgba[2] = static_cast<BYTE>((pixel & 31) * 255 / 31); rgba[3] = 255; break;
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
        memcpy(&pixel, source, sizeof(pixel));
        rgba[0] = static_cast<BYTE>(((pixel >> 10) & 31) * 255 / 31);
        rgba[1] = static_cast<BYTE>(((pixel >> 5) & 31) * 255 / 31);
        rgba[2] = static_cast<BYTE>((pixel & 31) * 255 / 31);
        rgba[3] = format == D3DFMT_A1R5G5B5 && !(pixel & 0x8000) ? 0 : 255; break;
    case D3DFMT_A4R4G4B4:
        memcpy(&pixel, source, sizeof(pixel));
        rgba[0] = static_cast<BYTE>(((pixel >> 8) & 15) * 17);
        rgba[1] = static_cast<BYTE>(((pixel >> 4) & 15) * 17);
        rgba[2] = static_cast<BYTE>((pixel & 15) * 17);
        rgba[3] = static_cast<BYTE>(((pixel >> 12) & 15) * 17); break;
    default:
        rgba[0] = source[2]; rgba[1] = source[1]; rgba[2] = source[0];
        rgba[3] = format == D3DFMT_X8R8G8B8 ? 255 : source[3]; break;
    }
}

void EncodePixel(BYTE *destination, D3DFORMAT format, const BYTE *rgba)
{
    WORD pixel;
    switch (format)
    {
    case D3DFMT_R8G8B8:
        destination[0] = rgba[2]; destination[1] = rgba[1]; destination[2] = rgba[0]; break;
    case D3DFMT_R5G6B5:
        pixel = static_cast<WORD>(((rgba[0] * 31 / 255) << 11) |
                                  ((rgba[1] * 63 / 255) << 5) | (rgba[2] * 31 / 255));
        memcpy(destination, &pixel, sizeof(pixel)); break;
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
        pixel = static_cast<WORD>(((format == D3DFMT_X1R5G5B5 || rgba[3] >= 128) ? 0x8000 : 0) |
                                  ((rgba[0] * 31 / 255) << 10) |
                                  ((rgba[1] * 31 / 255) << 5) | (rgba[2] * 31 / 255));
        memcpy(destination, &pixel, sizeof(pixel)); break;
    case D3DFMT_A4R4G4B4:
        pixel = static_cast<WORD>(((rgba[3] >> 4) << 12) | ((rgba[0] >> 4) << 8) |
                                  ((rgba[1] >> 4) << 4) | (rgba[2] >> 4));
        memcpy(destination, &pixel, sizeof(pixel)); break;
    default:
        destination[0] = rgba[2]; destination[1] = rgba[1]; destination[2] = rgba[0];
        destination[3] = format == D3DFMT_X8R8G8B8 ? 255 : rgba[3]; break;
    }
}

void Identity(D3DMATRIX *matrix)
{
    memset(matrix, 0, sizeof(*matrix));
    matrix->_11 = matrix->_22 = matrix->_33 = matrix->_44 = 1.0f;
}

void DrawPresentationRegion(int sourceX, int sourceY, int sourceWidth, int sourceHeight,
                            int destinationX, int destinationY,
                            int destinationWidth, int destinationHeight)
{
    // D3D8 sprite rectangles address texel edges, while the GLES sampler
    // uses texel centres. With GL_LINEAR, sampling the exact outer edge
    // blends the adjacent playfield/background texel into the presentation
    // region (visible as a persistent fragment at x=32). Pull both edges in
    // by half a texel so the crop is isolated from its neighbours.
    const float u0 = static_cast<float>(sourceX + 0.5f) / 640.0f;
    const float u1 = static_cast<float>(sourceX + sourceWidth - 0.5f) / 640.0f;
    const float vTop = 1.0f - static_cast<float>(sourceY + 0.5f) / 480.0f;
    const float vBottom =
        1.0f - static_cast<float>(sourceY + sourceHeight - 0.5f) / 480.0f;
    const float x0 = static_cast<float>(destinationX);
    const float x1 = static_cast<float>(destinationX + destinationWidth);
    const float y0 = static_cast<float>(destinationY);
    const float y1 = static_cast<float>(destinationY + destinationHeight);

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(u0, vTop); glVertex2f(x0, y0);
    glTexCoord2f(u1, vTop); glVertex2f(x1, y0);
    glTexCoord2f(u0, vBottom); glVertex2f(x0, y1);
    glTexCoord2f(u1, vBottom); glVertex2f(x1, y1);
    glEnd();
}

struct IosClipVertex
{
    float x, y, z, w;
    float u, v;
    float red, green, blue, alpha;
    float fogCoordinate;
};

class LinuxSurface : public IDirect3DSurface8
{
  public:
    LinuxSurface(UINT width_, UINT height_, D3DFORMAT format_, bool backbuffer_, LinuxTexture *owner_)
        : refs(1), width(width_), height(height_), format(format_), backbuffer(backbuffer_), owner(owner_),
          dirty(!backbuffer_), blitTexture(0)
    {
        pitch = width * BytesPerPixel(format);
        pixels.resize(pitch * height);
    }
    ~LinuxSurface()
    {
        if (blitTexture != 0)
            glDeleteTextures(1, &blitTexture);
    }
    ULONG AddRef() { return ++refs; }
    ULONG Release() { ULONG value = --refs; if (value == 0) delete this; return value; }
    HRESULT GetDesc(D3DSURFACE_DESC *description)
    {
        if (description == NULL) return E_INVALIDARG;
        memset(description, 0, sizeof(*description));
        description->Format = format; description->Type = D3DRTYPE_SURFACE;
        description->Pool = backbuffer ? D3DPOOL_DEFAULT : D3DPOOL_SYSTEMMEM;
        description->Size = static_cast<UINT>(pixels.size());
        description->Width = width; description->Height = height; return S_OK;
    }
    HRESULT LockRect(D3DLOCKED_RECT *locked, const RECT *rect, DWORD flags)
    {
        if (locked == NULL) return E_INVALIDARG;
        if (backbuffer && (flags & D3DLOCK_READONLY)) ReadBackbuffer();
        UINT left = rect != NULL && rect->left > 0 ? static_cast<UINT>(rect->left) : 0;
        UINT top = rect != NULL && rect->top > 0 ? static_cast<UINT>(rect->top) : 0;
        if (left >= width || top >= height) return E_INVALIDARG;
        locked->Pitch = pitch;
        locked->pBits = &pixels[top * pitch + left * BytesPerPixel(format)]; return S_OK;
    }
    HRESULT UnlockRect() { dirty = true; return S_OK; }
    HRESULT GetDC(HDC *dc)
    { if (dc == NULL) return E_INVALIDARG; *dc = CreateCompatibleDC(NULL); return *dc ? S_OK : E_FAIL; }
    HRESULT ReleaseDC(HDC dc) { return DeleteDC(dc) ? S_OK : E_FAIL; }
    void ReadBackbuffer()
    {
        if (!backbuffer || width == 0 || height == 0) return;
        std::vector<BYTE> rgba(width * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &rgba[0]);
        const UINT bytes = BytesPerPixel(format);
        for (UINT y = 0; y < height; ++y)
            for (UINT x = 0; x < width; ++x)
                EncodePixel(&pixels[y * pitch + x * bytes], format,
                            &rgba[((height - 1 - y) * width + x) * 4]);
        dirty = false;
    }
    void FlushBackbuffer()
    {
        if (!backbuffer || !dirty || width == 0 || height == 0) return;
        const bool directBgra =
            (format == D3DFMT_X8R8G8B8 || format == D3DFMT_A8R8G8B8) &&
            pitch == width * 4;
        std::vector<BYTE> rgba;
        const BYTE *uploadPixels = &pixels[0];
        GLenum uploadFormat = GL_BGRA;
        if (!directBgra)
        {
            rgba.resize(width * height * 4);
            const UINT bytes = BytesPerPixel(format);
            for (UINT y = 0; y < height; ++y)
                for (UINT x = 0; x < width; ++x)
                    DecodePixel(&pixels[y * pitch + x * bytes], format,
                                &rgba[(y * width + x) * 4]);
            uploadPixels = &rgba[0];
            uploadFormat = GL_RGBA;
        }

        const bool allocateTexture = blitTexture == 0;
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        // Every compatibility draw samples from texture unit zero.  A 3-D
        // background may leave unit one active, and binding the blit texture
        // there makes the following text batch sample an unrelated texture.
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST); glDisable(GL_SCISSOR_TEST);
        glDepthMask(GL_FALSE);
        if (allocateTexture)
            glGenTextures(1, &blitTexture);
        glBindTexture(GL_TEXTURE_2D, blitTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (allocateTexture)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                         uploadFormat, GL_UNSIGNED_BYTE, uploadPixels);
        else
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                            uploadFormat, GL_UNSIGNED_BYTE, uploadPixels);
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
        IosLegacySetTextureUsage(GL_TRUE, GL_FALSE);
        glColor4ub(255, 255, 255, 255);
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(static_cast<float>(width), 0.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, static_cast<float>(height));
        glTexCoord2f(1.0f, 1.0f); glVertex2f(static_cast<float>(width), static_cast<float>(height));
        glEnd();
        IosLegacySetTextureUsage(GL_TRUE, GL_TRUE);
        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
        glPopAttrib();
        dirty = false;
    }
    ULONG refs;
    UINT width, height, pitch;
    D3DFORMAT format;
    bool backbuffer;
    LinuxTexture *owner;
    bool dirty;
    GLuint blitTexture;
    std::vector<BYTE> pixels;
};

class LinuxTexture : public IDirect3DTexture8
{
  public:
    LinuxTexture(UINT width, UINT height, D3DFORMAT format)
        : refs(1), priority(0), glName(0), uploaded(false), rgbaSource(false)
    { surface = new LinuxSurface(width, height, format, false, this); }
    ~LinuxTexture()
    {
        surface->owner = NULL; surface->Release();
        if (glName != 0) glDeleteTextures(1, &glName);
    }
    ULONG AddRef() { return ++refs; }
    ULONG Release() { ULONG value = --refs; if (value == 0) delete this; return value; }
    DWORD SetPriority(DWORD value) { DWORD old = priority; priority = value; return old; }
    void PreLoad() { Upload(); }
    HRESULT GetLevelDesc(UINT level, D3DSURFACE_DESC *description)
    { return level == 0 ? surface->GetDesc(description) : E_INVALIDARG; }
    HRESULT GetSurfaceLevel(UINT level, IDirect3DSurface8 **result)
    {
        if (level != 0 || result == NULL) return E_INVALIDARG;
        surface->AddRef(); *result = surface; return S_OK;
    }
    HRESULT LockRect(UINT level, D3DLOCKED_RECT *locked, const RECT *rect, DWORD flags)
    { return level == 0 ? surface->LockRect(locked, rect, flags) : E_INVALIDARG; }
    HRESULT UnlockRect(UINT level)
    { if (level != 0) return E_INVALIDARG; return surface->UnlockRect(); }
    void Upload()
    {
        if (uploaded && !surface->dirty) return;
        if (glName == 0) glGenTextures(1, &glName);
        // The legacy renderer can leave another texture unit active after a
        // 3-D background pass.  Textures are sampled from unit zero by the
        // GLES shader, so make the binding deterministic before uploading.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, glName);
        // A newly-created GLES texture defaults to a mipmap minification
        // filter.  Since the text atlas has no mip levels, that default makes
        // the sampler return (0, 0, 0, 1), which is seen as a solid black
        // rectangle.  Set complete, non-mipmap sampling at creation time;
        // PrepareState still applies the D3D filter requested by the game.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        std::vector<BYTE> rgba;
        const BYTE *uploadPixels = NULL;
        GLenum uploadFormat = GL_RGBA;
        if (rgbaSource && surface->format == D3DFMT_A8R8G8B8 &&
            surface->pitch == surface->width * 4)
        {
            // TH095IosRasterizeTextUtf8 writes byte-order RGBA directly.
            uploadPixels = surface->pixels.empty() ? NULL : &surface->pixels[0];
        }
        else
        {
            rgba.resize(surface->width * surface->height * 4);
            UINT bytes = BytesPerPixel(surface->format);
            for (UINT y = 0; y < surface->height; ++y)
                for (UINT x = 0; x < surface->width; ++x)
                    DecodePixel(&surface->pixels[y * surface->pitch + x * bytes], surface->format,
                                &rgba[(y * surface->width + x) * 4]);
            uploadPixels = rgba.empty() ? NULL : &rgba[0];
        }
        if (surface->format == D3DFMT_A8R8G8B8)
        {
            static int uploadDiagnostics;
            if (SDL_getenv("TH095_IOS_TEXT_DIAGNOSTICS") != NULL && uploadDiagnostics < 12)
            {
                unsigned int nonzeroAlpha = 0;
                unsigned int opaqueAlpha = 0;
                unsigned int nonzeroRed = 0;
                unsigned int nonzeroGreen = 0;
                unsigned int nonzeroBlue = 0;
                const BYTE *diagnosticPixels = uploadPixels;
                const size_t diagnosticSize =
                    static_cast<size_t>(surface->width) * surface->height * 4;
                for (size_t index = 0; diagnosticPixels != NULL &&
                                      index + 3 < diagnosticSize; index += 4)
                {
                    if (diagnosticPixels[index + 0] != 0) ++nonzeroRed;
                    if (diagnosticPixels[index + 1] != 0) ++nonzeroGreen;
                    if (diagnosticPixels[index + 2] != 0) ++nonzeroBlue;
                    if (diagnosticPixels[index + 3] != 0) ++nonzeroAlpha;
                    if (diagnosticPixels[index + 3] == 255) ++opaqueAlpha;
                }
                char message[224];
                SDL_snprintf(message, sizeof(message),
                             "text/upload: %ux%u rgba=%d rgb=%u/%u/%u alpha=%u opaque=%u",
                             surface->width, surface->height, rgbaSource ? 1 : 0,
                             nonzeroRed, nonzeroGreen, nonzeroBlue, nonzeroAlpha, opaqueAlpha);
                th095::modern::LogStartup(message);
                ++uploadDiagnostics;
            }
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (uploaded)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surface->width, surface->height,
                            uploadFormat, GL_UNSIGNED_BYTE, uploadPixels);
        else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->width, surface->height, 0,
                         uploadFormat, GL_UNSIGNED_BYTE, uploadPixels);
        uploaded = true; surface->dirty = false;
    }
    ULONG refs;
    DWORD priority;
    GLuint glName;
    bool uploaded;
    bool rgbaSource;
    LinuxSurface *surface;
};

class LinuxVertexBuffer : public IDirect3DVertexBuffer8
{
  public:
    explicit LinuxVertexBuffer(UINT size) : refs(1), bytes(size) {}
    ULONG AddRef() { return ++refs; }
    ULONG Release() { ULONG value = --refs; if (value == 0) delete this; return value; }
    HRESULT Lock(UINT offset, UINT size, BYTE **data, DWORD)
    {
        if (data == NULL || offset > bytes.size()) return E_INVALIDARG;
        if (size == 0) size = static_cast<UINT>(bytes.size() - offset);
        if (offset + size > bytes.size()) return E_INVALIDARG;
        *data = bytes.empty() ? NULL : &bytes[offset]; return S_OK;
    }
    HRESULT Unlock() { return S_OK; }
    ULONG refs;
    std::vector<BYTE> bytes;
};

GLenum PrimitiveMode(D3DPRIMITIVETYPE type)
{
    switch (type)
    {
    case D3DPT_POINTLIST: return GL_POINTS;
    case D3DPT_LINELIST: return GL_LINES;
    case D3DPT_LINESTRIP: return GL_LINE_STRIP;
    case D3DPT_TRIANGLESTRIP: return GL_TRIANGLE_STRIP;
    case D3DPT_TRIANGLEFAN: return GL_TRIANGLE_FAN;
    default: return GL_TRIANGLES;
    }
}

UINT VertexCount(D3DPRIMITIVETYPE type, UINT primitiveCount)
{
    if (type == D3DPT_POINTLIST) return primitiveCount;
    if (type == D3DPT_LINELIST) return primitiveCount * 2;
    if (type == D3DPT_LINESTRIP) return primitiveCount + 1;
    if (type == D3DPT_TRIANGLELIST) return primitiveCount * 3;
    return primitiveCount + 2;
}

GLenum CompareFunction(DWORD function)
{
    switch (function)
    {
    case D3DCMP_NEVER: return GL_NEVER;
    case D3DCMP_LESS: return GL_LESS;
    case D3DCMP_EQUAL: return GL_EQUAL;
    case D3DCMP_LESSEQUAL: return GL_LEQUAL;
    case D3DCMP_GREATER: return GL_GREATER;
    case D3DCMP_NOTEQUAL: return GL_NOTEQUAL;
    case D3DCMP_GREATEREQUAL: return GL_GEQUAL;
    default: return GL_ALWAYS;
    }
}

GLenum BlendFunction(DWORD function)
{
    switch (function)
    {
    case D3DBLEND_ZERO: return GL_ZERO;
    case D3DBLEND_ONE: return GL_ONE;
    case D3DBLEND_SRCCOLOR: return GL_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR: return GL_ONE_MINUS_SRC_COLOR;
    case D3DBLEND_SRCALPHA: return GL_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA: return GL_ONE_MINUS_SRC_ALPHA;
    case D3DBLEND_DESTALPHA: return GL_DST_ALPHA;
    case D3DBLEND_INVDESTALPHA: return GL_ONE_MINUS_DST_ALPHA;
    case D3DBLEND_DESTCOLOR: return GL_DST_COLOR;
    case D3DBLEND_INVDESTCOLOR: return GL_ONE_MINUS_DST_COLOR;
    case D3DBLEND_SRCALPHASAT: return GL_SRC_ALPHA_SATURATE;
    default: return GL_ONE;
    }
}

bool TextureOperationUsesTexture(DWORD operation, DWORD argument1, DWORD argument2)
{
    if (operation == D3DTOP_DISABLE) return false;
    if (operation == D3DTOP_SELECTARG1)
        return (argument1 & D3DTA_SELECTMASK) == D3DTA_TEXTURE;
    return (argument1 & D3DTA_SELECTMASK) == D3DTA_TEXTURE ||
           (argument2 & D3DTA_SELECTMASK) == D3DTA_TEXTURE;
}

DWORD TextureArgumentModifier(DWORD argument, DWORD diffuse, DWORD factor)
{
    switch (argument & D3DTA_SELECTMASK)
    {
    case D3DTA_TEXTURE: return 0xffffffffu;
    case D3DTA_TFACTOR: return factor;
    default: return diffuse;
    }
}

BYTE MultiplyColorComponent(BYTE left, BYTE right)
{
    return static_cast<BYTE>((static_cast<UINT>(left) * right + 127u) / 255u);
}

DWORD MultiplyColors(DWORD left, DWORD right)
{
    return
        (static_cast<DWORD>(MultiplyColorComponent((left >> 24) & 255, (right >> 24) & 255)) << 24) |
        (static_cast<DWORD>(MultiplyColorComponent((left >> 16) & 255, (right >> 16) & 255)) << 16) |
        (static_cast<DWORD>(MultiplyColorComponent((left >> 8) & 255, (right >> 8) & 255)) << 8) |
        static_cast<DWORD>(MultiplyColorComponent(left & 255, right & 255));
}

DWORD TextureOperationModifier(DWORD operation, DWORD argument1, DWORD argument2,
                               DWORD diffuse, DWORD factor)
{
    const DWORD first = TextureArgumentModifier(argument1, diffuse, factor);
    if (operation == D3DTOP_SELECTARG1)
        return first;
    if (operation == D3DTOP_MODULATE)
        return MultiplyColors(first, TextureArgumentModifier(argument2, diffuse, factor));
    return diffuse;
}

GLenum TextureArgumentSource(DWORD argument)
{
    switch (argument & D3DTA_SELECTMASK)
    {
    case D3DTA_TEXTURE: return GL_TEXTURE;
    case D3DTA_TFACTOR: return GL_CONSTANT;
    default: return GL_PRIMARY_COLOR;
    }
}

void ConfigureTextureComponent(GLenum combineParameter, GLenum source0Parameter,
                               GLenum source1Parameter, GLenum operand0Parameter,
                               GLenum operand1Parameter, DWORD operation,
                               DWORD argument1, DWORD argument2, GLenum operand)
{
    glTexEnvi(GL_TEXTURE_ENV, combineParameter,
              operation == D3DTOP_SELECTARG1 ? GL_REPLACE : GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, source0Parameter, TextureArgumentSource(argument1));
    glTexEnvi(GL_TEXTURE_ENV, operand0Parameter, operand);
    glTexEnvi(GL_TEXTURE_ENV, source1Parameter, TextureArgumentSource(argument2));
    glTexEnvi(GL_TEXTURE_ENV, operand1Parameter, operand);
}

class LinuxDevice : public IDirect3DDevice8
{
  public:
    LinuxDevice(SDL_Window *window_, const D3DPRESENT_PARAMETERS &parameters)
        : refs(1), window(window_), context(NULL), backbuffer(NULL), texture(NULL), vertexBuffer(NULL),
          fvf(0), streamStride(0), renderFramebuffer(0), renderColorTexture(0), renderDepthBuffer(0),
          dialogueSnapshotTexture(0), framebufferReady(false), dialogueSnapshotReady(false),
          wasDialogPresent(false), presentCount(0), defaultFramebuffer(0), defaultRenderbuffer(0)
    {
        memset(renderStates, 0, sizeof(renderStates)); memset(textureStates, 0, sizeof(textureStates));
        Identity(&world); Identity(&view); Identity(&projection); Identity(&textureTransform);
        context = SDL_GL_CreateContext(window);
        if (context == NULL) return;
        SDL_GL_MakeCurrent(window, context);
#ifdef TH095_IOS
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFramebuffer);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &defaultRenderbuffer);
        char framebufferMessage[128];
        snprintf(framebufferMessage, sizeof(framebufferMessage),
                 "gles2: SDL default framebuffer=%d renderbuffer=%d", defaultFramebuffer,
                 defaultRenderbuffer);
        th095::modern::LogStartup(framebufferMessage);
#endif
        g_fogCoordf = reinterpret_cast<FogCoordfFunction>(SDL_GL_GetProcAddress("glFogCoordf"));
        if (g_fogCoordf == NULL)
            g_fogCoordf = reinterpret_cast<FogCoordfFunction>(SDL_GL_GetProcAddress("glFogCoordfEXT"));
        const bool fastSmoke = SDL_getenv("TH095_IOS_SMOKE_FAST") != NULL;
        SDL_GL_SetSwapInterval(fastSmoke ? 0 :
            (parameters.FullScreen_PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE ? 0 : 1));
        framebufferReady = ResetInternal(parameters);
        renderStates[D3DRS_TEXTUREFACTOR] = 0xffffffffu;
        renderStates[D3DRS_SRCBLEND] = D3DBLEND_SRCALPHA;
        renderStates[D3DRS_DESTBLEND] = D3DBLEND_INVSRCALPHA;
        renderStates[D3DRS_ZWRITEENABLE] = TRUE;
        renderStates[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL; renderStates[D3DRS_ALPHAFUNC] = D3DCMP_ALWAYS;
        textureStates[D3DTSS_COLOROP] = D3DTOP_MODULATE;
        textureStates[D3DTSS_COLORARG1] = D3DTA_TEXTURE; textureStates[D3DTSS_COLORARG2] = D3DTA_DIFFUSE;
        textureStates[D3DTSS_ALPHAOP] = D3DTOP_MODULATE;
        textureStates[D3DTSS_ALPHAARG1] = D3DTA_TEXTURE; textureStates[D3DTSS_ALPHAARG2] = D3DTA_DIFFUSE;
        textureStates[D3DTSS_ADDRESSU] = D3DTADDRESS_WRAP; textureStates[D3DTSS_ADDRESSV] = D3DTADDRESS_WRAP;
        textureStates[D3DTSS_MINFILTER] = D3DTEXF_POINT; textureStates[D3DTSS_MAGFILTER] = D3DTEXF_POINT;
        glDisable(GL_CULL_FACE);
    }
    ~LinuxDevice()
    {
        if (context != NULL) SDL_GL_MakeCurrent(window, context);
        if (texture != NULL) texture->Release();
        if (vertexBuffer != NULL) vertexBuffer->Release();
        if (backbuffer != NULL) backbuffer->Release();
        DestroyRenderTarget();
        if (context != NULL) SDL_GL_DeleteContext(context);
    }
    bool Ready() const { return context != NULL && backbuffer != NULL && framebufferReady; }
    ULONG AddRef() { return ++refs; }
    ULONG Release() { ULONG value = --refs; if (value == 0) delete this; return value; }
    HRESULT TestCooperativeLevel() { return S_OK; }
    HRESULT Reset(D3DPRESENT_PARAMETERS *parameters)
    {
        if (parameters == NULL) return E_INVALIDARG;
        framebufferReady = ResetInternal(*parameters);
        return framebufferReady ? S_OK : E_FAIL;
    }
    HRESULT Present(const RECT *, const RECT *, HWND, const RGNDATA *)
    {
        backbuffer->FlushBackbuffer();
        presentCount++;

        int drawableWidth, drawableHeight;
        SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
        if (drawableWidth <= 0 || drawableHeight <= 0)
            return E_FAIL;

        int viewportX = 0;
        int viewportY = 0;
        int viewportWidth = drawableWidth;
        int viewportHeight = drawableHeight;
        const bool portraitGameplay = th095::modern::ios::UsePortraitBattle() && drawableHeight > drawableWidth &&
            th095::g_RuntimePlayerOwner != NULL && th095::g_RuntimeGlobalStateOwner != NULL;
        if (static_cast<long long>(drawableWidth) * 480LL >
            static_cast<long long>(drawableHeight) * 640LL)
        {
            viewportHeight = drawableHeight;
            viewportWidth = drawableHeight * 640 / 480;
            viewportX = (drawableWidth - viewportWidth) / 2;
        }
        else if (static_cast<long long>(drawableWidth) * 480LL <
                 static_cast<long long>(drawableHeight) * 640LL)
        {
            viewportWidth = drawableWidth;
            viewportHeight = drawableWidth * 480 / 640;
            viewportY = (drawableHeight - viewportHeight) / 2;
        }
#ifdef TH095_IOS
        if (drawableHeight > drawableWidth)
        {
            const int sourceWidth = portraitGameplay ? 384 : 640;
            const int sourceHeight = portraitGameplay ? 448 : 480;
            viewportX = 0;
            viewportWidth = drawableWidth;
            viewportHeight = drawableWidth * sourceHeight / sourceWidth;
            // Reserve the same lower control deck on both phones and iPads.
            // A percentage alone lets the taller S button overlap on 4:3 iPads.
            const float controlScale = th095::modern::ios::ControlScale(drawableWidth,drawableHeight);
            const int topInset = static_cast<int>(82 * controlScale);
            const int controlDeck = static_cast<int>(204 * controlScale);
            const int maximumHeight = std::max(1, drawableHeight - topInset - controlDeck);
            if (viewportHeight > maximumHeight)
            {
                viewportHeight = maximumHeight;
                viewportWidth = viewportHeight * sourceWidth / sourceHeight;
            }
            viewportX = (drawableWidth - viewportWidth) / 2;
            viewportY = topInset + (maximumHeight - viewportHeight) / 2;
        }
        th095::modern::ios::PresentationLayout layout = {
            drawableWidth, drawableHeight, viewportX, viewportY,
            viewportWidth, viewportHeight};
        th095::modern::ios::SetPresentationLayout(layout);
        if (presentCount == 1 || presentCount % 300 == 0)
        {
            char presentationMessage[224];
            SDL_snprintf(presentationMessage, sizeof(presentationMessage),
                         "ios: present mode=%s drawable=%dx%d viewport=%d,%d %dx%d",
                         portraitGameplay ? "portrait-gameplay" : "whole-frame",
                         drawableWidth, drawableHeight, viewportX, viewportY,
                         viewportWidth, viewportHeight);
            th095::modern::LogStartup(presentationMessage);
        }
#endif
        g_framebufferApi.bindFramebuffer(GL_FRAMEBUFFER,
#ifdef TH095_IOS
                                         static_cast<GLuint>(defaultFramebuffer)
#else
                                         0
#endif
        );
        glDrawBuffer(GL_BACK);
        glViewport(0, 0, drawableWidth, drawableHeight);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST); glDisable(GL_SCISSOR_TEST);
        glDepthMask(GL_FALSE);
        glBindTexture(GL_TEXTURE_2D, renderColorTexture);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glOrtho(0.0, drawableWidth, drawableHeight, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
        glColor4ub(255, 255, 255, 255);
#ifdef TH095_IOS
        f32 sourceShakeX = 0.0f;
        f32 sourceShakeY = 0.0f;
        th095::modern::ios::GetPresentationShake(&sourceShakeX, &sourceShakeY);
        const float presentationScale =
            drawableWidth / 640.0f < drawableHeight / 480.0f
                ? drawableWidth / 640.0f
                : drawableHeight / 480.0f;
        const int presentationShakeX = static_cast<int>(sourceShakeX * presentationScale +
                                                        (sourceShakeX >= 0.0f ? 0.5f : -0.5f));
        const int presentationShakeY = static_cast<int>(sourceShakeY * presentationScale +
                                                        (sourceShakeY >= 0.0f ? 0.5f : -0.5f));
#else
        const int presentationShakeX = 0;
        const int presentationShakeY = 0;
#endif
        if (portraitGameplay)
        {
            // TH095's centered playfield includes its score/photo HUD.
            // Keep its aspect ratio; reserve the lower margin for fingers.
            DrawPresentationRegion(128, 16, 384, 448,
                                   viewportX + presentationShakeX,
                                   viewportY + presentationShakeY,
                                   viewportWidth, viewportHeight);
        }
        else
        {
            DrawPresentationRegion(0, 0, 640, 480,
                                   viewportX + presentationShakeX,
                                   viewportY + presentationShakeY,
                                   viewportWidth, viewportHeight);
        }
        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
        glPopAttrib();

#ifdef TH095_IOS
        th095::modern::ios::DrawVirtualControls();
#endif
        glFlush();
#ifdef TH095_IOS
        glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(defaultRenderbuffer));
#endif
        SDL_GL_SwapWindow(window);

        g_framebufferApi.bindFramebuffer(GL_FRAMEBUFFER, renderFramebuffer);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0, 0, backbuffer->width, backbuffer->height);
        return S_OK;
    }
    HRESULT GetBackBuffer(UINT index, D3DBACKBUFFER_TYPE, IDirect3DSurface8 **result)
    {
        if (index != 0 || result == NULL || backbuffer == NULL) return E_INVALIDARG;
        backbuffer->AddRef(); *result = backbuffer; return S_OK;
    }
    HRESULT CreateTexture(UINT width, UINT height, UINT, DWORD, D3DFORMAT format, D3DPOOL,
                          IDirect3DTexture8 **result)
    {
        if (result == NULL || width == 0 || height == 0) return E_INVALIDARG;
        if (format == D3DFMT_UNKNOWN) format = D3DFMT_A8R8G8B8;
        *result = new(std::nothrow) LinuxTexture(width, height, format);
        return *result != NULL ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT CreateVertexBuffer(UINT size, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer8 **result)
    {
        if (result == NULL) return E_INVALIDARG;
        *result = new(std::nothrow) LinuxVertexBuffer(size); return *result != NULL ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT CreateRenderTarget(UINT width, UINT height, D3DFORMAT format, D3DMULTISAMPLE_TYPE, BOOL,
                               IDirect3DSurface8 **result)
    { return CreateSurface(width, height, format, result); }
    HRESULT CreateImageSurface(UINT width, UINT height, D3DFORMAT format, IDirect3DSurface8 **result)
    { return CreateSurface(width, height, format, result); }
    HRESULT CopyRects(IDirect3DSurface8 *sourceRaw, const RECT *sourceRects, UINT count,
                      IDirect3DSurface8 *destinationRaw, const POINT *destinationPoints)
    {
        LinuxSurfaceAccess source, destination;
        if (!TH095_linux_surface_access(sourceRaw, &source, true) ||
            !TH095_linux_surface_access(destinationRaw, &destination, false)) return E_INVALIDARG;
        if (source.format != destination.format) return E_NOTIMPL;
        if (count == 0) count = 1;
        UINT bytes = BytesPerPixel(source.format);
        for (UINT index = 0; index < count; ++index)
        {
            RECT rect;
            if (sourceRects != NULL) rect = sourceRects[index];
            else { rect.left = 0; rect.top = 0; rect.right = source.width; rect.bottom = source.height; }
            POINT point; point.x = destinationPoints != NULL ? destinationPoints[index].x : 0;
            point.y = destinationPoints != NULL ? destinationPoints[index].y : 0;
            UINT copyWidth = rect.right > rect.left ? rect.right - rect.left : 0;
            UINT copyHeight = rect.bottom > rect.top ? rect.bottom - rect.top : 0;
            if (point.x < 0 || point.y < 0 || rect.left < 0 || rect.top < 0) continue;
            if (static_cast<UINT>(point.x) + copyWidth > destination.width) copyWidth = destination.width - point.x;
            if (static_cast<UINT>(point.y) + copyHeight > destination.height) copyHeight = destination.height - point.y;
            for (UINT y = 0; y < copyHeight; ++y)
                memcpy(destination.pixels + (point.y + y) * destination.pitch + point.x * bytes,
                       source.pixels + (rect.top + y) * source.pitch + rect.left * bytes, copyWidth * bytes);
        }
        TH095_linux_surface_changed(destinationRaw);
        return S_OK;
    }
    HRESULT BeginScene()
    {
#ifdef TH095_IOS
        const bool dialogPresent = th095::g_RuntimeGlobalStateOwner &&
            static_cast<th095::PhotoGameTaskView *>(th095::g_RuntimeGlobalStateOwner)->resultScreenActive;
        g_framebufferApi.bindFramebuffer(GL_FRAMEBUFFER, renderFramebuffer);
        if (dialogPresent && !wasDialogPresent) CaptureDialogueSnapshot();
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glClearColor(0, 0, 0, 1);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (dialogPresent && dialogueSnapshotReady) RestoreDialogueSnapshot();
        glDepthMask(renderStates[D3DRS_ZWRITEENABLE] ? GL_TRUE : GL_FALSE);
#else
        const bool dialogPresent = th095::g_Gui.IsDialoguePresent() != 0;
        if (dialogPresent && !wasDialogPresent) CaptureDialogueSnapshot();
        if (dialogPresent && dialogueSnapshotReady) RestoreDialogueSnapshot();
#endif
#ifdef TH095_IOS
        if (dialogPresent != wasDialogPresent)
            th095::modern::LogStartup(dialogPresent
                                         ? "render/pause: frozen frame captured"
                                         : "render/pause: normal redraw restored");
#endif
        if (!dialogPresent) dialogueSnapshotReady = false;
        wasDialogPresent = dialogPresent;
        return S_OK;
    }
    HRESULT EndScene() { return S_OK; }
    HRESULT Clear(DWORD, const D3DRECT *, DWORD flags, D3DCOLOR color, float depth, DWORD)
    {
        if ((flags & D3DCLEAR_TARGET) && getenv("TH095_LINUX_RENDER_TRACE") != NULL)
        {
            FILE *trace = fopen("modern-render.txt", "ab");
            if (trace != NULL)
            {
                fprintf(trace,
                        "frame=%lu flags=%08lx color=%08lx viewport=%lu,%lu,%lu,%lu caller=%p\n",
                        presentCount, static_cast<unsigned long>(flags), static_cast<unsigned long>(color),
                        static_cast<unsigned long>(viewport.X), static_cast<unsigned long>(viewport.Y),
                        static_cast<unsigned long>(viewport.Width), static_cast<unsigned long>(viewport.Height),
                        __builtin_return_address(0));
                fclose(trace);
            }
        }

        GLbitfield mask = 0;
        if (flags & D3DCLEAR_TARGET)
        {
            glClearColor(((color >> 16) & 255) / 255.0f, ((color >> 8) & 255) / 255.0f,
                         (color & 255) / 255.0f, ((color >> 24) & 255) / 255.0f);
            mask |= GL_COLOR_BUFFER_BIT;
        }
        if (flags & D3DCLEAR_ZBUFFER) { glClearDepth(depth); mask |= GL_DEPTH_BUFFER_BIT; }
        if (flags & D3DCLEAR_STENCIL) mask |= GL_STENCIL_BUFFER_BIT;
        const int targetWidth = backbuffer != NULL ? backbuffer->width : viewport.Width;
        const int targetHeight = backbuffer != NULL ? backbuffer->height : viewport.Height;
        const int left = static_cast<int>(viewport.X);
        const int bottom = targetHeight - static_cast<int>(viewport.Y + viewport.Height);
        const int width = static_cast<int>(viewport.Width);
        const int height = static_cast<int>(viewport.Height);
        glViewport(0, 0, targetWidth, targetHeight);
        glEnable(GL_SCISSOR_TEST);
        glScissor(left, bottom, width, height);
        if (flags & D3DCLEAR_ZBUFFER) glDepthMask(GL_TRUE);
        glClear(mask);
        if (flags & D3DCLEAR_ZBUFFER)
            glDepthMask(renderStates[D3DRS_ZWRITEENABLE] ? GL_TRUE : GL_FALSE);
        glDisable(GL_SCISSOR_TEST);
        return S_OK;
    }
    HRESULT SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX *matrix)
    {
        if (matrix == NULL) return E_INVALIDARG;
        if (state == D3DTS_WORLD) world = *matrix;
        else if (state == D3DTS_VIEW) view = *matrix;
        else if (state == D3DTS_PROJECTION) projection = *matrix;
        else if (state == D3DTS_TEXTURE0) textureTransform = *matrix;
        return S_OK;
    }
    HRESULT SetViewport(const D3DVIEWPORT8 *value)
    { if (value == NULL) return E_INVALIDARG; viewport = *value; return S_OK; }
    HRESULT GetViewport(D3DVIEWPORT8 *value)
    { if (value == NULL) return E_INVALIDARG; *value = viewport; return S_OK; }
    HRESULT SetRenderState(D3DRENDERSTATETYPE state, DWORD value)
    { if (static_cast<UINT>(state) < 256) renderStates[state] = value; return S_OK; }
    HRESULT SetTexture(DWORD stage, IDirect3DTexture8 *value)
    {
        if (stage != 0) return S_OK;
        LinuxTexture *next = static_cast<LinuxTexture *>(value);
        if (next != NULL) next->AddRef();
        if (texture != NULL) texture->Release(); texture = next; return S_OK;
    }
    HRESULT SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE state, DWORD value)
    { if (stage == 0 && static_cast<UINT>(state) < 32) textureStates[state] = value; return S_OK; }
    HRESULT SetVertexShader(DWORD value) { fvf = value; return S_OK; }
    HRESULT SetStreamSource(UINT stream, IDirect3DVertexBuffer8 *value, UINT stride)
    {
        if (stream != 0) return E_INVALIDARG;
        LinuxVertexBuffer *next = static_cast<LinuxVertexBuffer *>(value);
        if (next != NULL) next->AddRef();
        if (vertexBuffer != NULL) vertexBuffer->Release();
        vertexBuffer = next; streamStride = stride; return S_OK;
    }
    HRESULT DrawPrimitive(D3DPRIMITIVETYPE type, UINT startVertex, UINT primitiveCount)
    {
        if (vertexBuffer == NULL || streamStride == 0) return E_FAIL;
        UINT offset = startVertex * streamStride, count = VertexCount(type, primitiveCount);
        if (offset + count * streamStride > vertexBuffer->bytes.size()) return E_INVALIDARG;
        return Draw(type, primitiveCount, &vertexBuffer->bytes[offset], streamStride);
    }
    HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE type, UINT primitiveCount, const void *vertices, UINT stride)
    { return vertices != NULL ? Draw(type, primitiveCount, static_cast<const BYTE *>(vertices), stride) : E_INVALIDARG; }
    HRESULT GetDeviceCaps(D3DCAPS8 *caps)
    {
        if (caps == NULL) return E_INVALIDARG;
        memset(caps, 0, sizeof(*caps)); caps->DeviceType = D3DDEVTYPE_HAL;
        caps->Caps2 = D3DCAPS2_CANRENDERWINDOWED;
        caps->PresentationIntervals = D3DPRESENT_INTERVAL_ONE | D3DPRESENT_INTERVAL_IMMEDIATE;
        caps->DevCaps = D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_HWRASTERIZATION |
                        D3DDEVCAPS_TEXTURESYSTEMMEMORY | D3DDEVCAPS_TEXTUREVIDEOMEMORY |
                        D3DDEVCAPS_TLVERTEXSYSTEMMEMORY | D3DDEVCAPS_TLVERTEXVIDEOMEMORY;
        caps->MaxTextureWidth = caps->MaxTextureHeight = 4096;
        caps->MaxTextureBlendStages = 1; caps->MaxSimultaneousTextures = 1;
        caps->MaxPrimitiveCount = 0x100000; caps->MaxStreams = 1; caps->MaxStreamStride = 256;
        caps->TextureOpCaps = D3DTEXOPCAPS_ADD | D3DTEXOPCAPS_MODULATE | D3DTEXOPCAPS_SELECTARG1;
        return S_OK;
    }
    HRESULT ResourceManagerDiscardBytes(DWORD) { return S_OK; }

    void DrawScreenQuad(const void *vertices, UINT stride)
    {
        // Screen-space HUD/fade quads must not inherit a laser's blend,
        // stage depth/fog, or a texture's alpha-test state.
        DWORD savedRender[256], savedTexture[32];
        memcpy(savedRender, renderStates, sizeof(savedRender));
        memcpy(savedTexture, textureStates, sizeof(savedTexture));
        const DWORD savedFvf = fvf;
        renderStates[D3DRS_ZENABLE] = FALSE;
        renderStates[D3DRS_ZWRITEENABLE] = FALSE;
        renderStates[D3DRS_FOGENABLE] = FALSE;
        renderStates[D3DRS_ALPHATESTENABLE] = FALSE;
        renderStates[D3DRS_ALPHABLENDENABLE] = TRUE;
        renderStates[D3DRS_SRCBLEND] = D3DBLEND_SRCALPHA;
        renderStates[D3DRS_DESTBLEND] = D3DBLEND_INVSRCALPHA;
        textureStates[D3DTSS_COLOROP] = textureStates[D3DTSS_ALPHAOP] = D3DTOP_SELECTARG1;
        textureStates[D3DTSS_COLORARG1] = textureStates[D3DTSS_ALPHAARG1] = D3DTA_DIFFUSE;
        fvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
        Draw(D3DPT_TRIANGLESTRIP, 2, static_cast<const BYTE *>(vertices), stride);
        fvf = savedFvf;
        memcpy(renderStates, savedRender, sizeof(savedRender));
        memcpy(textureStates, savedTexture, sizeof(savedTexture));
    }

  private:
    HRESULT CreateSurface(UINT width, UINT height, D3DFORMAT format, IDirect3DSurface8 **result)
    {
        if (result == NULL || width == 0 || height == 0) return E_INVALIDARG;
        if (format == D3DFMT_UNKNOWN) format = D3DFMT_A8R8G8B8;
        *result = new(std::nothrow) LinuxSurface(width, height, format, false, NULL);
        return *result != NULL ? S_OK : E_OUTOFMEMORY;
    }
    void DestroyRenderTarget()
    {
        if (dialogueSnapshotTexture != 0)
            glDeleteTextures(1, &dialogueSnapshotTexture);
        if (renderDepthBuffer != 0 && g_framebufferApi.deleteRenderbuffers != NULL)
            g_framebufferApi.deleteRenderbuffers(1, &renderDepthBuffer);
        if (renderFramebuffer != 0 && g_framebufferApi.deleteFramebuffers != NULL)
            g_framebufferApi.deleteFramebuffers(1, &renderFramebuffer);
        if (renderColorTexture != 0)
            glDeleteTextures(1, &renderColorTexture);
        renderDepthBuffer = renderFramebuffer = renderColorTexture = dialogueSnapshotTexture = 0;
        dialogueSnapshotReady = false;
        wasDialogPresent = false;
    }
    bool CreateRenderTarget(UINT width, UINT height)
    {
        if (!g_framebufferApi.Initialize())
        {
            fprintf(stderr, "TH095-modern: OpenGL framebuffer objects are unavailable\n");
            return false;
        }

        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &renderColorTexture);
        glBindTexture(GL_TEXTURE_2D, renderColorTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        glGenTextures(1, &dialogueSnapshotTexture);
        glBindTexture(GL_TEXTURE_2D, dialogueSnapshotTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        g_framebufferApi.genRenderbuffers(1, &renderDepthBuffer);
        g_framebufferApi.bindRenderbuffer(GL_RENDERBUFFER, renderDepthBuffer);
        // Depth24 is optional on GLES2.  iOS guarantees the 16-bit OES depth
        // format, which is sufficient for the original D3D8 depth range.
        g_framebufferApi.renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);

        g_framebufferApi.genFramebuffers(1, &renderFramebuffer);
        g_framebufferApi.bindFramebuffer(GL_FRAMEBUFFER, renderFramebuffer);
        g_framebufferApi.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                              GL_TEXTURE_2D, renderColorTexture, 0);
        g_framebufferApi.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                                 GL_RENDERBUFFER, renderDepthBuffer);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        GLenum status = g_framebufferApi.checkFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            fprintf(stderr, "TH095-modern: unable to create OpenGL framebuffer (status 0x%04x)\n",
                    static_cast<unsigned int>(status));
            DestroyRenderTarget();
            return false;
        }
        return true;
    }
    void CaptureDialogueSnapshot()
    {
        if (dialogueSnapshotTexture == 0 || backbuffer == NULL)
            return;
        glBindTexture(GL_TEXTURE_2D, dialogueSnapshotTexture);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, backbuffer->width, backbuffer->height);
        dialogueSnapshotReady = true;
    }
    void RestoreDialogueSnapshot()
    {
        const UINT width = backbuffer->width;
        const UINT height = backbuffer->height;
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST); glDisable(GL_SCISSOR_TEST);
        glDepthMask(GL_FALSE);
        glViewport(0, 0, width, height);
#ifdef TH095_IOS
        IosLegacySetTextureUsage(GL_TRUE, GL_FALSE);
        IosLegacySetFog(GL_FALSE, 0, 0, 0, 0, 1);
        IosLegacySetAlphaThreshold(-1);
        IosLegacySetForceAlphaDiscard(GL_FALSE);
#endif
        glBindTexture(GL_TEXTURE_2D, dialogueSnapshotTexture);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
        glColor4ub(255, 255, 255, 255);
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(static_cast<float>(width), 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, static_cast<float>(height));
        glTexCoord2f(1.0f, 0.0f); glVertex2f(static_cast<float>(width), static_cast<float>(height));
        glEnd();
        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
        glPopAttrib();
    }
    bool ResetInternal(const D3DPRESENT_PARAMETERS &parameters)
    {
        UINT width = parameters.BackBufferWidth != 0 ? parameters.BackBufferWidth : 640;
        UINT height = parameters.BackBufferHeight != 0 ? parameters.BackBufferHeight : 480;
        D3DFORMAT format = parameters.BackBufferFormat;
        if (format == D3DFMT_UNKNOWN) format = D3DFMT_X8R8G8B8;
        DestroyRenderTarget();
        if (backbuffer != NULL) backbuffer->Release();
        backbuffer = new LinuxSurface(width, height, format, true, NULL);
        if (backbuffer == NULL || !CreateRenderTarget(width, height)) return false;
        viewport.X = viewport.Y = 0; viewport.Width = width; viewport.Height = height;
        viewport.MinZ = 0.0f; viewport.MaxZ = 1.0f;
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0);
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return true;
    }
    void TransformToClip(const float *position, IosClipVertex *vertex)
    {
        float vector[4] = {position[0], position[1], position[2], 1.0f};
        const D3DMATRIX *matrices[3] = {&world, &view, &projection};
        vertex->fogCoordinate = 0.0f;
        for (int index = 0; index < 3; ++index)
        {
            const D3DMATRIX &matrix = *matrices[index];
            float next[4];
            next[0] = vector[0] * matrix._11 + vector[1] * matrix._21 +
                      vector[2] * matrix._31 + vector[3] * matrix._41;
            next[1] = vector[0] * matrix._12 + vector[1] * matrix._22 +
                      vector[2] * matrix._32 + vector[3] * matrix._42;
            next[2] = vector[0] * matrix._13 + vector[1] * matrix._23 +
                      vector[2] * matrix._33 + vector[3] * matrix._43;
            next[3] = vector[0] * matrix._14 + vector[1] * matrix._24 +
                      vector[2] * matrix._34 + vector[3] * matrix._44;
            memcpy(vector, next, sizeof(vector));
            if (index == 1)
                vertex->fogCoordinate = fabsf(vector[2]);
        }
        vertex->x = vector[0];
        vertex->y = vector[1];
        vertex->z = vector[2];
        vertex->w = vector[3];
    }

    float ClipDistance(const IosClipVertex &vertex, int plane)
    {
        switch (plane)
        {
        case 0: return vertex.w - 1.0e-5f;
        case 1: return vertex.x + vertex.w;
        case 2: return vertex.w - vertex.x;
        case 3: return vertex.y + vertex.w;
        case 4: return vertex.w - vertex.y;
        case 5: return vertex.z;
        default: return vertex.w - vertex.z;
        }
    }

    IosClipVertex InterpolateClipVertex(const IosClipVertex &from,
                                        const IosClipVertex &to, float amount)
    {
        IosClipVertex result;
        result.x = from.x + (to.x - from.x) * amount;
        result.y = from.y + (to.y - from.y) * amount;
        result.z = from.z + (to.z - from.z) * amount;
        result.w = from.w + (to.w - from.w) * amount;
        result.u = from.u + (to.u - from.u) * amount;
        result.v = from.v + (to.v - from.v) * amount;
        result.red = from.red + (to.red - from.red) * amount;
        result.green = from.green + (to.green - from.green) * amount;
        result.blue = from.blue + (to.blue - from.blue) * amount;
        result.alpha = from.alpha + (to.alpha - from.alpha) * amount;
        result.fogCoordinate = from.fogCoordinate +
                               (to.fogCoordinate - from.fogCoordinate) * amount;
        return result;
    }

    int ClipTriangle(const IosClipVertex *triangle, IosClipVertex *clipped)
    {
        IosClipVertex buffers[2][24];
        memcpy(buffers[0], triangle, sizeof(IosClipVertex) * 3);
        int inputIndex = 0;
        int count = 3;
        for (int plane = 0; plane < 7 && count > 0; ++plane)
        {
            IosClipVertex *input = buffers[inputIndex];
            IosClipVertex *output = buffers[1 - inputIndex];
            int outputCount = 0;
            IosClipVertex previous = input[count - 1];
            float previousDistance = ClipDistance(previous, plane);
            bool previousInside = previousDistance >= 0.0f;
            for (int index = 0; index < count; ++index)
            {
                const IosClipVertex current = input[index];
                const float currentDistance = ClipDistance(current, plane);
                const bool currentInside = currentDistance >= 0.0f;
                if (currentInside != previousInside)
                {
                    const float denominator = previousDistance - currentDistance;
                    const float amount = fabsf(denominator) > 1.0e-12f
                                             ? previousDistance / denominator
                                             : 0.0f;
                    if (outputCount < 24)
                        output[outputCount++] =
                            InterpolateClipVertex(previous, current, amount);
                }
                if (currentInside && outputCount < 24)
                    output[outputCount++] = current;
                previous = current;
                previousDistance = currentDistance;
                previousInside = currentInside;
            }
            count = outputCount;
            inputIndex = 1 - inputIndex;
        }
        if (count > 0)
            memcpy(clipped, buffers[inputIndex], sizeof(IosClipVertex) * count);
        return count;
    }

    bool ReadClipVertex(const BYTE *data, UINT colorOffset, UINT textureOffset,
                        bool hasDiffuse, bool hasTexture, IosClipVertex *vertex)
    {
        TransformToClip(reinterpret_cast<const float *>(data), vertex);
        D3DCOLOR color = hasDiffuse
                             ? *reinterpret_cast<const D3DCOLOR *>(data + colorOffset)
                             : 0xffffffffu;
        color = EffectiveColor(color);
        vertex->red = ((color >> 16) & 255) / 255.0f;
        vertex->green = ((color >> 8) & 255) / 255.0f;
        vertex->blue = (color & 255) / 255.0f;
        vertex->alpha = ((color >> 24) & 255) / 255.0f;
        vertex->u = vertex->v = 0.0f;
        if (hasTexture)
        {
            const float *uv = reinterpret_cast<const float *>(data + textureOffset);
            vertex->u = uv[0] * textureTransform._11 + uv[1] * textureTransform._21 +
                        textureTransform._31;
            vertex->v = uv[0] * textureTransform._12 + uv[1] * textureTransform._22 +
                        textureTransform._32;
        }
        return isfinite(vertex->x) && isfinite(vertex->y) &&
               isfinite(vertex->z) && isfinite(vertex->w) &&
               isfinite(vertex->u) && isfinite(vertex->v);
    }

    GLubyte ColorByte(float value)
    {
        if (value <= 0.0f) return 0;
        if (value >= 1.0f) return 255;
        return static_cast<GLubyte>(value * 255.0f + 0.5f);
    }

    void EmitClipVertex(const IosClipVertex &vertex, bool hasTexture)
    {
        if (hasTexture)
            glTexCoord2f(vertex.u, vertex.v);
        glColor4ub(ColorByte(vertex.red), ColorByte(vertex.green),
                   ColorByte(vertex.blue), ColorByte(vertex.alpha));
        IosLegacySetFogCoordinate(vertex.fogCoordinate);
        const float targetWidth = static_cast<float>(
            backbuffer != NULL ? backbuffer->width : viewport.Width);
        const float targetHeight = static_cast<float>(
            backbuffer != NULL ? backbuffer->height : viewport.Height);

        // Preserve homogeneous W through rasterization. The old path divided
        // by W here and submitted a screen-space vertex, which forced GLES to
        // interpolate UVs affinely. Long Stage 5/Final floors then warped as
        // the camera advanced. Map the D3D viewport into the full render
        // target in clip space, but leave the final divide to the GPU.
        const float scaleX = viewport.Width / targetWidth;
        const float scaleY = viewport.Height / targetHeight;
        const float offsetX =
            (2.0f * viewport.X + viewport.Width) / targetWidth - 1.0f;
        const float offsetY =
            1.0f - (2.0f * viewport.Y + viewport.Height) / targetHeight;
        const float x = vertex.x * scaleX + vertex.w * offsetX;
        const float y = vertex.y * scaleY + vertex.w * offsetY;
        const float z =
            2.0f * vertex.z * (viewport.MaxZ - viewport.MinZ) +
            vertex.w * (2.0f * viewport.MinZ - 1.0f);
        IosLegacyClipVertex4f(x, y, z, vertex.w);
    }

    HRESULT DrawClippedTriangles(D3DPRIMITIVETYPE type, UINT primitiveCount,
                                 const BYTE *data, UINT stride, UINT colorOffset,
                                 UINT textureOffset, bool hasDiffuse, bool hasTexture)
    {
        UINT clippedPrimitiveCount = 0;
        UINT rejectedPrimitiveCount = 0;
        glBegin(GL_TRIANGLES);
        for (UINT primitive = 0; primitive < primitiveCount; ++primitive)
        {
            UINT indices[3];
            if (type == D3DPT_TRIANGLELIST)
            {
                indices[0] = primitive * 3;
                indices[1] = primitive * 3 + 1;
                indices[2] = primitive * 3 + 2;
            }
            else if (type == D3DPT_TRIANGLESTRIP)
            {
                indices[0] = primitive;
                indices[1] = primitive + 1;
                indices[2] = primitive + 2;
            }
            else
            {
                indices[0] = 0;
                indices[1] = primitive + 1;
                indices[2] = primitive + 2;
            }

            IosClipVertex triangle[3];
            bool valid = true;
            for (int corner = 0; corner < 3; ++corner)
                valid = ReadClipVertex(data + indices[corner] * stride,
                                       colorOffset, textureOffset, hasDiffuse,
                                       hasTexture, &triangle[corner]) && valid;
            if (!valid)
            {
                ++rejectedPrimitiveCount;
                continue;
            }

            IosClipVertex polygon[24];
            const int count = ClipTriangle(triangle, polygon);
            if (count < 3)
            {
                ++rejectedPrimitiveCount;
                continue;
            }
            if (count != 3)
                ++clippedPrimitiveCount;
            for (int index = 1; index + 1 < count; ++index)
            {
                EmitClipVertex(polygon[0], hasTexture);
                EmitClipVertex(polygon[index], hasTexture);
                EmitClipVertex(polygon[index + 1], hasTexture);
            }
        }
        glEnd();

        static UINT diagnosticCount;
        if ((clippedPrimitiveCount != 0 || rejectedPrimitiveCount != 0) &&
            diagnosticCount < 24)
        {
            char message[160];
            SDL_snprintf(message, sizeof(message),
                         "ios/clip: type=%u primitives=%u clipped=%u rejected=%u",
                         static_cast<unsigned>(type), primitiveCount,
                         clippedPrimitiveCount, rejectedPrimitiveCount);
            th095::modern::LogStartup(message);
            ++diagnosticCount;
        }
        return S_OK;
    }

    void TransformPosition(const float *position, bool transformed, float *xOut, float *yOut,
                           float *zOut, float *fogCoordinateOut)
    {
        if (transformed)
        {
            // D3D8 pre-transformed vertices use integer pixel centers, while
            // OpenGL samples at half-integer centers.
            *xOut = position[0] + 0.5f; *yOut = position[1] + 0.5f; *zOut = position[2];
            *fogCoordinateOut = 0.0f;
            return;
        }
        float vector[4] = {position[0], position[1], position[2], 1.0f};
        const D3DMATRIX *matrices[3] = {&world, &view, &projection};
        for (int index = 0; index < 3; ++index)
        {
            const D3DMATRIX &m = *matrices[index]; float next[4];
            next[0] = vector[0] * m._11 + vector[1] * m._21 + vector[2] * m._31 + vector[3] * m._41;
            next[1] = vector[0] * m._12 + vector[1] * m._22 + vector[2] * m._32 + vector[3] * m._42;
            next[2] = vector[0] * m._13 + vector[1] * m._23 + vector[2] * m._33 + vector[3] * m._43;
            next[3] = vector[0] * m._14 + vector[1] * m._24 + vector[2] * m._34 + vector[3] * m._44;
            memcpy(vector, next, sizeof(vector));
            if (index == 1)
                *fogCoordinateOut = fabsf(vector[2]);
        }
        float reciprocal = fabsf(vector[3]) > 1.0e-8f ? 1.0f / vector[3] : 1.0f;
        *xOut = viewport.X + (vector[0] * reciprocal + 1.0f) * viewport.Width * 0.5f;
        *yOut = viewport.Y + (1.0f - vector[1] * reciprocal) * viewport.Height * 0.5f;
        *zOut = viewport.MinZ + vector[2] * reciprocal * (viewport.MaxZ - viewport.MinZ);
    }
    void PrepareState()
    {
        const UINT width = backbuffer != NULL ? backbuffer->width : viewport.Width;
        const UINT height = backbuffer != NULL ? backbuffer->height : viewport.Height;
        const int drawableWidth = width;
        const int drawableHeight = height;
        glEnable(GL_SCISSOR_TEST);
        glScissor(static_cast<int>(viewport.X * drawableWidth / width),
                  drawableHeight - static_cast<int>((viewport.Y + viewport.Height) * drawableHeight / height),
                  static_cast<int>(viewport.Width * drawableWidth / width),
                  static_cast<int>(viewport.Height * drawableHeight / height));
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        if (renderStates[D3DRS_ALPHABLENDENABLE])
        { glEnable(GL_BLEND); glBlendFunc(BlendFunction(renderStates[D3DRS_SRCBLEND]), BlendFunction(renderStates[D3DRS_DESTBLEND])); }
        else glDisable(GL_BLEND);
        GLfloat alphaThreshold = -1.0f;
        if (renderStates[D3DRS_ALPHATESTENABLE])
        {
            if (renderStates[D3DRS_ALPHAFUNC] == D3DCMP_GREATEREQUAL)
                alphaThreshold = (renderStates[D3DRS_ALPHAREF] & 255) / 255.0f;
            else if (renderStates[D3DRS_ALPHAFUNC] == D3DCMP_NEVER)
                alphaThreshold = 2.0f;
        }
        IosLegacySetAlphaThreshold(alphaThreshold);
        if (renderStates[D3DRS_ZENABLE]) { glEnable(GL_DEPTH_TEST); glDepthFunc(CompareFunction(renderStates[D3DRS_ZFUNC])); }
        else glDisable(GL_DEPTH_TEST);
        glDepthMask(renderStates[D3DRS_ZWRITEENABLE] ? GL_TRUE : GL_FALSE);
        if (renderStates[D3DRS_FOGENABLE] &&
            renderStates[D3DRS_FOGVERTEXMODE] == D3DFOG_LINEAR)
        {
            const DWORD color = renderStates[D3DRS_FOGCOLOR];
            const GLfloat fogColor[4] = {
                ((color >> 16) & 255) / 255.0f,
                ((color >> 8) & 255) / 255.0f,
                (color & 255) / 255.0f,
                1.0f
            };
            GLfloat fogStart, fogEnd;
            memcpy(&fogStart, &renderStates[D3DRS_FOGSTART], sizeof(fogStart));
            memcpy(&fogEnd, &renderStates[D3DRS_FOGEND], sizeof(fogEnd));
            IosLegacySetFog(GL_TRUE, fogColor[0], fogColor[1], fogColor[2], fogStart, fogEnd);
        }
        else
        {
            IosLegacySetFog(GL_FALSE, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        }
        const bool colorUsesTexture = TextureOperationUsesTexture(
            textureStates[D3DTSS_COLOROP], textureStates[D3DTSS_COLORARG1],
            textureStates[D3DTSS_COLORARG2]);
        const bool alphaUsesTexture = TextureOperationUsesTexture(
            textureStates[D3DTSS_ALPHAOP], textureStates[D3DTSS_ALPHAARG1],
            textureStates[D3DTSS_ALPHAARG2]);
        static int textStateDiagnostics;
        if (SDL_getenv("TH095_IOS_TEXT_DIAGNOSTICS") != NULL && texture != NULL && texture->surface != NULL &&
            texture->surface->format == D3DFMT_A8R8G8B8 && textStateDiagnostics < 24)
        {
            char message[192];
            SDL_snprintf(message, sizeof(message),
                         "text/state: colorTex=%d alphaTex=%d blend=%u src=%u dst=%u tex=%u",
                         colorUsesTexture ? 1 : 0, alphaUsesTexture ? 1 : 0,
                         static_cast<unsigned>(renderStates[D3DRS_ALPHABLENDENABLE]),
                         static_cast<unsigned>(renderStates[D3DRS_SRCBLEND]),
                         static_cast<unsigned>(renderStates[D3DRS_DESTBLEND]),
                         static_cast<unsigned>(texture->surface->format));
            th095::modern::LogStartup(message);
            ++textStateDiagnostics;
        }
        IosLegacySetTextureUsage(colorUsesTexture ? GL_TRUE : GL_FALSE,
                                 alphaUsesTexture ? GL_TRUE : GL_FALSE);
        // D3D8 only exposes stage 0 to this renderer.  Always select the
        // corresponding GLES unit before changing its binding or sampler
        // state; otherwise a previous multi-texture pass can poison text and
        // HUD draws with a stale/black texture.
        glActiveTexture(GL_TEXTURE0);
        if (texture != NULL && (colorUsesTexture || alphaUsesTexture))
        {
            texture->Upload(); glBindTexture(GL_TEXTURE_2D, texture->glName);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, textureStates[D3DTSS_MINFILTER] == D3DTEXF_LINEAR ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, textureStates[D3DTSS_MAGFILTER] == D3DTEXF_LINEAR ? GL_LINEAR : GL_NEAREST);
            // GL_CLAMP is not a valid GLES2 wrap mode. Use the edge mode for
            // D3D's clamp state so NPOT text atlases remain complete.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, textureStates[D3DTSS_ADDRESSU] == D3DTADDRESS_CLAMP ? GL_CLAMP_TO_EDGE : GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, textureStates[D3DTSS_ADDRESSV] == D3DTADDRESS_CLAMP ? GL_CLAMP_TO_EDGE : GL_REPEAT);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            ConfigureTextureComponent(GL_COMBINE_RGB, GL_SOURCE0_RGB, GL_SOURCE1_RGB,
                                      GL_OPERAND0_RGB, GL_OPERAND1_RGB,
                                      textureStates[D3DTSS_COLOROP],
                                      textureStates[D3DTSS_COLORARG1],
                                      textureStates[D3DTSS_COLORARG2], GL_SRC_COLOR);
            ConfigureTextureComponent(GL_COMBINE_ALPHA, GL_SOURCE0_ALPHA, GL_SOURCE1_ALPHA,
                                      GL_OPERAND0_ALPHA, GL_OPERAND1_ALPHA,
                                      textureStates[D3DTSS_ALPHAOP],
                                      textureStates[D3DTSS_ALPHAARG1],
                                      textureStates[D3DTSS_ALPHAARG2], GL_SRC_ALPHA);
            const DWORD factor = renderStates[D3DRS_TEXTUREFACTOR];
            const GLfloat constantColor[4] = {
                ((factor >> 16) & 255) / 255.0f,
                ((factor >> 8) & 255) / 255.0f,
                (factor & 255) / 255.0f,
                ((factor >> 24) & 255) / 255.0f
            };
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constantColor);
        }
        else
        {
            // The GLES2 immediate wrapper treats a bound texture as the
            // authoritative texture-enable state. D3D8 untextured primitives
            // (notably the dialogue box) must not sample the preceding sprite.
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    D3DCOLOR EffectiveColor(D3DCOLOR diffuse)
    {
        const DWORD factor = renderStates[D3DRS_TEXTUREFACTOR];
        const DWORD colorModifier = TextureOperationModifier(
            textureStates[D3DTSS_COLOROP], textureStates[D3DTSS_COLORARG1],
            textureStates[D3DTSS_COLORARG2], diffuse, factor);
        const DWORD alphaModifier = TextureOperationModifier(
            textureStates[D3DTSS_ALPHAOP], textureStates[D3DTSS_ALPHAARG1],
            textureStates[D3DTSS_ALPHAARG2], diffuse, factor);
        return (alphaModifier & 0xff000000u) | (colorModifier & 0x00ffffffu);
    }
    HRESULT Draw(D3DPRIMITIVETYPE type, UINT primitiveCount, const BYTE *data, UINT stride)
    {
        UINT count = VertexCount(type, primitiveCount);
        bool transformed = (fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW;
        UINT offset = transformed ? 16 : 12;
        if (fvf & D3DFVF_NORMAL) offset += 12;
        if (fvf & D3DFVF_PSIZE) offset += 4;
        bool hasDiffuse = (fvf & D3DFVF_DIFFUSE) != 0; UINT colorOffset = offset;
        if (hasDiffuse) offset += 4;
        if (fvf & D3DFVF_SPECULAR) offset += 4;
        bool hasTexture = (fvf & D3DFVF_TEXCOUNT_MASK) != 0; UINT textureOffset = offset;
        PrepareState();
        if (!transformed &&
            (type == D3DPT_TRIANGLELIST || type == D3DPT_TRIANGLESTRIP ||
             type == D3DPT_TRIANGLEFAN))
        {
            return DrawClippedTriangles(type, primitiveCount, data, stride,
                                        colorOffset, textureOffset,
                                        hasDiffuse, hasTexture);
        }
        glBegin(PrimitiveMode(type));
        for (UINT index = 0; index < count; ++index)
        {
            const BYTE *vertex = data + index * stride; float x, y, z, fogCoordinate;
            TransformPosition(reinterpret_cast<const float *>(vertex), transformed, &x, &y, &z,
                              &fogCoordinate);
            D3DCOLOR color = hasDiffuse ? *reinterpret_cast<const D3DCOLOR *>(vertex + colorOffset) : 0xffffffffu;
            color = EffectiveColor(color);
            if (hasTexture)
            {
                const float *uv = reinterpret_cast<const float *>(vertex + textureOffset);
                float u = uv[0], v = uv[1];
                if (!transformed)
                {
                    u = uv[0] * textureTransform._11 + uv[1] * textureTransform._21 + textureTransform._31;
                    v = uv[0] * textureTransform._12 + uv[1] * textureTransform._22 + textureTransform._32;
                }
                glTexCoord2f(u, v);
            }
            glColor4ub((color >> 16) & 255, (color >> 8) & 255, color & 255, (color >> 24) & 255);
            IosLegacySetFogCoordinate(fogCoordinate);
            // TransformPosition returns D3D's [0, 1] post-transform depth.
            // Unlike the desktop fixed-function path, the iOS GLES2 shader
            // does not apply glOrtho's Z negation, so map D3D near/far to
            // GLES clip space directly. Reversing this value lets distant
            // floor quads overwrite nearer scenery such as Stage 1 trees.
            glVertex3f(x, y, 2.0f * z - 1.0f);
        }
        glEnd(); return S_OK;
    }
    ULONG refs;
    SDL_Window *window;
    SDL_GLContext context;
    LinuxSurface *backbuffer;
    LinuxTexture *texture;
    LinuxVertexBuffer *vertexBuffer;
    DWORD fvf;
    UINT streamStride;
    GLuint renderFramebuffer, renderColorTexture, renderDepthBuffer, dialogueSnapshotTexture;
    bool framebufferReady, dialogueSnapshotReady, wasDialogPresent;
    unsigned long presentCount;
    GLint defaultFramebuffer;
    GLint defaultRenderbuffer;
    DWORD renderStates[256], textureStates[32];
    D3DMATRIX world, view, projection, textureTransform;
    D3DVIEWPORT8 viewport;
};

class LinuxDirect3D : public IDirect3D8
{
  public:
    LinuxDirect3D() : refs(1) {}
    ULONG AddRef() { return ++refs; }
    ULONG Release() { ULONG value = --refs; if (value == 0) delete this; return value; }
    HRESULT GetAdapterDisplayMode(UINT, D3DDISPLAYMODE *mode)
    {
        if (mode == NULL) return E_INVALIDARG;
        mode->Width = 640; mode->Height = 480; mode->RefreshRate = 60; mode->Format = D3DFMT_X8R8G8B8; return S_OK;
    }
    HRESULT CheckDeviceFormat(UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) { return S_OK; }
    HRESULT CreateDevice(UINT, D3DDEVTYPE, HWND window, DWORD, D3DPRESENT_PARAMETERS *parameters,
                         IDirect3DDevice8 **result)
    {
        if (window == NULL || parameters == NULL || result == NULL) return E_INVALIDARG;
        LinuxDevice *device = new(std::nothrow) LinuxDevice(reinterpret_cast<SDL_Window *>(window), *parameters);
        if (device == NULL) return E_OUTOFMEMORY;
        if (!device->Ready()) { delete device; *result = NULL; return E_FAIL; }
        *result = device; return S_OK;
    }
  private: ULONG refs;
};
} // namespace

void TH095IosDrawScreenQuad(IDirect3DDevice8 *device, const void *vertices, UINT stride)
{
    if (device != NULL && vertices != NULL)
        static_cast<LinuxDevice *>(device)->DrawScreenQuad(vertices, stride);
}

void TH095IosMarkTextureRgba(IDirect3DTexture8 *texture)
{
    if (texture == NULL) return;
    LinuxTexture *iosTexture = static_cast<LinuxTexture *>(texture);
    iosTexture->rgbaSource = true;
    iosTexture->uploaded = false;
}

bool TH095_linux_surface_access(IDirect3DSurface8 *surfaceRaw, LinuxSurfaceAccess *access, bool readBackbuffer)
{
    if (surfaceRaw == NULL || access == NULL) return false;
    LinuxSurface *surface = static_cast<LinuxSurface *>(surfaceRaw);
    // Only read GPU pixels when the caller explicitly requests a source
    // snapshot.  Reading a destination backbuffer before every CopyRects call
    // turns one 640x480 blit into a full GPU->CPU round trip per frame.
    if (readBackbuffer) surface->ReadBackbuffer();
    access->pixels = surface->pixels.empty() ? NULL : &surface->pixels[0];
    access->width = surface->width; access->height = surface->height;
    access->pitch = surface->pitch; access->format = surface->format; return true;
}

void TH095_linux_surface_changed(IDirect3DSurface8 *surfaceRaw)
{
    if (surfaceRaw == NULL) return;
    LinuxSurface *surface = static_cast<LinuxSurface *>(surfaceRaw);
    surface->dirty = true;
    surface->FlushBackbuffer();
}

extern "C" IDirect3D8 *Direct3DCreate8(UINT sdkVersion)
{ return sdkVersion == D3D_SDK_VERSION ? new(std::nothrow) LinuxDirect3D() : NULL; }
