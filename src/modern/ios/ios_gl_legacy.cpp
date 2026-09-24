#include "ios_gl_legacy.hpp"
#include "ios_touch.hpp"

#include <OpenGLES/ES2/glext.h>
#include <stdio.h>
#include <string.h>
#include <vector>

namespace
{
struct ImmediateVertex
{
    GLfloat position[4];
    GLfloat color[4];
    GLfloat texcoord[2];
    GLfloat fogFactor;
    GLfloat clipSpace;
};

std::vector<ImmediateVertex> g_vertices;
GLenum g_primitiveMode = GL_TRIANGLES;
GLfloat g_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
GLfloat g_texcoord[2] = {0.0f, 0.0f};
GLuint g_program;
GLint g_position;
GLint g_colorAttribute;
GLint g_texcoordAttribute;
GLint g_resolution;
GLint g_texture;
GLint g_useTextureRgb;
GLint g_useTextureAlpha;
GLint g_clipSpaceAttribute;
GLint g_fogFactorAttribute;
GLint g_fogColorUniform;
GLint g_alphaThresholdUniform;
GLfloat g_fogColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
GLfloat g_fogStart;
GLfloat g_fogEnd = 1.0f;
GLfloat g_fogCoordinate;
GLfloat g_alphaThreshold = -1.0f;
GLboolean g_forceAlphaDiscard = GL_FALSE;
bool g_fogEnabled;
bool g_textureRgbEnabled = true;
bool g_textureAlphaEnabled = true;
bool g_initialized;

GLuint CompileShader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[1024]; GLsizei length = 0;
        glGetShaderInfoLog(shader, sizeof(log), &length, log);
        fprintf(stderr, "TH095-modern: GLES2 shader compile failed: %.*s\n", (int)length, log);
        glDeleteShader(shader); return 0;
    }
    return shader;
}

bool Initialize()
{
    if (g_initialized) return g_program != 0;
    g_initialized = true;
    static const char vertexSource[] =
        "attribute vec4 aPosition;\n"
        "attribute vec4 aColor;\n"
        "attribute vec2 aTexCoord;\n"
        "attribute float aFogFactor;\n"
        "attribute float aClipSpace;\n"
        "uniform vec2 uResolution;\n"
        "varying vec4 vColor;\n"
        "varying vec2 vTexCoord;\n"
        "varying float vFogFactor;\n"
        "void main() {\n"
        "  vec2 p = aPosition.xy / uResolution * 2.0 - 1.0;\n"
        "  vec4 screenPosition = vec4(p.x, -p.y, aPosition.z, 1.0);\n"
        "  gl_Position = mix(screenPosition, aPosition, aClipSpace);\n"
        "  vColor = aColor; vTexCoord = aTexCoord; vFogFactor = aFogFactor;\n"
        "}\n";
    static const char fragmentSource[] =
        "precision mediump float;\n"
        "uniform sampler2D uTexture;\n"
        "uniform int uUseTextureRgb;\n"
        "uniform int uUseTextureAlpha;\n"
        "uniform vec4 uFogColor;\n"
        "uniform float uAlphaThreshold;\n"
        "varying vec4 vColor;\n"
        "varying vec2 vTexCoord;\n"
        "varying float vFogFactor;\n"
        "void main() {\n"
        "  vec4 texel = texture2D(uTexture, vTexCoord);\n"
        "  vec3 textureRgb = uUseTextureRgb != 0 ? texel.rgb : vec3(1.0);\n"
        "  float textureAlpha = uUseTextureAlpha != 0 ? texel.a : 1.0;\n"
        "  vec4 result = vec4(textureRgb * vColor.rgb, textureAlpha * vColor.a);\n"
        "  if (result.a < uAlphaThreshold) discard;\n"
        "  result.rgb = mix(uFogColor.rgb, result.rgb, vFogFactor);\n"
        "  gl_FragColor = result;\n"
        "}\n";
    GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertex == 0 || fragment == 0)
    {
        th095::modern::LogStartup("gles2: shader compilation failed");
        return false;
    }
    g_program = glCreateProgram();
    glAttachShader(g_program, vertex); glAttachShader(g_program, fragment);
    glBindAttribLocation(g_program, 0, "aPosition");
    glBindAttribLocation(g_program, 1, "aColor");
    glBindAttribLocation(g_program, 2, "aTexCoord");
    glBindAttribLocation(g_program, 3, "aFogFactor");
    glBindAttribLocation(g_program, 4, "aClipSpace");
    glLinkProgram(g_program);
    glDeleteShader(vertex); glDeleteShader(fragment);
    GLint success = GL_FALSE;
    glGetProgramiv(g_program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[1024]; GLsizei length = 0;
        glGetProgramInfoLog(g_program, sizeof(log), &length, log);
        fprintf(stderr, "TH095-modern: GLES2 program link failed: %.*s\n", (int)length, log);
        glDeleteProgram(g_program); g_program = 0;
        th095::modern::LogStartup("gles2: program link failed");
        return false;
    }
    g_position = 0; g_colorAttribute = 1; g_texcoordAttribute = 2;
    g_resolution = glGetUniformLocation(g_program, "uResolution");
    g_texture = glGetUniformLocation(g_program, "uTexture");
    g_useTextureRgb = glGetUniformLocation(g_program, "uUseTextureRgb");
    g_useTextureAlpha = glGetUniformLocation(g_program, "uUseTextureAlpha");
    g_fogFactorAttribute = 3;
    g_clipSpaceAttribute = 4;
    g_fogColorUniform = glGetUniformLocation(g_program, "uFogColor");
    g_alphaThresholdUniform = glGetUniformLocation(g_program, "uAlphaThreshold");
    char uniformMessage[192];
    snprintf(uniformMessage, sizeof(uniformMessage),
             "gles2: uniforms texture=%d rgb=%d alpha=%d threshold=%d",
             g_texture, g_useTextureRgb, g_useTextureAlpha, g_alphaThresholdUniform);
    th095::modern::LogStartup(uniformMessage);
    th095::modern::LogStartup("gles2: immediate shader ready");
    return true;
}
}

