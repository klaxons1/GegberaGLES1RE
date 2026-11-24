#!/usr/bin/env python3
"""
Symbian OS v9.x (EKA2 / S60 3rd Ed+) .dso import
library generator using elf2e32 --targettype=IMPLIB.

The table specifies imported ordinals and link names for version 10.0
({000a0000}) of the EKA2 platform DLLs.
"""
import os
import subprocess
import sys

IMPLIBS = [
    (
        "euser{000a0000}.dso",
        "euser{000a0000}[100039e5].dll",
        {
            32: "sym_RAllocator_Close",
            46: "sym_RArrayBase_Close",
            48: "sym_RArrayBase_Append",
            53: "sym_RArrayBase_HeapSort",
            59: "sym_RArrayBase_Ctor",
            120: "sym_RHandleBase_Close",
            593: "sym_User_LeaveIfError",
            644: "sym_User_Free",
            646: "sym_User_Alloc",
            652: "sym_User_AllocZ",
            654: "sym_User_AllocZL",
            667: "sym_User_IMB_Range",
            669: "sym_User_Invariant",
            905: "sym_RChunk_CreateLocalCode",
            1210: "sym_UserSvr_DllFreeTls",
            1363: "sym_UserHeap_ChunkHeap",
            1507: "sym_RArrayBase_At",
            1652: "sym_TRect_IsEmpty",
            1702: "sym_RChunk_Base",
            1703: "sym_RChunk_Size",
            1951: "sym_memclr",
            1952: "sym_memcompare",
            1954: "sym_memmove",
            1956: "sym_wordmove",
            2060: "sym_User_MarkCleanupStack",
            2061: "sym_User_UnMarkCleanupStack",
            2082: "sym_XLeaveException_GetReason",
            2176: "sym_UserSvr_DllTls",
            2177: "sym_UserSvr_DllSetTls",
        },
    ),
    (
        "drtaeabi{000a0000}.dso",
        "drtaeabi{000a0000}.dll",
        {
            158: "__aeabi_unwind_cpp_pr0",
            159: "__aeabi_unwind_cpp_pr1",
            160: "__aeabi_unwind_cpp_pr2",
            180: "__cxa_begin_catch",
            183: "__cxa_end_catch",
            184: "__cxa_end_cleanup",
            206: "_ZTI15XLeaveException",
        },
    ),
    (
        "ws32{000a0000}.dso",
        "ws32{000a0000}[10003b21].dll",
        {
            224: "sym_RWindowTreeNode_Close",
            264: "sym_RWindow_Invalidate",
            266: "sym_RWindow_BeginRedraw",
            275: "sym_RWindow_Construct",
            276: "sym_RWindow_EndRedraw",
            280: "sym_RWindow_Ctor",
            352: "sym_CWindowGc_Ctor",
            358: "sym_CWsBitmap_Reset",
            359: "sym_CWsBitmap_Create",
            363: "sym_CWsBitmap_Ctor",
            384: "sym_RWsSession_GetDefModeMaxNumColors",
            387: "sym_RWindowBase_Size",
            490: "sym_RDrawableWindow_GetDrawRect",
        },
    ),
    (
        "fbscli{000a0000}.dso",
        "fbscli{000a0000}[10003a15].dll",
        {
            14: "sym_CFbsBitmap_ScanLineLength",
            25: "sym_CFbsBitmap_Reset",
            27: "sym_CFbsBitmap_Resize",
            107: "sym_CFbsBitmap_UnlockHeap",
            108: "sym_CFbsBitmap_DataAddress",
            109: "sym_CFbsBitmap_DisplayMode",
            117: "sym_CFbsBitmap_SizeInPixels",
            133: "sym_CFbsBitmap_LockHeap",
        },
    ),
    (
        "cone{000a0000}.dso",
        "cone{000a0000}[10003a41].dll",
        {
            136: "sym_CCoeEnv_ConstructL",
            182: "sym_CCoeEnv_Static",
            192: "sym_CCoeEnv_Ctor",
        },
    ),
    (
        "gdi{000a0000}.dso",
        "gdi{000a0000}[10003b15].dll",
        {
            98: "sym_TDisplayModeUtils_NumDisplayModeBitsPerPixel",
        },
    ),
    (
        "hal{000a0000}.dso",
        "hal{000a0000}[100039e8].dll",
        {
            1: "sym_HAL_Get",
        },
    ),
]


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit(f"Usage: {sys.argv[0]} <elf2e32_bin> <out_dir>")

    elf2e32 = sys.argv[1]
    out_dir = sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)

    for dso_name, linkas, ordinals in IMPLIBS:
        def_path = os.path.join(out_dir, dso_name.replace(".dso", ".def"))
        dso_path = os.path.join(out_dir, dso_name)
        max_ord = max(ordinals)
        with open(def_path, "w", encoding="utf-8") as f:
            f.write("EXPORTS\n")
            for i in range(1, max_ord + 1):
                if i in ordinals:
                    kind = " DATA 8" if ordinals[i] == "_ZTI15XLeaveException" else ""
                    f.write(f"\t{ordinals[i]} @ {i} NONAME{kind}\n")
                else:
                    f.write(f"\t_unused_{dso_name.split('{')[0]}_{i} @ {i} NONAME ABSENT\n")

        subprocess.check_call(
            [
                elf2e32,
                "--targettype=IMPLIB",
                f"--definput={def_path}",
                f"--dso={dso_path}",
                f"--linkas={linkas}",
            ]
        )


if __name__ == "__main__":
    main()
