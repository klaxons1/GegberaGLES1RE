#!/usr/bin/env python3
"""run_emu.py - run a symbian/emu/build_emu.sh image in unicorn (ARM).

Counts executed ARM instructions per benchmark frame (resolution: --chunk
instructions), samples the PC at every chunk boundary for a profile, and
records the colour+depth CRC of every frame so two builds/configurations can
be compared pixel for pixel.

  run_emu.py IMAGE.elf [--frames N] [--scenes A-B] [--ccsoft 0|1]
             [--dump-frame F --dump-dir DIR] [--json OUT.json]
"""
import argparse
import bisect
import json
import struct
import sys
import time
import zlib

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UcError, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_INTR, UC_HOOK_MEM_UNMAPPED
from unicorn.arm_const import UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_CPSR

MARK_SCENE_BEGIN, MARK_FRAME_BEGIN, MARK_FRAME_END, MARK_CHECKSUM, MARK_PIPE_BEGIN, MARK_PIPE_END = 1, 2, 3, 4, 5, 6


def write_png(path, pixels565, width, height):
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            c = pixels565[y * width + x]
            r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
            rows += bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(bytes(rows), 6)) + chunk(b"IEND", b""))


class Runner:
    def __init__(self, image, chunk):
        self.chunk = chunk
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        with open(image, "rb") as f:
            elf = ELFFile(f)
            self.symbols = {}
            funcs = []
            for sym in elf.get_section_by_name(".symtab").iter_symbols():
                if sym.name:
                    self.symbols.setdefault(sym.name, sym["st_value"])
                    if sym["st_info"]["type"] == "STT_FUNC" and sym["st_size"] > 0:
                        funcs.append((sym["st_value"] & ~1, sym["st_size"], sym.name))
            funcs.sort()
            self.func_addr = [a for a, _, _ in funcs]
            self.funcs = funcs
            top = self.symbols["__stack_top"]
            self.text_end = self.symbols.get("__bss_start", top)
            self.uc.mem_map(0x10000, ((top + 0xFFFF) & ~0xFFFF) - 0x10000)
            for seg in elf.iter_segments():
                if seg["p_type"] == "PT_LOAD" and seg["p_filesz"]:
                    self.uc.mem_write(seg["p_vaddr"], seg.data())
            self.entry = elf.header["e_entry"]
            self.stack_top = top
        self.heap_start = self.symbols["__heap_start"]
        self.executed = 0
        self.events = []
        self.output = []
        self.exit_code = None
        self.frames = {}
        self.dump_dir = None
        self.samples = []
        self.uc.hook_add(UC_HOOK_INTR, self.on_interrupt)
        self.uc.hook_add(UC_HOOK_MEM_UNMAPPED, self.on_unmapped)

    def where(self, pc):
        i = bisect.bisect_right(self.func_addr, pc) - 1
        if i >= 0:
            a, size, name = self.funcs[i]
            if pc < a + size:
                return name
        return "JIT code" if pc >= self.heap_start else "?%x" % pc

    def on_unmapped(self, uc, access, address, size, value, user):
        pc = uc.reg_read(UC_ARM_REG_PC)
        print("unmapped access %#x at pc %#x (%s)" % (address, pc, self.where(pc)), file=sys.stderr)
        return False

    def on_interrupt(self, uc, intno, user):
        pc = uc.reg_read(UC_ARM_REG_PC)
        number = struct.unpack("<I", uc.mem_read(pc - 4, 4))[0] & 0xFFFFFF
        r0, r1, r2, r3 = (uc.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3))
        if number == 1:
            text = bytes(uc.mem_read(r0, r1)).decode("latin1")
            self.output.append(text)
            sys.stdout.write(text)
            sys.stdout.flush()
        elif number == 2:
            self.exit_code = r0
            uc.emu_stop()
        elif number == 3:
            self.events.append((r0, r1 - (1 << 32) if r1 & 0x80000000 else r1, r2, self.executed))
        elif number == 5 and self.dump_dir:
            data = bytes(uc.mem_read(r0, r1 * r2 * 2))
            write_png("%s/scene%02d.png" % (self.dump_dir, r3), struct.unpack("<%dH" % (r1 * r2), data), r1, r2)

    def patch_config(self, **fields):
        base = self.symbols["emu_config"]
        order = ["magic", "frames", "firstScene", "lastScene", "ccsoft", "dumpFrame", "dither"]
        magic = struct.unpack("<I", self.uc.mem_read(base, 4))[0]
        assert magic == 0xCC3D0001, hex(magic)
        for name, value in fields.items():
            self.uc.mem_write(base + 4 * order.index(name), struct.pack("<i", value))

    def run(self, budget):
        pc = self.entry
        from unicorn.arm_const import UC_ARM_REG_SP
        self.uc.reg_write(UC_ARM_REG_SP, self.stack_top)
        while self.exit_code is None and self.executed < budget:
            try:
                self.uc.emu_start(pc, 0xFFFFFFF0, count=self.chunk)
            except UcError as e:
                pc = self.uc.reg_read(UC_ARM_REG_PC)
                raise RuntimeError("emulation fault %s at pc %#x (%s)" % (e, pc, self.where(pc)))
            pc = self.uc.reg_read(UC_ARM_REG_PC)
            if self.uc.reg_read(UC_ARM_REG_CPSR) & 0x20:
                pc |= 1
            self.executed += self.chunk
            self.samples.append((self.executed, pc & ~1))