void glBegin(GLenum mode)
{
    static bool loggedBegin;
    if (!loggedBegin)
    {
        loggedBegin = true;
        th095::modern::LogStartup("gles2: glBegin reached");
    }
    g_primitiveMode = mode;
    g_vertices.clear();
}

void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    g_color[0] = red / 255.0f; g_color[1] = green / 255.0f;
    g_color[2] = blue / 255.0f; g_color[3] = alpha / 255.0f;
}

void glTexCoord2f(GLfloat u, GLfloat v)
{
    g_texcoord[0] = u; g_texcoord[1] = v;
}

void glVertex2f(GLfloat x, GLfloat y)
{
    glVertex3f(x, y, 0.0f);
}

static void AppendVertex(GLfloat x, GLfloat y, GLfloat z, GLfloat w,
                         GLfloat clipSpace)
{
    ImmediateVertex vertex;
    vertex.position[0] = x; vertex.position[1] = y;
    vertex.position[2] = z; vertex.position[3] = w;
    memcpy(vertex.color, g_color, sizeof(g_color));
    memcpy(vertex.texcoord, g_texcoord, sizeof(g_texcoord));
    vertex.clipSpace = clipSpace;
    vertex.fogFactor = 1.0f;
    if (g_fogEnabled)
    {
        const GLfloat span = g_fogEnd - g_fogStart;
        if (span > -1.0e-8f && span < 1.0e-8f)
            vertex.fogFactor = g_fogCoordinate <= g_fogStart ? 1.0f : 0.0f;
        else
            vertex.fogFactor = (g_fogEnd - g_fogCoordinate) / span;
        if (vertex.fogFactor < 0.0f) vertex.fogFactor = 0.0f;
        if (vertex.fogFactor > 1.0f) vertex.fogFactor = 1.0f;
    }
    g_vertices.push_back(vertex);
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    AppendVertex(x, y, z, 1.0f, 0.0f);
}

void IosLegacyClipVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    AppendVertex(x, y, z, w, 1.0f);
}

void IosLegacySetFog(GLboolean enabled, GLfloat red, GLfloat green, GLfloat blue,
                     GLfloat start, GLfloat end)
{
    g_fogEnabled = enabled != GL_FALSE;
    g_fogColor[0] = red;
    g_fogColor[1] = green;
    g_fogColor[2] = blue;
    g_fogColor[3] = 1.0f;
    g_fogStart = start;
    g_fogEnd = end;
}

void IosLegacySetFogCoordinate(GLfloat coordinate)
{
    g_fogCoordinate = coordinate;
}

void IosLegacySetAlphaThreshold(GLfloat threshold)
{
    g_alphaThreshold = threshold;
}

void IosLegacySetTextureUsage(GLboolean useRgb, GLboolean useAlpha)
{
    g_textureRgbEnabled = useRgb != GL_FALSE;
    g_textureAlphaEnabled = useAlpha != GL_FALSE;
}

