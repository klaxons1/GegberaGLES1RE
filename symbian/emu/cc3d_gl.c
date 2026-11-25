/* cc3d_gl.c - the CC3DBench GLES scenes (docs/CC3DBench.7z,
 * benchmark/cc3dprobe2.cpp: InitGl, SetUpSceneGl, DrawSceneGl, SceneMatrix),
 * ported to C for the bare-metal ARM emulator harness.
 *
 * Every frame of every scene is drawn through the real EGL/GL entry points
 * into a 320x240 EColor64K window with a 16-bit depth buffer, exactly as the
 * phone benchmark requests (EGL_BUFFER_SIZE 16, EGL_DEPTH_SIZE 16). After each
 * frame the runner receives CRCs of the colour and depth buffers, so a build
 * with CCsoft spans can be compared pixel for pixel with the JIT pipeline.
 */
#include "GLES/gl.h"
#include "GLES/egl.h"
#include "gerbera/egl/eglInternal.h"
#include "gerbera/api_opengl/oglContext.h"
#include "gerbera/grState.h"
#include "suite.h"
#include "emu_rt.h"

#define WIDTH 320
#define HEIGHT 240

/* Runner-patched configuration (symbol emu_config). */
volatile struct
{
    int magic;       /* 0xCC3D0001 */
    int frames;      /* frames per scene (16 on the phone) */
    int firstScene;
    int lastScene;   /* inclusive */
    int ccsoft;      /* 1: CCsoft spans enabled, 0: JIT only */
    int dumpFrame;   /* frame index to dump through svc #5, -1 none */
    int dither;      /* 1: leave GL_DITHER on (GL's default) instead of the bench's glDisable */
} emu_config = { 0xCC3D0001, 16, 0, SUITE_COUNT - 1, 1, -1, 0 };

#ifdef GERBERA_CCSOFT
#include "gerbera/grCcsoft.h"
#endif

static const float KPi = 3.14159265358979f;
static unsigned short s_tile[64 * 64];
static unsigned short s_front[WIDTH * HEIGHT];
static GLuint s_textures[ST_KINDS];
static eglNativeWindow_s s_window = { WIDTH, HEIGHT, WIDTH * 2, EGL_SYMBIAN_ECOLOR64K, s_front, 0 };
static EGLDisplay s_display;
static EGLSurface s_surface;

/* --- math (cc3dprobe2.cpp Multiply/Identity/Translate/Rotate) ------------ */

static double emu_sine(double x)
{
    double sign = 1, term, sum, square;
    int i;
    while (x > 3.14159265358979) x -= 6.28318530717959;
    while (x < -3.14159265358979) x += 6.28318530717959;
    if (x < 0) { x = -x; sign = -1; }
    if (x > 1.5707963267949) x = 3.14159265358979 - x;
    square = x * x;
    term = x;
    sum = x;
    for (i = 1; i < 10; i++)
    {
        term = -term * square / ((2 * i) * (2 * i + 1));
        sum += term;
    }
    return sign * sum;
}

static double emu_cosine(double x) { return emu_sine(x + 1.5707963267949); }

static void Multiply(float *out, const float *a, const float *b)
{
    float result[16];
    int c, r;
    for (c = 0; c < 4; c++)
        for (r = 0; r < 4; r++)
            result[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] +
                                a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
    for (c = 0; c < 16; c++)
        out[c] = result[c];
}

static void Identity(float *out)
{
    int i;
    for (i = 0; i < 16; i++)
        out[i] = 0;
    out[0] = out[5] = out[10] = out[15] = 1;
}

static void Translate(float *m, float x, float y, float z)
{
    float t[16];
    Identity(t);
    t[12] = x;
    t[13] = y;
    t[14] = z;
    Multiply(m, m, t);
}

static void Rotate(float *m, int aroundZ, double degrees)
{
    double radians = degrees * 3.14159265358979 / 180;
    float s = (float)emu_sine(radians), c = (float)emu_cosine(radians), t[16];
    Identity(t);
    if (aroundZ)
    {
        t[0] = c; t[1] = s; t[4] = -s; t[5] = c;
    }
    else
    {
        t[5] = c; t[6] = s; t[9] = -s; t[10] = c;
    }
    Multiply(m, m, t);
}