def summarize(runner):
    scenes = {}
    current = {}
    for kind, a, b, at in runner.events:
        if kind == MARK_FRAME_BEGIN:
            current = {"scene": a, "frame": b, "begin": at}
        elif kind == MARK_PIPE_END:
            current["crc_begin"] = at
        elif kind == MARK_PIPE_BEGIN:
            current["crc_end"] = at
        elif kind == MARK_FRAME_END:
            cost = at - current["begin"] - (current["crc_end"] - current["crc_begin"])
            scenes.setdefault(a, {"frames": []})["frames"].append({"frame": b, "instructions": cost,
                                                                   "window": (current["begin"], at),
                                                                   "crc_window": (current["crc_begin"], current["crc_end"])})
        elif kind == MARK_CHECKSUM:
            scene, frame = a >> 8, a & 255
            scenes[scene]["frames"][frame]["crc"] = b & 0xFFFFFFFF
    # profile: samples inside measured frames (excluding frame 0, which compiles pipelines)
    for scene, info in scenes.items():
        prof = {}
        windows = [(f["window"], f["crc_window"]) for f in info["frames"] if f["frame"] > 0]
        for at, pc in runner.samples:
            for (b, e), (cb, ce) in windows:
                if b < at <= e and not (cb < at <= ce):
                    name = runner.where(pc)
                    prof[name] = prof.get(name, 0) + 1
                    break
        info["profile"] = sorted(prof.items(), key=lambda kv: -kv[1])
    return scenes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--frames", type=int, default=16)
    ap.add_argument("--scenes", default="0-72")
    ap.add_argument("--ccsoft", type=int, default=1)
    ap.add_argument("--chunk", type=int, default=10000)
    ap.add_argument("--budget", type=float, default=2e11)
    ap.add_argument("--dither", type=int, default=0)
    ap.add_argument("--dump-frame", type=int, default=-1)
    ap.add_argument("--dump-dir")
    ap.add_argument("--json")
    args = ap.parse_args()
    first, _, last = args.scenes.partition("-")
    runner = Runner(args.image, args.chunk)
    runner.dump_dir = args.dump_dir
    runner.patch_config(frames=args.frames, firstScene=int(first), lastScene=int(last or first),
                        ccsoft=args.ccsoft, dumpFrame=args.dump_frame, dither=args.dither)
    start = time.time()
    runner.run(args.budget)
    elapsed = time.time() - start
    scenes = summarize(runner)
    print("exit %s, %.2f G instructions in %.1f s" % (runner.exit_code, runner.executed / 1e9, elapsed), file=sys.stderr)
    if args.json:
        with open(args.json, "w") as f:
            json.dump({"exit": runner.exit_code, "chunk": args.chunk, "ccsoft": args.ccsoft,
                       "scenes": {str(k): v for k, v in scenes.items()}}, f)
    return 0 if runner.exit_code == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
