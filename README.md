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

Status: `hg` (295 checks), `imath` scalar/vector/matrix layers (311 checks),
`gerbera/core` + `grr` rasterizer, and the **complete `gerbera/hc` JIT
compiler** (`hcCompiler`, CFG/dominators/liveness/SSA-optimizations,
graph-coloring & panic register allocators, interpreter, and BOTH x86 & ARM32
JIT backends; 2,254 core checks) are ported and green (`2,860` total checks,
0 failures, 0 warnings across `src/`). Next up: `api_opengl` and `egl`. See
`PROGRESS.md`.
