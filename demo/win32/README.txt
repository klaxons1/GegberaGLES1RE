Gerbera OpenGL ES 1.1 + EGL 1.1 - Win32 (x86) Release Package
=============================================================

Contents
--------
  libGLES_CM.dll    Reconstructed Nokia / Hybrid Graphics Gerbera OpenGL ES 1.1
                    Common Profile + EGL 1.1 software driver (32-bit Win32 PE32,
                    -O3 release build, 190 exported symbols). Pixel
                    pipelines are compiled by the HC JIT to native x86 code
                    (HCX86Backend) and executed from VirtualAlloc'd
                    PAGE_EXECUTE_READWRITE memory, as in the original driver.
  libGLES_CM.lib    Win32 COFF import library for linking C/C++ applications.
  libGLES_CM.def    PE module-definition file (ordinals @1..@177 + hosted helpers).
  include/          Khronos <GLES/gl.h>, <GLES/egl.h>, and <EGL/egl.h> headers.
  ClassiCube.exe    ClassiCube 1.4.0 (32-bit Win32 GUI release build) compiled
                    with CC_GFX_BACKEND_GL1 + CC_BUILD_GLES and dynamically
                    linked solely against libGLES_CM.dll (no OPENGL32.dll/D3D).
  texpacks/         Pre-bundled CC0 default.zip texture pack for offline play.

Running ClassiCube on Windows (x86 / x64)
-----------------------------------------
1. Keep ClassiCube.exe, libGLES_CM.dll, and texpacks\ in the same folder.
2. Run ClassiCube.exe (or "ClassiCube.exe --singleplayer" to jump straight in).
3. In game, open chat (T) and run:
       /client gpuinfo
   to verify live driver strings from libGLES_CM.dll:
       Vendor: klaxons
       Renderer: 1.1.0
       GL version: OpenGL ES-CM 1.1
       EGL: 1.1 (Hybrid Graphics, Ltd. - Gerbera libGLES_CM.dll Win32)

Linking Your Own Win32 Apps Against libGLES_CM.dll
--------------------------------------------------
Compile with -DGERBERA_DLL_IMPORTS -Iinclude and link against libGLES_CM.lib.

Known issue (JIT)
-----------------
The x86 JIT's register allocator does not yet match the original driver
when it runs out of registers (it spills a different value). Most pixel
programs render correctly, but one stencil-mask case in the host test suite
draws nothing, so some stencil/blend effects may render wrong.