static void Perspective(float *out)
{
    const float nearPlane = 1, farPlane = 2000;
    int i;
    for (i = 0; i < 16; i++)
        out[i] = 0;
    out[0] = 1.125f;
    out[5] = 1.5f;
    out[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    out[11] = -1;
    out[14] = -2 * farPlane * nearPlane / (farPlane - nearPlane);
}

static void SceneProjection(int scene, float *out)
{
    const int kind = suite_scenes[scene].matrix;
    Identity(out);
    if (kind == SX_OBJECT || kind == SX_FOG)
        Perspective(out);
}

static void SceneMatrix(int scene, int frame, float *out)
{
    Identity(out);
    switch (suite_scenes[scene].matrix)
    {
    case SX_PERSP:
        Perspective(out); Translate(out, 0, 0, -180); Rotate(out, 0, -60);
        Rotate(out, 1, frame); Translate(out, -100, -60, 0);
        return;
    case SX_FLOOR:
        Perspective(out); Translate(out, 0, 0, -60); Rotate(out, 0, -70);
        Translate(out, 0, -(frame & 15) * 0.5f, 0);
        return;
    case SX_OBJECT:
        Translate(out, 0, 0, -280); Rotate(out, 0, -50); Rotate(out, 1, frame * 3);
        return;
    case SX_FOG:
        Translate(out, 0, 0, -200); Rotate(out, 0, -70); Translate(out, 0, -(frame & 15) * 4.0f, 0);
        return;
    case SX_NEAR:
        Perspective(out); Translate(out, 0, 0, -20); Rotate(out, 0, -80);
        Translate(out, -500, -300 - (frame & 15) * 2, 0);
        return;
    case SX_GAME:
        Perspective(out); Translate(out, 0, 0, -420); Rotate(out, 0, -55); Rotate(out, 1, frame * 2);
        return;
    default:
        break;
    }
    out[0] = 2.0f / 320;
    out[5] = 2.0f / 240;
    out[12] = -1;
    out[13] = -1;
    if (suite_scenes[scene].matrix == SX_SPIN)
    {
        Translate(out, 160, 120, 0); Rotate(out, 1, frame); Translate(out, -100, -60, 0);
    }
    else if (suite_scenes[scene].matrix == SX_LAYERS)
        out[10] = 1.0f / 128;
    else if (suite_scenes[scene].matrix == SX_OFFSCREEN)
        Translate(out, -(frame & 15), 0, 0);
}

/* --- materials and lights (cc3dprobe2.cpp BuildColorLights, GlLight) ----- */

enum { LIGHT_DIRECTIONAL, LIGHT_POINT, LIGHT_SPOT };
typedef struct
{
    int kind;
    int x, y, z;             /* 16.16 */
    int diffuse[3], specular[3], ambient[3];
    int constant, linear;    /* 16.16 */
    int spot_x, spot_y, spot_z;
} ColorLight;
typedef struct { int ambient[3], diffuse[3], specular[3], emission[3], scene_ambient[3]; } Material;

static ColorLight s_colorLights[3];
static int s_colorLightCount;
static Material s_material;
static const int KShininess = 16, KSpotExponent = 8, KSpotDegrees = 14;
static const float KLightDirections[8][3] = {
    {0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {0.707f, 0.707f, 0},
    {-0.707f, 0.707f, 0}, {0, 0.707f, 0.707f}, {0.707f, 0, 0.707f}, {0, -0.707f, 0.707f}};

static int IsColorLit(int mode) { return mode >= SM_LIGHT_COLOR && mode <= SM_COLOR_MATERIAL; }
static void SetColor(int *out, int r, int g, int b) { out[0] = r; out[1] = g; out[2] = b; }

static void BuildColorLights(int mode)
{
    int i;
    ColorLight *first = &s_colorLights[0];
    SetColor(s_material.ambient, 51, 51, 51);
    SetColor(s_material.diffuse, 205, 205, 205);
    SetColor(s_material.specular, 0, 0, 0);
    SetColor(s_material.emission, 0, 0, 0);
    SetColor(s_material.scene_ambient, 51, 51, 51);
    for (i = 0; i < (int)sizeof(s_colorLights); i++)
        ((char *)s_colorLights)[i] = 0;
    s_colorLightCount = mode == SM_LIGHT_COLOR3 ? 3 : 1;
    for (i = 0; i < s_colorLightCount; i++)
    {
        ColorLight *l = &s_colorLights[i];
        l->kind = LIGHT_DIRECTIONAL;
        l->x = i == 1 ? 65536 : 0;
        l->y = i == 2 ? 65536 : 0;
        l->z = i == 0 ? 65536 : 0;
        SetColor(l->diffuse, 256, 256, 256);
        l->constant = 65536;
    }
    switch (mode)
    {
    case SM_LIGHT_COLOR: SetColor(first->diffuse, 256, 102, 51); break;
    case SM_LIGHT_COLOR3:
        SetColor(s_colorLights[0].diffuse, 256, 51, 51);
        SetColor(s_colorLights[1].diffuse, 51, 256, 51);
        SetColor(s_colorLights[2].diffuse, 51, 51, 256);
        break;
    case SM_LIGHT_POINT:
        first->kind = LIGHT_POINT;
        first->x = 150 << 16; first->y = 100 << 16; first->z = -150 << 16;
        first->linear = 66;
        break;
    case SM_LIGHT_SPOT:
        first->kind = LIGHT_SPOT;
        first->x = first->y = first->z = 0;
        first->spot_z = -65536;
        break;
    case SM_MATERIAL:
        SetColor(s_material.ambient, 77, 77, 26);
        SetColor(s_material.diffuse, 230, 179, 77);
        SetColor(s_material.specular, 256, 256, 256);
        SetColor(s_material.emission, 26, 0, 51);
        SetColor(s_material.scene_ambient, 51, 51, 77);
        SetColor(first->specular, 256, 256, 256);
        break;
    default:
        break;
    }
}

static void GlColor(GLfloat *out, const int *color)
{
    int i;
    for (i = 0; i < 3; i++)
        out[i] = color[i] / 256.0f;
    out[3] = 1;
}

static void GlLight(int index)
{
    const GLenum light = GL_LIGHT0 + index;
    const GLfloat position[] = {KLightDirections[index][0], KLightDirections[index][1], KLightDirections[index][2], 0};
    const GLfloat white[] = {1, 1, 1, 1};
    const GLfloat black[] = {0, 0, 0, 1};
    glEnable(light);
    glLightfv(light, GL_POSITION, position);
    glLightfv(light, GL_DIFFUSE, white);
    glLightfv(light, GL_SPECULAR, white);
    glLightfv(light, GL_AMBIENT, black);
    glLightf(light, GL_CONSTANT_ATTENUATION, 1);
    glLightf(light, GL_LINEAR_ATTENUATION, 0);
    glLightf(light, GL_SPOT_CUTOFF, 180);
}

static void GlColorLight(int index, const ColorLight *l)
{
    const GLenum light = GL_LIGHT0 + index;
    const int directional = l->kind == LIGHT_DIRECTIONAL;
    const GLfloat position[] = {l->x / 65536.0f, l->y / 65536.0f, l->z / 65536.0f, directional ? 0.0f : 1.0f};
    GLfloat color[4];
    glEnable(light);
    glLightfv(light, GL_POSITION, position);
    GlColor(color, l->diffuse);
    glLightfv(light, GL_DIFFUSE, color);
    GlColor(color, l->specular);
    glLightfv(light, GL_SPECULAR, color);
    GlColor(color, l->ambient);
    glLightfv(light, GL_AMBIENT, color);
    glLightf(light, GL_CONSTANT_ATTENUATION, l->constant / 65536.0f);
    glLightf(light, GL_LINEAR_ATTENUATION, l->linear / 65536.0f);
    if (l->kind == LIGHT_SPOT)
    {
        const GLfloat direction[] = {l->spot_x / 65536.0f, l->spot_y / 65536.0f, l->spot_z / 65536.0f};
        glLightfv(light, GL_SPOT_DIRECTION, direction);
        glLightf(light, GL_SPOT_CUTOFF, (GLfloat)KSpotDegrees);
        glLightf(light, GL_SPOT_EXPONENT, (GLfloat)KSpotExponent);
    }
    else
    {
        glLightf(light, GL_SPOT_CUTOFF, 180);
    }
}

static int SceneLightCount(int scene)
{
    const SuiteScene *s = &suite_scenes[scene];
    if (s->mode == SM_LIGHT8) return 8;
    if (s->flags & SF_LIGHT3) return 3;
    return (s->flags & SF_LIGHT1) ? 1 : 0;
}

/* --- InitGl / SetUpSceneGl / DrawSceneGl -------------------------------- */

static void p2_make_tile(unsigned short *tile)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            tile[y * 64 + x] = (unsigned short)((((x ^ y) & 8) ? 0xF800 : 0x001F) | (x << 5));
}

static unsigned char s_texturePixels[256 * 256 * 4];

static int InitGl(void)
{
    const EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BUFFER_SIZE, 16,
                                 EGL_DEPTH_SIZE, 16, EGL_NONE};
    EGLConfig config;
    EGLint found = 0, actual = 0;
    EGLContext context;
    int kind;

    s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s_display == EGL_NO_DISPLAY || !eglInitialize(s_display, NULL, NULL))
        return 0;
    if (!eglChooseConfig(s_display, attributes, &config, 1, &found) || found == 0)
        return 0;
    eglGetConfigAttrib(s_display, config, EGL_BUFFER_SIZE, &actual);
    s_surface = eglCreateWindowSurface(s_display, config, &s_window, NULL);
    if (s_surface == EGL_NO_SURFACE)
        return 0;
    context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, NULL);
    if (context == EGL_NO_CONTEXT || !eglMakeCurrent(s_display, s_surface, s_surface, context))
        return 0;
    eglSwapInterval(s_display, 0);
    emu_printf("GLES requested 16 bit, config %d bit\n", actual);
    glGenTextures(ST_KINDS, s_textures);
    for (kind = 0; kind < ST_KINDS; kind++)
    {
        glBindTexture(GL_TEXTURE_2D, s_textures[kind]);
        if (kind == ST_TILE)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, s_tile);
        }
        else
        {
            const int texels = suite_texture_size(kind);
            suite_texture(kind, s_texturePixels);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texels, texels, 0, GL_RGBA, GL_UNSIGNED_BYTE, s_texturePixels);
        }
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glViewport(0, 0, WIDTH, HEIGHT);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    if (emu_config.dither)
        glEnable(GL_DITHER);
    else
        glDisable(GL_DITHER);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef(1.0f / (64 * 16), 1.0f / (64 * 16), 1);
    glMatrixMode(GL_MODELVIEW);
    return 1;
}

