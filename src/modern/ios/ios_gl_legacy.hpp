#pragma once

// The first iOS bring-up keeps the D3D8 compatibility device source shared
// with the validated Linux port. OpenGL ES 2 has no fixed-function entry
// points, so the legacy calls are isolated here. The renderer replacement will
// remove these shims; keeping them local makes that migration reviewable.
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>

typedef double GLdouble;
typedef double GLclampd;

#ifndef GL_ALL_ATTRIB_BITS
#define GL_ALL_ATTRIB_BITS 0xFFFFFFFFu
#endif
#ifndef GL_LIGHTING
#define GL_LIGHTING 0x0B50
#endif
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST 0x0BC0
#endif
#ifndef GL_FOG
#define GL_FOG 0x0B60
#endif
#ifndef GL_FOG_MODE
#define GL_FOG_MODE 0x0B65
#endif
#ifndef GL_FOG_COORDINATE_SOURCE
#define GL_FOG_COORDINATE_SOURCE 0x8450
#endif
#ifndef GL_FOG_COORDINATE
#define GL_FOG_COORDINATE 0x8451
#endif
#ifndef GL_FOG_COLOR
#define GL_FOG_COLOR 0x0B66
#endif
#ifndef GL_FOG_START
#define GL_FOG_START 0x0B63
#endif
#ifndef GL_FOG_END
#define GL_FOG_END 0x0B64
#endif
#ifndef GL_FOG_DENSITY
#define GL_FOG_DENSITY 0x0B62
#endif
#ifndef GL_FOG_LINEAR
#define GL_FOG_LINEAR 0x2601
#endif
#ifndef GL_TEXTURE_ENV
#define GL_TEXTURE_ENV 0x2300
#endif
#ifndef GL_TEXTURE_ENV_MODE
#define GL_TEXTURE_ENV_MODE 0x2200
#endif
#ifndef GL_TEXTURE_ENV_COLOR
#define GL_TEXTURE_ENV_COLOR 0x2201
#endif
#ifndef GL_COMBINE
#define GL_COMBINE 0x8570
#endif
#ifndef GL_COMBINE_RGB
#define GL_COMBINE_RGB 0x8571
#endif
#ifndef GL_COMBINE_ALPHA
#define GL_COMBINE_ALPHA 0x8572
#endif
#ifndef GL_SOURCE0_RGB
#define GL_SOURCE0_RGB 0x8580
#endif
#ifndef GL_SOURCE1_RGB
#define GL_SOURCE1_RGB 0x8581
#endif
#ifndef GL_SOURCE0_ALPHA
#define GL_SOURCE0_ALPHA 0x8588
#endif
#ifndef GL_SOURCE1_ALPHA
#define GL_SOURCE1_ALPHA 0x8589
#endif
#ifndef GL_OPERAND0_RGB
#define GL_OPERAND0_RGB 0x8590
#endif
#ifndef GL_OPERAND1_RGB
#define GL_OPERAND1_RGB 0x8591
#endif
#ifndef GL_OPERAND0_ALPHA
#define GL_OPERAND0_ALPHA 0x8598
#endif
#ifndef GL_OPERAND1_ALPHA
#define GL_OPERAND1_ALPHA 0x8599
#endif
#ifndef GL_PRIMARY_COLOR
#define GL_PRIMARY_COLOR 0x8577
#endif
#ifndef GL_CONSTANT
#define GL_CONSTANT 0x8576
#endif
#ifndef GL_TEXTURE
#define GL_TEXTURE 0x1702
#endif
#ifndef GL_SRC_COLOR
#define GL_SRC_COLOR 0x0300
#endif
#ifndef GL_SRC_ALPHA
#define GL_SRC_ALPHA 0x0302
#endif
#ifndef GL_ONE_MINUS_SRC_COLOR
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#endif
#ifndef GL_ONE_MINUS_SRC_ALPHA
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#endif
#ifndef GL_BACK
#define GL_BACK 0x0405
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_OES
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_OES
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER GL_RENDERBUFFER_OES
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#endif
#ifndef GL_CLAMP
#define GL_CLAMP GL_CLAMP_TO_EDGE
#endif
#ifndef GL_MODELVIEW
#define GL_MODELVIEW 0x1700
#endif
#ifndef GL_PROJECTION
#define GL_PROJECTION 0x1701
#endif
#ifndef GL_MODULATE
#define GL_MODULATE 0x2100
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif

// The original renderer uses the OpenGL 1.x immediate-mode API. OpenGL ES 2
// does not expose that API, so these functions are implemented by the small
// GLES2 compatibility layer in ios_gl_legacy.cpp. Keeping the old call sites
// unchanged is important because the D3D8 shim emits already-transformed
// screen-space vertices.
void glBegin(GLenum mode);
void glEnd();
inline void glPushAttrib(GLbitfield) {}
inline void glPopAttrib() {}
inline void glPushMatrix() {}
inline void glPopMatrix() {}
inline void glLoadIdentity() {}
inline void glMatrixMode(GLenum) {}
inline void glOrtho(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble) {}
void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void glTexCoord2f(GLfloat u, GLfloat v);
void glVertex2f(GLfloat x, GLfloat y);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
// Submit an already transformed homogeneous clip-space position. Keeping W
// lets GLES perform perspective-correct interpolation for stage 3D geometry.
void IosLegacyClipVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void IosLegacySetFog(GLboolean enabled, GLfloat red, GLfloat green, GLfloat blue,
                     GLfloat start, GLfloat end);
void IosLegacySetFogCoordinate(GLfloat coordinate);
void IosLegacySetAlphaThreshold(GLfloat threshold);
void IosLegacySetTextureUsage(GLboolean useRgb, GLboolean useAlpha);
void IosLegacySetForceAlphaDiscard(GLboolean enabled);
inline void glDrawBuffer(GLenum) {}
inline void glReadBuffer(GLenum) {}
inline void glAlphaFunc(GLenum, GLclampf) {}
inline void glFogCoordf(GLfloat coordinate) { IosLegacySetFogCoordinate(coordinate); }
inline void glFogf(GLenum, GLfloat) {}
inline void glFogfv(GLenum, const GLfloat *) {}
inline void glFogi(GLenum, GLint) {}
inline void glTexEnvi(GLenum, GLenum, GLint) {}
inline void glTexEnvfv(GLenum, GLenum, const GLfloat *) {}
inline void glClearDepth(GLclampd depth) { glClearDepthf((GLclampf)depth); }
