/* GLES/egl.h - Khronos EGL 1.1 public header for OpenGL ES 1.1 Common Profile
 * (Nokia / Hybrid Graphics Gerbera driver reconstruction).
 */
#ifndef __egl_h_
#define __egl_h_

#include <GLES/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- EGL Types ------------------------------------------------------------- */
typedef int           EGLBoolean;
typedef int           EGLint;
typedef int           EGLDisplay;
typedef int           EGLConfig;
typedef int           EGLSurface;
typedef int           EGLContext;
typedef int           NativeDisplayType;
typedef void         *NativeWindowType;
typedef void         *NativePixmapType;

#ifndef EGLAPI
#define EGLAPI GL_API
#endif

#ifndef EGLAPIENTRY
#define EGLAPIENTRY GL_APIENTRY
#endif

/* --- Hosted Native Window / Pixmap Descriptors ----------------------------- */
/* Symbian TDisplayMode values recognized by egliConfigPixmapMatch / s_validColors */
typedef enum eglSymbianDisplayMode_e
{
    EGL_SYMBIAN_ECOLOR64K  = 7,   /* RGB565   (16 bpp) */
    EGL_SYMBIAN_ECOLOR4K   = 10,  /* xRGB4444 (16 bpp) */
    EGL_SYMBIAN_ECOLOR16MU = 11,  /* xRGB8888 (32 bpp) */
    EGL_SYMBIAN_ECOLOR16MA = 12   /* ARGB8888 (32 bpp) */
} eglSymbianDisplayMode_e;

typedef struct eglNativePixmap_s
{
    int   width;
    int   height;
    int   stride;
    int   displayMode; /* eglSymbianDisplayMode_e */
    void *data;
    int   locked;
} eglNativePixmap_s;

typedef struct eglNativeWindow_s
{
    int   width;
    int   height;
    int   stride;
    int   displayMode; /* eglSymbianDisplayMode_e */
    void *frontBuffer; /* optional destination buffer updated on eglSwapBuffers */
    int   swapCount;
} eglNativeWindow_s;

/* --- EGL Versioning -------------------------------------------------------- */
#define EGL_VERSION_1_0                1
#define EGL_VERSION_1_1                1

/* --- EGL Boolean & Special Values ------------------------------------------ */
#define EGL_FALSE                      0
#define EGL_TRUE                       1
#define EGL_DEFAULT_DISPLAY            ((NativeDisplayType)0)
#define EGL_NO_CONTEXT                 ((EGLContext)0)
#define EGL_NO_DISPLAY                 ((EGLDisplay)0)
#define EGL_NO_SURFACE                 ((EGLSurface)0)
#define EGL_DONT_CARE                  ((EGLint)-1)

/* --- EGL Errors ------------------------------------------------------------ */
#define EGL_SUCCESS                    0x3000
#define EGL_NOT_INITIALIZED            0x3001
#define EGL_BAD_ACCESS                 0x3002
#define EGL_BAD_ALLOC                  0x3003
#define EGL_BAD_ATTRIBUTE              0x3004
#define EGL_BAD_CONFIG                 0x3005
#define EGL_BAD_CONTEXT                0x3006
#define EGL_BAD_CURRENT_SURFACE        0x3007
#define EGL_BAD_DISPLAY                0x3008
#define EGL_BAD_MATCH                  0x3009
#define EGL_BAD_NATIVE_PIXMAP          0x300A
#define EGL_BAD_NATIVE_WINDOW          0x300B
#define EGL_BAD_PARAMETER              0x300C
#define EGL_BAD_SURFACE                0x300D
#define EGL_CONTEXT_LOST               0x300E

/* --- EGL Config Attributes ------------------------------------------------- */
#define EGL_BUFFER_SIZE                0x3020
#define EGL_ALPHA_SIZE                 0x3021
#define EGL_BLUE_SIZE                  0x3022
#define EGL_GREEN_SIZE                 0x3023
#define EGL_RED_SIZE                   0x3024
#define EGL_DEPTH_SIZE                 0x3025
#define EGL_STENCIL_SIZE               0x3026
#define EGL_CONFIG_CAVEAT              0x3027
#define EGL_CONFIG_ID                  0x3028
#define EGL_LEVEL                      0x3029
#define EGL_MAX_PBUFFER_HEIGHT         0x302A
#define EGL_MAX_PBUFFER_PIXELS         0x302B
#define EGL_MAX_PBUFFER_WIDTH          0x302C
#define EGL_NATIVE_RENDERABLE          0x302D
#define EGL_NATIVE_VISUAL_ID           0x302E
#define EGL_NATIVE_VISUAL_TYPE         0x302F
#define EGL_SAMPLES                    0x3031
#define EGL_SAMPLE_BUFFERS             0x3032
#define EGL_SURFACE_TYPE               0x3033
#define EGL_TRANSPARENT_TYPE           0x3034
#define EGL_TRANSPARENT_BLUE_VALUE     0x3035
#define EGL_TRANSPARENT_GREEN_VALUE    0x3036
#define EGL_TRANSPARENT_RED_VALUE      0x3037
#define EGL_NONE                       0x3038
#define EGL_BIND_TO_TEXTURE_RGB        0x3039
#define EGL_BIND_TO_TEXTURE_RGBA       0x303A
#define EGL_MIN_SWAP_INTERVAL          0x303B
#define EGL_MAX_SWAP_INTERVAL          0x303C