static void SetUpSceneGl(int scene)
{
    const int flags = suite_scenes[scene].flags, mode = suite_scenes[scene].mode;
    float matrix[16];
    GLfloat value[4];
    GLfloat filter, wrap;
    int i, lights;

    SceneProjection(scene, matrix);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(matrix);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    for (i = 0; i < 8; i++)
        glDisable(GL_LIGHT0 + i);
    glDisable(GL_NORMALIZE);
    glDisable(GL_FOG);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisable(GL_COLOR_MATERIAL);
    BuildColorLights(IsColorLit(mode) ? mode : SM_NONE);
    GlColor(value, s_material.ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, value);
    GlColor(value, s_material.diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, value);
    GlColor(value, s_material.specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, value);
    GlColor(value, s_material.emission);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, value);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, (GLfloat)KShininess);
    GlColor(value, s_material.scene_ambient);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, value);
    glDepthFunc(mode == SM_Z_ALWAYS ? GL_ALWAYS : GL_LEQUAL);
    glPointSize((GLfloat)(mode == SM_POINTS ? suite_scenes[scene].point_size : 1));
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
              mode == SM_REPLACE ? GL_REPLACE : mode == SM_ENV_ADD ? GL_ADD : GL_MODULATE);
    lights = SceneLightCount(scene);
    if (lights > 0)
    {
        glEnable(GL_LIGHTING);
        glEnable(GL_NORMALIZE);
        glEnableClientState(GL_NORMAL_ARRAY);
        for (i = 0; i < lights; i++)
            GlLight(i);
    }
    if (IsColorLit(mode))
    {
        glEnable(GL_LIGHTING);
        glEnable(GL_NORMALIZE);
        glEnableClientState(GL_NORMAL_ARRAY);
        for (i = 0; i < s_colorLightCount; i++)
            GlColorLight(i, &s_colorLights[i]);
        if (mode == SM_COLOR_MATERIAL)
            glEnable(GL_COLOR_MATERIAL);
    }
    if (mode == SM_SPECULAR || mode == SM_SPECULAR3)
    {
        const GLfloat white[] = {1, 1, 1, 1};
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
    }
    if (flags & SF_FOG)
    {
        const GLfloat color[] = {0.5f, 0.5f, 0.6f, 1};
        glEnable(GL_FOG);
        glFogf(GL_FOG_MODE, mode == SM_FOG_EXP ? GL_EXP : mode == SM_FOG_EXP2 ? GL_EXP2 : GL_LINEAR);
        glFogf(GL_FOG_DENSITY, 0.003f);
        glFogf(GL_FOG_START, 100);
        glFogf(GL_FOG_END, 900);
        glFogfv(GL_FOG_COLOR, color);
    }
    glBindTexture(GL_TEXTURE_2D, s_textures[ST_TILE]);
    filter = (flags & SF_LINEAR) ? GL_LINEAR : GL_NEAREST;
    wrap = mode == SM_CLAMP ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    glActiveTexture(GL_TEXTURE1);
    glClientActiveTexture(GL_TEXTURE1);
    if (mode == SM_TWO_TEXTURES)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, s_textures[ST_LIGHT]);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glScalef(1.0f / (64 * 16), 1.0f / (64 * 16), 1);
        glMatrixMode(GL_MODELVIEW);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_SHORT, 0, suite_uv);
    }
    else
    {
        glDisable(GL_TEXTURE_2D);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    glActiveTexture(GL_TEXTURE0);
    glClientActiveTexture(GL_TEXTURE0);
}

