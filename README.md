# Gerbera GLES1 RE

Reverse engineering of `libGLES_CM.dll` - the OpenGL ES 1.1 Common Profile
driver shipped on Symbian devices (Nokia / Hybrid Graphics "Gerbera" stack,
WINSCW emulator build, x86).

What is in this repository:

| path             | contents                                                          |
|------------------|-------------------------------------------------------------------|
| `docs/`          | the reference binary and the raw Hex-Rays decompilation (input)   |
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

Status: phase 1 - the `hg` support library is fully ported and tested.
See `PROGRESS.md`.