void IosLegacySetForceAlphaDiscard(GLboolean enabled)
{
    g_forceAlphaDiscard = enabled != GL_FALSE ? GL_TRUE : GL_FALSE;
}

void glEnd()
{
    static bool loggedEnd;
    if (!loggedEnd)
    {
        loggedEnd = true;
        th095::modern::LogStartup("gles2: glEnd reached");
    }
    if (g_vertices.empty() || !Initialize()) return;
    GLint viewport[4] = {0, 0, 640, 480};
    glGetIntegerv(GL_VIEWPORT, viewport);
    static unsigned errorReports = 0;
    for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
    {
        if (errorReports++ < 16)
        {
            char message[96];
            snprintf(message, sizeof(message), "gles2: pre-draw error=0x%x", error);
            th095::modern::LogStartup(message);
        }
    }
    // D3D8 stage 0 maps to GLES texture unit zero.  Explicitly selecting it
    // here prevents a previous multi-texture/background pass from making the
    // localized text shader sample an uninitialised unit (which appears as a
    // solid black quad on the simulator).
    glActiveTexture(GL_TEXTURE0);
    // Query the binding after selecting unit zero. Reading it before the
    // active-unit change observes a stale unit left by a 3-D pass and makes a
    // valid text texture look unbound to the shader.
    GLint textureName = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureName);
    glUseProgram(g_program);
    glUniform2f(g_resolution, viewport[2] > 0 ? (GLfloat)viewport[2] : 640.0f,
                viewport[3] > 0 ? (GLfloat)viewport[3] : 480.0f);
    glUniform1i(g_texture, 0);
    // D3D8 can apply its texture independently to RGB and alpha. The shim
    // passes both decisions explicitly so untextured UI and TFACTOR-driven
    // stage fades do not inherit the previous sprite's sampler state.
    const bool textureBound = textureName != 0;
    glUniform1i(g_useTextureRgb, textureBound && g_textureRgbEnabled ? 1 : 0);
    glUniform1i(g_useTextureAlpha, textureBound && g_textureAlphaEnabled ? 1 : 0);
    glUniform4fv(g_fogColorUniform, 1, g_fogColor);
    const GLfloat alphaThreshold = g_forceAlphaDiscard
        ? (g_alphaThreshold > (1.0f / 255.0f) ? g_alphaThreshold : (1.0f / 255.0f))
        : g_alphaThreshold;
    glUniform1f(g_alphaThresholdUniform, alphaThreshold);
    glEnableVertexAttribArray(g_position); glEnableVertexAttribArray(g_colorAttribute);
    glEnableVertexAttribArray(g_texcoordAttribute);
    glEnableVertexAttribArray(g_fogFactorAttribute);
    glEnableVertexAttribArray(g_clipSpaceAttribute);
    const GLsizei stride = sizeof(ImmediateVertex);
    // Attributes below point at CPU memory, never at an external VBO.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    const GLubyte *base = reinterpret_cast<const GLubyte *>(&g_vertices[0]);
    glVertexAttribPointer(g_position, 4, GL_FLOAT, GL_FALSE, stride, base + 0);
    glVertexAttribPointer(g_colorAttribute, 4, GL_FLOAT, GL_FALSE, stride,
                          base + sizeof(GLfloat) * 4);
    glVertexAttribPointer(g_texcoordAttribute, 2, GL_FLOAT, GL_FALSE, stride,
                          base + sizeof(GLfloat) * 8);
    glVertexAttribPointer(g_fogFactorAttribute, 1, GL_FLOAT, GL_FALSE, stride,
                          base + sizeof(GLfloat) * 10);
    glVertexAttribPointer(g_clipSpaceAttribute, 1, GL_FLOAT, GL_FALSE, stride,
                          base + sizeof(GLfloat) * 11);
    glDrawArrays(g_primitiveMode, 0, (GLsizei)g_vertices.size());
    const GLenum drawError = glGetError();
    if (drawError != GL_NO_ERROR && errorReports++ < 16)
    {
        char message[96];
        snprintf(message, sizeof(message), "gles2: draw error=0x%x texture=%d", drawError, textureName);
        th095::modern::LogStartup(message);
    }
    glDisableVertexAttribArray(g_position); glDisableVertexAttribArray(g_colorAttribute);
    glDisableVertexAttribArray(g_texcoordAttribute);
    glDisableVertexAttribArray(g_fogFactorAttribute);
    glDisableVertexAttribArray(g_clipSpaceAttribute);
}
