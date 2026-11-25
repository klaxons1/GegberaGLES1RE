#!/usr/bin/env python3
"""Instruction-level hot spots of one CC3D emu scene.

Counts every executed instruction in either the JIT heap ("jit") or a named
function, then prints the hottest instructions with capstone disassembly in
address order, so loop bodies read naturally:

    hotspot.py build/emu/cc3d_emu_ccsoft1.elf --scene 23 --ccsoft 0 jit
    hotspot.py build/emu/cc3d_emu_ccsoft1.elf --scene 23 --ccsoft 1 ccSpan_0200000
"""
import argparse
import os
import sys

from capstone import CS_ARCH_ARM, CS_MODE_ARM, Cs
from unicorn import UC_HOOK_CODE

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run_emu  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("target", help="'jit' or a function symbol")
    ap.add_argument("--scene", type=int, required=True)
    ap.add_argument("--frames", type=int, default=2)
    ap.add_argument("--ccsoft", type=int, default=1)
    ap.add_argument("--dither", type=int, default=0)
    ap.add_argument("--min", type=float, default=0.002, help="hide instructions below this share")
    args = ap.parse_args()

    runner = run_emu.Runner(args.image, 10000)
    runner.patch_config(frames=args.frames, firstScene=args.scene, lastScene=args.scene,
                        ccsoft=args.ccsoft, dumpFrame=-1, dither=args.dither)
    if args.target == "jit":
        begin, end = runner.heap_start, runner.stack_top
    else:
        match = [f for f in runner.funcs if f[2] == args.target]
        if not match:
            sys.exit("no function %s" % args.target)
        begin, end = match[0][0], match[0][0] + match[0][1]
    counts = {}

    def on_code(uc, address, size, user):
        counts[address] = counts.get(address, 0) + 1

    runner.uc.hook_add(UC_HOOK_CODE, on_code, begin=begin, end=end - 1)
    runner.run(2e10)
    total = sum(counts.values())
    print("%s: %d instructions executed in %#x-%#x" % (args.target, total, begin, end))
    if not total:
        return 0
    md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
    last = None
    for address in sorted(counts):
        n = counts[address]
        if n < args.min * total:
            continue
        if last is not None and address != last + 4:
            print("  ...")
        code = bytes(runner.uc.mem_read(address, 4))
        insn = next(md.disasm(code, address), None)
        text = "%s %s" % (insn.mnemonic, insn.op_str) if insn else code.hex()
        print("%8x %10d %5.1f%%  %s" % (address, n, 100.0 * n / total, text))
        last = address
    return 0


if __name__ == "__main__":
    sys.exit(main())