static void DrawSceneGl(int scene, int frame)
{
    const int flags = suite_scenes[scene].flags, mode = suite_scenes[scene].mode;
    int i;
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glClearColor(0.05f, 0.1f, (GLfloat)(frame & 15) / 30.0f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (suite_vertex_count == 0)
        return;
    if (mode == SM_Z_NOWRITE)
        glDepthMask(GL_FALSE);
    if (mode == SM_SCISSOR)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(40, 30, 240, 180);
    }
    if (mode == SM_UPLOAD)
    {
        glBindTexture(GL_TEXTURE_2D, s_textures[ST_TILE]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 64, 64, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, s_tile);
    }
    if (flags & (SF_BLEND | SF_ADD) || mode == SM_MULTIPLY || mode == SM_RGB_MULTIPLY)
    {
        glEnable(GL_BLEND);
        if (flags & SF_ADD) glBlendFunc(GL_ONE, GL_ONE);
        else if (mode == SM_MULTIPLY || mode == SM_RGB_MULTIPLY) glBlendFunc(GL_DST_COLOR, GL_ZERO);
        else if (mode == SM_PREMULT) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        else if (mode == SM_ALPHA_ADD || mode == SM_PARTICLE) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    if (flags & SF_CUTOUT)
    {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.5f);
    }
    else
    {
        glDisable(GL_ALPHA_TEST);
    }
    if (flags & SF_DEPTH) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (flags & SF_CULL) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (flags & SF_TINT) glColor4f(0.6f, 0.6f, 0.6f, 1);
    else if (flags & SF_HALF) glColor4f(1, 1, 1, 0.5f);
    else if (mode == SM_ALPHA25) glColor4f(1, 1, 1, 0.25f);
    else glColor4f(1, 1, 1, 1);
    if (flags & SF_TEX) glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s_textures[suite_scenes[scene].texture]);
    if (flags & SF_COLOR)
    {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, suite_colors);
    }
    else
    {
        glDisableClientState(GL_COLOR_ARRAY);
    }
    if ((flags & (SF_LIGHT1 | SF_LIGHT3)) || IsColorLit(mode))
        glNormalPointer(GL_BYTE, 0, suite_normals);
    if (flags & SF_DYNAMIC)
        suite_animate(frame);
    glVertexPointer(3, GL_SHORT, 0, suite_positions);
    glTexCoordPointer(2, GL_SHORT, 0, suite_uv);
    if (mode == SM_STRIP)
    {
        glDrawElements(GL_TRIANGLE_STRIP, suite_index_count, GL_UNSIGNED_SHORT, suite_indices);
        return;
    }
    if (mode == SM_LINES)
    {
        glDrawElements(GL_LINES, suite_index_count, GL_UNSIGNED_SHORT, suite_indices);
        return;
    }
    if (mode == SM_POINTS)
    {
        glDrawArrays(GL_POINTS, 0, suite_vertex_count);
        return;
    }
    if (!(flags & SF_SPLIT))
    {
        glDrawElements(GL_TRIANGLES, suite_index_count, GL_UNSIGNED_SHORT, suite_indices);
        return;
    }
    for (i = 0; i < suite_index_count; i += 6)
    {
        if (flags & SF_SWAP)
            glBindTexture(GL_TEXTURE_2D, s_textures[(i / 6 & 1) == 0 ? ST_TILE : ST_ALPHA]);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, suite_indices + i);
    }
}

