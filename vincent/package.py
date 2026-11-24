"""Package the native DLL with its license, build notes and SDK import files."""
import pathlib
import zipfile
root = pathlib.Path(__file__).resolve().parent
with zipfile.ZipFile(root / "out/Vincent-GLES1-Symbian-EKA2-ARMv5.zip", "w", zipfile.ZIP_DEFLATED) as z:
    for file in ("LICENSE", "README.md", "UPSTREAM_REVISION", "THIRD_PARTY_NOTICES.txt"):
        z.write(root / file, file)
    z.write(root / "out/libgles_cm.dll", "sys/bin/libgles_cm.dll")
    for name in ("libgles_cm.dso", "libgles_cm{000a0000}.dso"):
        z.write(root / "out" / name, "epoc32/release/armv5/lib/" + name)
    z.write(root / "out/libgles_cm.def", "epoc32/include/def/eabi/libgles_cmU.def")
    for file in (root / "upstream/include/GLES").glob("*.h"):
        z.write(file, "epoc32/include/GLES/" + file.name)
