# CC3DBench in an ARM emulator

`symbian/emu` runs the CC3DBench GL scenes (`docs/CC3DBench.7z`) on the
Gerbera core inside unicorn. The core is compiled with the Symbian DLL's
compiler and flags. The harness counts the ARM instructions of every frame
and records each frame's colour and depth CRC. Two builds or configurations
can therefore be compared for both speed and bit-exactness, without a phone.

Per-scene instruction counts track the E72 RGB565 benchmark log closely:
Pearson r = 0.989, phone_ms ≈ 12.5 + 3.02 × Minstr per frame.

## Files

| File | What it does |
|---|---|
| `build_emu.sh` | Builds `build/emu/cc3d_emu_ccsoft{0,1}.elf`. Needs `/tmp/symbian_tools` (fetched by `symbian/build_symbian.sh`) and zig, which provides compiler-rt soft-float and `ld.lld`. `GERBERA_CCSOFT=1` (the default) adds the C spans plus a run-time toggle. |
| `run_emu.py` | Runs an image and prints per-scene JIT and span primitive counts. `--json` writes per-frame instructions, CRCs and a function profile. |
| `compare.py` | Compares two `--json` results scene by scene (CRCs and speed-up). Exits 1 on any CRC difference. |
| `hotspot.py` | Instruction-level hot spots of one scene, either in the JIT heap or in one function, with disassembly. |
| `cc3d_gl.c`, `cc3d/` | The bench driver and scene suite, ported from CC3DBench. |
| `emu_rt.c`, `start.S`, `emu.ld` | Bare-metal runtime for the emulator. |

The Python tools need `unicorn`, `capstone` and `pyelftools`.

## Typical session

```sh
GERBERA_CCSOFT=1 ./symbian/emu/build_emu.sh
E=build/emu/cc3d_emu_ccsoft1.elf
python symbian/emu/run_emu.py $E --frames 4 --ccsoft 0 --json /tmp/jit.json
python symbian/emu/run_emu.py $E --frames 4 --ccsoft 1 --json /tmp/cc.json
python symbian/emu/compare.py /tmp/jit.json /tmp/cc.json
python symbian/emu/hotspot.py $E --scene 23 --ccsoft 0 jit
```

`--dither 1` turns on GL_DITHER in every scene.

## CCsoft spans (GERBERA_CCSOFT) vs the JIT

Measured with 73 scenes at 4 frames, comparing mean instructions per frame:

- **Dither off:** all CRCs equal, 796M → 608M (1.31×).
- **Dither on:** all CRCs equal, 888M → 713M (1.24×).

Typical results:

| Kind of scene | Speed-up |
|---|---|
| Fill-bound scenes (overdraw, Z, textured quads, sprites, alpha) | 1.5–5.4× |
| Fog | 1.25× |
| Geometry-bound scenes (lighting, tiny triangles, points) | 0.95–1.05× (per-vertex work dominates) |
| States left to the JIT (texenv ADD, linear filtering, clamp-to-edge, two textures) | 1.0× |
