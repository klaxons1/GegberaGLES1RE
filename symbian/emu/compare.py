#!/usr/bin/env python3
"""Compare two run_emu.py --json results scene by scene.

Prints, per CC3D scene, whether every frame checksum matches and the mean
instructions per measured frame (frame 0 compiles pipelines and is skipped),
then the totals. Exits 1 when any scene's checksums differ.

    compare.py /tmp/jit.json /tmp/ccsoft.json
"""
import json
import os
import sys


def scene_names():
    suite = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cc3d", "suite.c")
    with open(suite) as f:
        table = f.read().split("suite_scenes[SUITE_COUNT] = {")[1].split("\n")[1:]
    return [line.split('"')[1] for line in table if '"' in line]


def mean_instructions(frames):
    measured = [f["instructions"] for f in frames if f["frame"] > 0] or [f["instructions"] for f in frames]
    return sum(measured) / len(measured)


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: compare.py BASE.json NEW.json")
    with open(sys.argv[1]) as f:
        base = json.load(f)["scenes"]
    with open(sys.argv[2]) as f:
        new = json.load(f)["scenes"]
    names = scene_names()
    bad = 0
    total_base = total_new = 0.0
    for scene in sorted(set(base) & set(new), key=int):
        fa, fb = base[scene]["frames"], new[scene]["frames"]
        same = [f.get("crc") for f in fa] == [f.get("crc") for f in fb]
        bad += not same
        ia, ib = mean_instructions(fa), mean_instructions(fb)
        total_base += ia
        total_new += ib
        name = names[int(scene)] if int(scene) < len(names) else "?"
        print("%2s %-8s %s  %7.2fM -> %7.2fM  x%.2f" % (scene, name, "OK  " if same else "DIFF",
                                                     ia / 1e6, ib / 1e6, ia / ib))
    print("mismatching scenes: %d  total %.0fM -> %.0fM (x%.2f)"
          % (bad, total_base / 1e6, total_new / 1e6, total_base / total_new))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
