# Gerbera GLES1 RE

Complete C99 source reconstruction of `libGLES_CM.dll` - the OpenGL ES 1.1
Common Profile + EGL 1.1 software driver shipped on Symbian devices (Nokia /
Hybrid Graphics "Gerbera" stack, cross-referenced across both the WINSCW x86
emulator PE DLL and the EKA2 ARM32 device image `libgles_cm.dll.epoc`).

What is in this repository:

| path              | contents                                                          |
|-------------------|-------------------------------------------------------------------|
| `docs/`           | x86 (`libGLES_CM.dll`) + ARM (`libgles_cm.dll.epoc`) binaries & decompilations (input) |
| `tools/`          | scripts that split and attribute the decompilation                |
| `generated/`      | tool output (fragments + manifest), regenerable, not committed    |
| `src/`            | complete C99 reconstruction (`hg`, `imath`, `gerbera/core`, `grr`, `hc`, `api_opengl`, `egl`) |
| `tests/`          | automated unit & integration test suites (`test_hg`, `test_imath`, `test_core`, `test_opengl`, `test_egl`) |
| `AGENTS.md`       | how to work on this repo (agent guide)                            |
| `PROGRESS.md`     | module-by-module reconstruction log & verification status         |
| `docs/analysis.md`| reverse engineering findings: original tree, module map, artifacts|

Build and test:

```
make test      # builds build/libgerbera.a (66 .c files) and runs all 5 test suites
```

Status: **100% of all modules (`hg`, `imath`, `gerbera/core`, `grr`, `hc`,
`api_opengl`, `egl`, and `egl_symbian` hosted backend) and 177 / 177 non-CRT PE
ordinal exports of `libGLES_CM.dll` are ported and verified**:
- `hg`: 295 checks
- `imath`: 311 checks
- `gerbera/core` + `grr` + `hc` JIT compiler (x86 & ARM32 backends): 2,327 checks
- `gerbera/api_opengl` (158 `gl*` OpenGL ES 1.1 + OES entry points in `<GLES/gl.h>`): 405 checks
- `gerbera/egl` + `eglSymbian` (27 `egl*` EGL 1.1 entry points in `<GLES/egl.h>` & `<EGL/egl.h>`, incl. render-to-texture): 1,075 checks
- Total: **4,413 / 4,413 checks passing**, `0` failures, `0` compiler warnings, `0` AddressSanitizer / LeakSanitizer errors across 66 `.c` files. See `PROGRESS.md`.
