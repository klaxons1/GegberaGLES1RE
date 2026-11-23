# Gerbera GLES1 RE

Reverse engineering of `libGLES_CM.dll` - the OpenGL ES 1.1 Common Profile
driver shipped on Symbian devices (Nokia / Hybrid Graphics "Gerbera" stack,
cross-referenced across both the WINSCW x86 emulator PE DLL and the EKA2 ARM32
device image `libgles_cm.dll.epoc`).

What is in this repository:

| path             | contents                                                          |
|------------------|-------------------------------------------------------------------|
| `docs/`          | x86 (`libGLES_CM.dll`) + ARM (`libgles_cm.dll.epoc`) binaries & decompilations (input) |
| `tools/`         | scripts that split and attribute the decompilation                |
| `generated/`     | tool output (fragments + manifest), regenerable, not committed    |
| `src/`           | clean, compiling C reconstruction of the driver, module by module |
| `tests/`         | unit tests for the ported code                                    |
| `AGENTS.md`      | how to work on this repo (agent guide)                            |
| `PROGRESS.md`    | what is ported, what is not, what is next                         |
| `docs/analysis.md`| reverse engineering findings: original tree, module map, artifacts|

Build and test:

```
make test      # builds src/ and runs the unit tests
```

Status: `hg` (295 checks), `imath` (311 checks), `gerbera/core` + `grr`
rasterizer + the **complete `gerbera/hc` JIT compiler** (2,327 core checks),
and the **complete `gerbera/api_opengl` OpenGL ES 1.1 Common Profile + OES
layer** (all 10 TUs and all 158 `gl*` entry points; 405 opengl checks incl.
end-to-end rendering) are ported and green (`3,338` total checks, 0 failures,
0 warnings, 0 ASan errors across 62 `.c` files). Next up: `egl`. See
`PROGRESS.md`.