/* --- EGL Config Attribute Values & Masks ----------------------------------- */
#define EGL_SLOW_CONFIG                0x3050
#define EGL_NON_CONFORMANT_CONFIG      0x3051
#define EGL_TRANSPARENT_RGB            0x3052
#define EGL_NO_TEXTURE                 0x305C
#define EGL_TEXTURE_RGB                0x305D
#define EGL_TEXTURE_RGBA               0x305E
#define EGL_TEXTURE_2D                 0x305F

#define EGL_PBUFFER_BIT                0x01
#define EGL_PIXMAP_BIT                 0x02
#define EGL_WINDOW_BIT                 0x04

/* --- EGL QueryString Targets ----------------------------------------------- */
#define EGL_VENDOR                     0x3053
#define EGL_VERSION                    0x3054
#define EGL_EXTENSIONS                 0x3055

/* --- EGL Surface Attributes & Targets -------------------------------------- */
#define EGL_HEIGHT                     0x3056
#define EGL_WIDTH                      0x3057
#define EGL_LARGEST_PBUFFER            0x3058
#define EGL_DRAW                       0x3059
#define EGL_READ                       0x305A
#define EGL_CORE_NATIVE_ENGINE         0x305B

/* --- EGL 1.1 Render-to-Texture Tokens -------------------------------------- */
#define EGL_TEXTURE_FORMAT             0x3080
#define EGL_TEXTURE_TARGET             0x3081
#define EGL_MIPMAP_TEXTURE             0x3082
#define EGL_MIPMAP_LEVEL               0x3083
#define EGL_BACK_BUFFER                0x3084

/* --- EGL 1.1 Entry Points -------------------------------------------------- */
EGLAPI EGLint      EGLAPIENTRY eglGetError(void);

EGLAPI EGLDisplay  EGLAPIENTRY eglGetDisplay(NativeDisplayType display_id);
EGLAPI EGLBoolean  EGLAPIENTRY eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor);
EGLAPI EGLBoolean  EGLAPIENTRY eglTerminate(EGLDisplay dpy);
EGLAPI const char *EGLAPIENTRY eglQueryString(EGLDisplay dpy, EGLint name);
EGLAPI void      (*EGLAPIENTRY eglGetProcAddress(const char *procname))(void);

EGLAPI EGLBoolean  EGLAPIENTRY eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLAPI EGLBoolean  EGLAPIENTRY eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLAPI EGLBoolean  EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);

EGLAPI EGLSurface  EGLAPIENTRY eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, NativeWindowType win, const EGLint *attrib_list);
EGLAPI EGLSurface  EGLAPIENTRY eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, NativePixmapType pixmap, const EGLint *attrib_list);
EGLAPI EGLSurface  EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
EGLAPI EGLBoolean  EGLAPIENTRY eglDestroySurface(EGLDisplay dpy, EGLSurface surface);
EGLAPI EGLBoolean  EGLAPIENTRY eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);

/* EGL 1.1 render-to-texture & swap control */
EGLAPI EGLBoolean  EGLAPIENTRY eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value);
EGLAPI EGLBoolean  EGLAPIENTRY eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
EGLAPI EGLBoolean  EGLAPIENTRY eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
EGLAPI EGLBoolean  EGLAPIENTRY eglSwapInterval(EGLDisplay dpy, EGLint interval);

EGLAPI EGLContext  EGLAPIENTRY eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_list, const EGLint *attrib_list);
EGLAPI EGLBoolean  EGLAPIENTRY eglDestroyContext(EGLDisplay dpy, EGLContext ctx);
EGLAPI EGLBoolean  EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
EGLAPI EGLContext  EGLAPIENTRY eglGetCurrentContext(void);
EGLAPI EGLSurface  EGLAPIENTRY eglGetCurrentSurface(EGLint readdraw);
EGLAPI EGLDisplay  EGLAPIENTRY eglGetCurrentDisplay(void);
EGLAPI EGLBoolean  EGLAPIENTRY eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value);

EGLAPI EGLBoolean  EGLAPIENTRY eglWaitGL(void);
EGLAPI EGLBoolean  EGLAPIENTRY eglWaitNative(EGLint engine);
EGLAPI EGLBoolean  EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);
EGLAPI EGLBoolean  EGLAPIENTRY eglCopyBuffers(EGLDisplay dpy, EGLSurface surface, NativePixmapType target);

/* Hosted display mode & Gerbera statistics exports */
EGLAPI void        EGLAPIENTRY egliSetDefaultDisplayMode(int displayMode);
EGLAPI int         EGLAPIENTRY grGetStatistic(int stat);
EGLAPI void        EGLAPIENTRY grResetStatistics(void);

#ifdef __cplusplus
}
#endif

#endif /* __egl_h_ */