static unsigned int crc32(unsigned int crc, const void *data, int n)
{
    const unsigned char *p = data;
    int k;
    crc = ~crc;
    while (n--)
    {
        crc ^= *p++;
        for (k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

int main(void)
{
    int scene, frame;
    p2_make_tile(s_tile);
#ifdef GERBERA_CCSOFT
    g_grCcsoftEnabled = emu_config.ccsoft;
    emu_printf("CC3D emu: CCsoft spans %s\n", g_grCcsoftEnabled ? "on" : "off");
#else
    emu_printf("CC3D emu: JIT build\n");
#endif
    if (!InitGl())
    {
        emu_printf("InitGl failed %x\n", eglGetError());
        emu_exit(1);
    }
    for (scene = emu_config.firstScene; scene <= emu_config.lastScene; scene++)
    {
        oglContext_s *ctx;
        suite_prepare(scene);
        SetUpSceneGl(scene);
        emu_marker(EMU_MARK_SCENE_BEGIN, scene, 0);
        for (frame = 0; frame < emu_config.frames; frame++)
        {
            float matrix[16];
            const unsigned short *depth;
            unsigned int depthCrc = 0;
            SceneMatrix(scene, frame, matrix);
            emu_marker(EMU_MARK_FRAME_BEGIN, scene, frame);
            glLoadMatrixf(matrix);
            DrawSceneGl(scene, frame);
            glFinish();
            ctx = egliGetState()->currentESContext;
            depth = (const unsigned short *)ctx->gerbera->drawSurface.buffers[1].data;
            emu_marker(EMU_MARK_PIPE_END, scene, frame); /* excludes the CRC work below */
            if (depth != NULL)
                depthCrc = crc32(0, depth, WIDTH * HEIGHT * 2);
            emu_marker(EMU_MARK_PIPE_BEGIN, scene, frame);
            eglSwapBuffers(s_display, s_surface);
            emu_marker(EMU_MARK_FRAME_END, scene, frame);
            emu_marker(EMU_MARK_CHECKSUM, scene * 256 + frame, (int)crc32(depthCrc, s_front, WIDTH * HEIGHT * 2));
            if (frame == emu_config.dumpFrame)
                emu_frame(s_front, WIDTH, HEIGHT, scene);
        }
        if (glGetError() != GL_NO_ERROR)
            emu_printf("GL scene %d error\n", scene);
#if defined(GERBERA_CCSOFT_TOGGLE)
        emu_printf("scene %d: %u primitives by C spans, %u by the JIT\n", scene,
                   g_grCcsoftStats[0], g_grCcsoftStats[1]);
        g_grCcsoftStats[0] = g_grCcsoftStats[1] = 0;
#endif
    }
    emu_printf("DONE\n");
    emu_exit(0);
}
