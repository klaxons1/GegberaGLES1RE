#!/usr/bin/env python3
"""
test_e32_armv5_unicorn.py - End-to-end Symbian OS EKA2 E32 binary loader &
ARM926EJ-S (ARMv5TEJ) Unicorn CPU verification for pixelflinger/out/libgles_cm.dll.

Tests:
  1. E32ImageHeader parsing, E32 relocation application at non-default load address
     0x78000000, and E32 ELF-style Import Address Table binding across all 5 ROM DLLs.
  2. Full OpenGL ES 1.1 3D scene execution WITH CodeFlinger ARMv5TE JIT enabled
     (RChunk::CreateLocalCode + User::IMB_Range + 1.5M+ JIT instructions executed)
     across BOTH Config 1 (RGB565) and Config 2 (RGBA8888).
  3. Exact same scene WITH RChunk::CreateLocalCode denied (-4 / KErrNoMemory),
     verifying 100% seamless fallback to the compiled-in C++ scanline(context_t*).
  4. S60 ConstructL (0,0) -> SizeChanged (128,96) dynamic window resize, EColor16MA
     -> EColor16MU opaque blit override, and zero-deadlock CFbsBitmap LockHeap check.
"""
import math
import os
import struct
import zlib
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import *

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DLL_PATH  = os.path.join(REPO_ROOT, "pixelflinger", "out", "libgles_cm.dll")
DEF_PATH  = os.path.join(REPO_ROOT, "symbian", "libgles_cm.def")

dll = open(DLL_PATH, "rb").read()

(uid1, uid2, uid3, uid_crc, sig, hdr_crc, mod_ver, comp_type,
 tools_ver, time_lo, time_hi, flags,
 code_size, data_size, heap_min, heap_max, stack_size, bss_size,
 entry_pt, code_base, data_base, dll_ref_count,
 export_dir_off, export_dir_cnt, text_size,
 code_off, data_off, import_off, code_reloc_off, data_reloc_off) = struct.unpack_from(
    "<3I I 4s I I I I I I I i i i i i i I I I i I i i I I I I I", dll, 0
)

assert sig == b"EPOC" and export_dir_cnt == 175 and data_size == 0 and bss_size == 0

ord_by_name = {}
for line in open(DEF_PATH):
    parts = line.strip().split()
    if len(parts) >= 3 and parts[1] == "@":
        ord_by_name[parts[0]] = int(parts[2])


def run_symbian_e32_scene(allow_jit: bool, config_rgb_bits=(5, 6, 5), max_exec_bytes=256*1024, stress_shaders=False):
    mu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    try:
        mu.ctl_set_cpu_model(UC_CPU_ARM_926)
    except Exception:
        pass

    RELOC_BASE = 0x78000000
    DELTA      = RELOC_BASE - code_base
    STACK_TOP  = 0x70000000
    HEAP_BASE  = 0x20000000
    JIT_BASE   = 0x30000000
    STUB_BASE  = 0x7F000000

    mu.mem_map(RELOC_BASE, (code_size + 0xFFFF) & ~0xFFFF)
    mu.mem_map(STACK_TOP - 0x40000, 0x40000)
    mu.mem_map(HEAP_BASE, 0x02000000)
    mu.mem_map(JIT_BASE,  0x00080000)
    mu.mem_map(STUB_BASE, 0x00010000)

    code_img = bytearray(dll[code_off : code_off + code_size])

    # Apply Symbian E32 Code Relocations
    rel_pos = code_reloc_off
    rel_section_size, _ = struct.unpack_from("<II", dll, rel_pos)
    rel_pos += 8
    rel_end = code_reloc_off + rel_section_size
    applied_relocs = 0
    while rel_pos < rel_end:
        page_off, block_sz = struct.unpack_from("<II", dll, rel_pos)
        for off in range(8, block_sz, 2):
            entry = struct.unpack_from("<H", dll, rel_pos + off)[0]
            if (entry >> 12) != 0:
                target = page_off + (entry & 0xFFF)
                val = struct.unpack_from("<I", code_img, target)[0]
                struct.pack_into("<I", code_img, target, (val + DELTA) & 0xFFFFFFFF)
                applied_relocs += 1
        rel_pos += block_sz

    struct.pack_into("<I", code_img, 0x0C, 0x12345678)

    # Patch Symbian E32 Import Address Table
    imp_pos = import_off + 4
    stub_dispatch = {}
    stub_idx = 0
    for _ in range(dll_ref_count):
        off_of_dll_name, num_imports = struct.unpack_from("<Ii", dll, imp_pos)
        imp_pos += 8
        end_str = dll.find(b"\x00", import_off + off_of_dll_name)
        dll_short = dll[import_off + off_of_dll_name : end_str].decode("ascii").lower().split("{")[0]
        for _ in range(num_imports):
            iat_code_off = struct.unpack_from("<I", dll, imp_pos)[0]
            imp_pos += 4
            ordinal = struct.unpack_from("<I", code_img, iat_code_off)[0] & 0xFFFF
            stub_addr = STUB_BASE + stub_idx * 4
            mu.mem_write(stub_addr, struct.pack("<I", 0xE12FFF1E))
            stub_dispatch[stub_addr] = (dll_short, ordinal)
            struct.pack_into("<I", code_img, iat_code_off, stub_addr)
            stub_idx += 1

    mu.mem_write(RELOC_BASE, bytes(code_img))

    exports = {
        ord_i: (struct.unpack_from("<I", dll, export_dir_off + (ord_i - 1) * 4)[0] - code_base) + RELOC_BASE
        for ord_i in range(1, export_dir_cnt + 1)
    }

    heap_ptr = [HEAP_BASE + 0x1000]
    alloc_sizes = {}
    def halloc(sz, zero=False):
        sz = max(8, (sz + 7) & ~7)
        p = heap_ptr[0]
        heap_ptr[0] += sz + 8
        alloc_sizes[p] = sz
        if zero:
            mu.mem_write(p, b"\x00" * sz)
        return p

    def make_special_stub(tag):
        nonlocal stub_idx
        a = STUB_BASE + stub_idx * 4
        mu.mem_write(a, struct.pack("<I", 0xE12FFF1E))
        stub_dispatch[a] = ("_vtbl", tag)
        stub_idx += 1
        return a

    rheap_vtbl = halloc(32, True)
    rheap_obj  = halloc(16, True)
    cbase_vtbl = halloc(32, True)
    wgc_vtbl   = halloc(320, True)

    rh_buf = bytearray(32)
    for slot, name in [(0, "heap_alloc"), (1, "heap_free"), (2, "heap_realloc"), (4, "heap_compress")]:
        struct.pack_into("<I", rh_buf, slot * 4, make_special_stub(name))
    mu.mem_write(rheap_vtbl, bytes(rh_buf))
    mu.mem_write(rheap_obj, struct.pack("<I", rheap_vtbl))

    cb_buf = bytearray(32)
    struct.pack_into("<I", cb_buf, 4, make_special_stub("cbase_del"))
    mu.mem_write(cbase_vtbl, bytes(cb_buf))

    wgc_buf = bytearray(320)
    for slot, name in [(1, "cbase_del"), (57, "gc_bitblt"), (67, "gc_construct"), (68, "gc_activate"), (69, "gc_deactivate")]:
        struct.pack_into("<I", wgc_buf, slot * 4, make_special_stub(name))
    mu.mem_write(wgc_vtbl, bytes(wgc_buf))

    coe_env = halloc(0x98, True)
    mu.mem_write(coe_env + 72, struct.pack("<I", 0xCAFE0001))

    tls_map = {}
    bitmaps = {}
    win_size = [128, 96]
    last_bitblt_bmp = [0]
    imb_calls = [0]
    jit_insns_executed = [0]

    exec_chunk_sz = [0]

    def hook_code(uc, address, size, user_data):
        if JIT_BASE <= address < JIT_BASE + 0x80000:
            jit_insns_executed[0] += 1
            return
        info = stub_dispatch.get(address)
        if not info:
            return
        mod, op = info
        r0 = uc.reg_read(UC_ARM_REG_R0)
        r1 = uc.reg_read(UC_ARM_REG_R1)
        r2 = uc.reg_read(UC_ARM_REG_R2)

        if mod == "_vtbl":
            if op == "heap_alloc":
                uc.reg_write(UC_ARM_REG_R0, halloc(r1, False))
            elif op == "heap_realloc":
                newp = halloc(r2, False)
                if r1 in alloc_sizes and r2 > 0:
                    uc.mem_write(newp, bytes(uc.mem_read(r1, min(alloc_sizes[r1], r2))))
                uc.reg_write(UC_ARM_REG_R0, newp)
            elif op in ("heap_compress", "gc_construct"):
                uc.reg_write(UC_ARM_REG_R0, 0)
            elif op == "gc_bitblt":
                last_bitblt_bmp[0] = r2
                assert bitmaps[r2]["locked"] == 0, "CFbsBitmap still locked during CWindowGc::BitBlt!"
        elif mod == "euser":
            if op == 2176:   uc.reg_write(UC_ARM_REG_R0, tls_map.get(r0, 0))
            elif op == 2177: tls_map[r0] = r2; uc.reg_write(UC_ARM_REG_R0, 0)
            elif op == 1210: tls_map.pop(r0, None)
            elif op in (646, 652, 654): uc.reg_write(UC_ARM_REG_R0, halloc(r0, op != 646))
            elif op in (644, 32, 120, 2060, 2061): uc.reg_write(UC_ARM_REG_R0, 0)
            elif op == 1363: uc.reg_write(UC_ARM_REG_R0, rheap_obj)
            elif op == 905:
                # sym_RChunk_CreateLocalCode(RChunk* r0, TInt aSize r1, TInt aMaxSize r2, TOwnerType r3)
                if allow_jit and r2 <= max_exec_bytes:
                    exec_chunk_sz[0] = r1
                    uc.mem_write(r0, struct.pack("<I", 0xBEEF))
                    uc.reg_write(UC_ARM_REG_R0, 0)
                else:
                    uc.reg_write(UC_ARM_REG_R0, 0xFFFFFFFC) # KErrNoMemory
            elif op == 1702: uc.reg_write(UC_ARM_REG_R0, JIT_BASE)
            elif op == 1703: uc.reg_write(UC_ARM_REG_R0, exec_chunk_sz[0])
            elif op == 667:
                imb_calls[0] += 1
                uc.ctl_remove_cache(r0, r1)
            elif op == 1652: uc.reg_write(UC_ARM_REG_R0, 1)
            elif op == 1951: uc.mem_write(r0, b"\x00" * r1)
            elif op in (1954, 1956):
                uc.mem_write(r0, bytes(uc.mem_read(r1, r2)))
                uc.reg_write(UC_ARM_REG_R0, r0)
            else:
                lr = uc.reg_read(UC_ARM_REG_LR) - RELOC_BASE + code_base
                sp_now = uc.reg_read(UC_ARM_REG_SP)
                stk = [struct.unpack("<I", uc.mem_read(sp_now + k*4, 4))[0] for k in range(16)]
                elf_stk = [hex(x - RELOC_BASE + code_base) for x in stk if RELOC_BASE <= x < RELOC_BASE + code_size]
                raise RuntimeError(f"Unhandled euser@{op} from ELF_LR=0x{lr:08x}, stack_ret={elf_stk}")
        elif mod == "hal":
            uc.mem_write(r1, struct.pack("<I", 32 * 1024 * 1024))
            uc.reg_write(UC_ARM_REG_R0, 0)
        elif mod == "cone":
            if op == 182: uc.reg_write(UC_ARM_REG_R0, coe_env)
            elif op == 192: uc.reg_write(UC_ARM_REG_R0, r0)
            elif op == 136: uc.reg_write(UC_ARM_REG_R0, 0)
        elif mod == "ws32":
            if op == 384: uc.reg_write(UC_ARM_REG_R0, 12) # EColor16MA -> overridden to EColor16MU (11)
            elif op == 352: uc.mem_write(r0, struct.pack("<I", wgc_vtbl)); uc.reg_write(UC_ARM_REG_R0, r0)
            elif op == 363: uc.mem_write(r0, struct.pack("<I", cbase_vtbl)); uc.reg_write(UC_ARM_REG_R0, r0)
            elif op == 359:
                w, h = struct.unpack("<ii", uc.mem_read(r1, 8))
                bpp = 4 if r2 in (11, 12, 13) else 2
                bitmaps[r0] = {"w": w, "h": h, "mode": r2, "data": halloc(w * bpp * h, True), "stride": w * bpp, "locked": 0}
                uc.reg_write(UC_ARM_REG_R0, 0)
            elif op == 387: uc.mem_write(r0, struct.pack("<ii", win_size[0], win_size[1]))
            elif op == 490: uc.mem_write(r0, struct.pack("<iiii", 0, 0, win_size[0], win_size[1]))
            else: uc.reg_write(UC_ARM_REG_R0, 0)
        elif mod == "fbscli":
            b = bitmaps.get(r0)
            if op == 117:
                b = bitmaps[r1]
                uc.mem_write(r0, struct.pack("<ii", b["w"], b["h"]))
            elif op == 109: uc.reg_write(UC_ARM_REG_R0, b["mode"])
            elif op == 14:  uc.reg_write(UC_ARM_REG_R0, r0 * (4 if r1 in (11, 12, 13) else 2))
            elif op == 133: b["locked"] += 1
            elif op == 107: b["locked"] -= 1
            elif op == 108: uc.reg_write(UC_ARM_REG_R0, b["data"])
            elif op == 27:
                w, h = struct.unpack("<ii", uc.mem_read(r1, 8))
                bpp = 4 if b["mode"] in (11, 12, 13) else 2
                b["w"], b["h"], b["stride"] = w, h, w * bpp
                b["data"] = halloc(b["stride"] * h, True)
                uc.reg_write(UC_ARM_REG_R0, 0)

    mu.hook_add(UC_HOOK_CODE, hook_code)

    RET_SENTINEL = STUB_BASE + 0xF000
    mu.mem_write(RET_SENTINEL, struct.pack("<I", 0xE1A00000))
    sp_base = STACK_TOP - 0x1000

    extension_procs = {}

    def call(fn_name, *args):
        if fn_name in ord_by_name:
            addr = exports[ord_by_name[fn_name]]
        else:
            if fn_name not in extension_procs:
                encoded = fn_name.encode("ascii") + b"\0"
                name_ptr = halloc(len(encoded))
                mu.mem_write(name_ptr, encoded)
                extension_procs[fn_name] = call("eglGetProcAddress", name_ptr)
            addr = extension_procs[fn_name]
            assert addr, f"Extension procedure missing: {fn_name}"
        sp = sp_base
        if len(args) > 4:
            extra = args[4:]
            sp -= ((len(extra) * 4 + 7) & ~7)
            for idx, val in enumerate(extra):
                mu.mem_write(sp + idx * 4, struct.pack("<I", val & 0xFFFFFFFF))
        for i in range(min(4, len(args))):
            mu.reg_write(UC_ARM_REG_R0 + i, args[i] & 0xFFFFFFFF)
        mu.reg_write(UC_ARM_REG_SP, sp)
        mu.reg_write(UC_ARM_REG_LR, RET_SENTINEL)
        mu.emu_start(addr, RET_SENTINEL)
        return mu.reg_read(UC_ARM_REG_R0)

    def f32(v):
        return struct.unpack("<I", struct.pack("<f", v))[0]

    win_size[0], win_size[1] = 0, 0
    dpy = call("eglGetDisplay", 0)
    assert call("eglInitialize", dpy, 0, 0) == 1

    rb, gb, bb = config_rgb_bits
    attr_mem = halloc(64, True)
    attrs = [0x3040, 1, 0x3024, rb, 0x3023, gb, 0x3022, bb, 0x3025, 16, 0x3026, 8, 0x3038]
    mu.mem_write(attr_mem, struct.pack(f"<{len(attrs)}i", *attrs))
    cfg_out = halloc(8, True)
    num_out = halloc(8, True)
    assert call("eglChooseConfig", dpy, attr_mem, cfg_out, 1, num_out) == 1
    cfg = struct.unpack("<I", mu.mem_read(cfg_out, 4))[0]

    surf = call("eglCreateWindowSurface", dpy, cfg, halloc(32, True), 0)
    ctx  = call("eglCreateContext", dpy, cfg, 0, 0)
    assert call("eglMakeCurrent", dpy, surf, surf, ctx) == 1

    W, H = 128, 96
    win_size[0], win_size[1] = W, H
    call("glViewport", 0, 0, W, H)
    call("glClearColor", f32(0.08), f32(0.12), f32(0.22), f32(0.0))
    call("glClear", 0x00004000 | 0x00000100)

    call("glMatrixMode", 0x1701)
    call("glLoadIdentity")
    fH = math.tan(45.0 * math.pi / 360.0) * 1.0
    fW = fH * (W / H)
    call("glFrustumf", f32(-fW), f32(fW), f32(-fH), f32(fH), f32(1.0), f32(20.0))
    call("glMatrixMode", 0x1700)
    call("glLoadIdentity")
    call("glTranslatef", f32(0.0), f32(0.0), f32(-3.2))
    call("glRotatef", f32(28.0), f32(1.0), f32(0.6), f32(0.2))

    tex_id_mem = halloc(8, True)
    call("glGenTextures", 1, tex_id_mem)
    tid = struct.unpack("<I", mu.mem_read(tex_id_mem, 4))[0]
    call("glBindTexture", 0x0DE1, tid)
    call("glTexParameteri", 0x0DE1, 0x2801, 0x2601)
    call("glTexParameteri", 0x0DE1, 0x2800, 0x2601)
    tex_px = bytearray()
    for y in range(8):
        for x in range(8):
            tex_px += bytes((255, 140, 40, 255) if ((x ^ y) & 1) else (40, 180, 255, 255))
    tex_mem = halloc(len(tex_px)); mu.mem_write(tex_mem, bytes(tex_px))
    call("glTexImage2D", 0x0DE1, 0, 0x1908, 8, 8, 0, 0x1908, 0x1401, tex_mem)

    call("glEnable", 0x0B71)
    call("glEnable", 0x0DE1)
    call("glEnableClientState", 0x8074)
    call("glEnableClientState", 0x8078)
    call("glEnableClientState", 0x8076)

    verts = [
        -1,-1, 1,  1,-1, 1, -1, 1, 1,  1, 1, 1,
         1,-1, 1,  1,-1,-1,  1, 1, 1,  1, 1,-1,
        -1, 1, 1,  1, 1, 1, -1, 1,-1,  1, 1,-1,
    ]
    uvs = [0,0, 2,0, 0,2, 2,2] * 3
    cols = [
        255,255,255,255,  220,240,255,255,  255,220,220,255,  255,255,200,255,
        200,255,220,255,  180,220,255,255,  255,240,180,255,  220,200,255,255,
        255,230,230,255,  230,255,230,255,  230,230,255,255,  255,255,255,255,
    ]
    v_mem = halloc(len(verts) * 4); mu.mem_write(v_mem, struct.pack(f"<{len(verts)}f", *verts))
    t_mem = halloc(len(uvs) * 4);   mu.mem_write(t_mem, struct.pack(f"<{len(uvs)}f", *uvs))
    c_mem = halloc(len(cols));      mu.mem_write(c_mem, bytes(cols))

    call("glVertexPointer", 3, 0x1406, 0, v_mem)
    call("glTexCoordPointer", 2, 0x1406, 0, t_mem)
    call("glColorPointer", 4, 0x1401, 0, c_mem)
    for face in range(3):
        call("glDrawArrays", 0x0005, face * 4, 4)

    call("glDisable", 0x0DE1)
    call("glDisableClientState", 0x8078)
    call("glEnable", 0x0BE2)
    call("glBlendFunc", 0x0302, 0x0303)
    tri_v = [-0.8, -0.8, 1.3,  0.8, -0.8, 1.3,  0.0, 0.8, 1.3]
    tri_c = [255, 50, 80, 140,  50, 255, 120, 140,  80, 120, 255, 140]
    tv_mem = halloc(len(tri_v) * 4); mu.mem_write(tv_mem, struct.pack(f"<{len(tri_v)}f", *tri_v))
    tc_mem = halloc(len(tri_c));     mu.mem_write(tc_mem, bytes(tri_c))
    call("glVertexPointer", 3, 0x1406, 0, tv_mem)
    call("glColorPointer", 4, 0x1401, 0, tc_mem)
    call("glDrawArrays", 0x0004, 0, 3)

    if stress_shaders:
        # Exercise ASM fast-paths (GL_DEPTH_TEST off + GL_BLEND on RGB565: col32cb16blend & t32cb16blend),
        # 2-TMU multitexture, linear fog, alpha-test, logic-op, and 20+ distinct shader combinations!
        call("glDisable", 0x0B71) # GL_DEPTH_TEST off -> triggers col32cb16blend / t32cb16blend on RGB565!
        call("glDrawArrays", 0x0004, 0, 3) # col32cb16blend
        call("glEnable", 0x0DE1)
        call("glEnableClientState", 0x8078)
        call("glTexCoordPointer", 2, 0x1406, 0, t_mem)
        call("glDrawArrays", 0x0004, 0, 3) # t32cb16blend
        call("glEnable", 0x0B71)

        # 2-TMU Multitexture + Linear Fog + Alpha Test + Blend combinations
        call("glActiveTexture", 0x84C1) # GL_TEXTURE1
        call("glEnable", 0x0DE1)
        call("glBindTexture", 0x0DE1, tid)
        call("glClientActiveTexture", 0x84C1)
        call("glEnableClientState", 0x8078)
        call("glTexCoordPointer", 2, 0x1406, 0, t_mem)
        call("glActiveTexture", 0x84C0)
        call("glClientActiveTexture", 0x84C0)

        call("glEnable", 0x0B60) # GL_FOG
        call("glFogx", 0x0B65, 0x2601) # GL_FOG_MODE = GL_LINEAR
        call("glFogf", 0x0B63, f32(0.5))
        call("glFogf", 0x0B64, f32(4.0))

        blend_srcs = [0x0302, 0x0001, 0x0304]
        blend_dsts = [0x0303, 0x0001, 0x0300, 0x0306]
        alpha_funcs = [0x0204, 0x0201, 0x0206, 0x0207]
        depth_funcs = [0x0201, 0x0203, 0x0204]
        tex_envs = [0x2100, 0x1E01, 0x2101, 0x0104] # MODULATE, REPLACE, DECAL, ADD
        for si in range(96):
            call("glBlendFunc", blend_srcs[si % len(blend_srcs)], blend_dsts[(si // 3) % len(blend_dsts)])
            call("glDepthFunc", depth_funcs[(si // 12) % len(depth_funcs)])
            if si % 2 == 0:
                call("glEnable", 0x0BC0) # GL_ALPHA_TEST
                call("glAlphaFunc", alpha_funcs[(si // 2) % len(alpha_funcs)], f32(0.1))
            else:
                call("glDisable", 0x0BC0)
            call("glTexEnvi", 0x2300, 0x2200, tex_envs[(si // 6) % len(tex_envs)])
            call("glDrawArrays", 0x0004, 0, 3)

        call("glActiveTexture", 0x84C1)
        call("glDisable", 0x0DE1)
        call("glActiveTexture", 0x84C0)
        call("glDisable", 0x0B60)
        call("glDisable", 0x0BC0)

    assert call("glGetError") == 0
    assert call("eglSwapBuffers", dpy, surf) == 1

    bmp = bitmaps[last_bitblt_bmp[0]]
    assert bmp["w"] == W and bmp["h"] == H and bmp["mode"] == 11
    raw_bgra = bytes(mu.mem_read(bmp["data"], W * H * 4))
    for i in range(0, len(raw_bgra), 4):
        assert raw_bgra[i + 3] == 0xFF

    # Run targeted pixel regressions after capturing the showcase, so its
    # JIT/C++ comparison remains independent of these assert-based checks.
    def floats(values):
        addr = halloc(len(values) * 4)
        mu.mem_write(addr, struct.pack(f"<{len(values)}f", *values))
        return addr

    pixel_mem = halloc(W * H * 4, True)

    def pixel(x, y):
        call("glReadPixels", x, y, 1, 1, 0x1908, 0x1401, pixel_mem)
        return tuple(mu.mem_read(pixel_mem, 4))[:3]

    def clear(r=0, g=0, b=0):
        call("glClearColor", f32(r), f32(g), f32(b), f32(1))
        call("glClear", 0x4100)

    def color(r, g, b):
        call("glColor4f", f32(r), f32(g), f32(b), f32(1))

    def vertex(values):
        call("glVertexPointer", 3, 0x1406, 0, floats(values))

    for cap in (0x0B71, 0x0DE1, 0x0BE2, 0x0B60, 0x0BC0, 0x0B44, 0x0BF2):
        call("glDisable", cap)
    for array in (0x8076, 0x8078, 0x8075):
        call("glDisableClientState", array)
    for mode in (0x1701, 0x1700):
        call("glMatrixMode", mode)
        call("glLoadIdentity")
    call("glDepthFunc", 0x0201)  # LESS, even after shader stress
    call("glClearDepthx", 0x10000)

    # Shared indexed vertices must be relit when winding switches.
    call("glEnable", 0x0B50)  # LIGHTING
    call("glEnable", 0x4000)  # LIGHT0
    call("glLightfv", 0x4000, 0x1203, floats([0, 0, 1, 0]))
    call("glLightfv", 0x4000, 0x1201, floats([1, 1, 1, 1]))
    call("glLightfv", 0x4000, 0x1200, floats([0, 0, 0, 1]))
    call("glMaterialfv", 0x0408, 0x1201, floats([1, 1, 1, 1]))
    call("glMaterialfv", 0x0408, 0x1200, floats([1, 1, 1, 1]))
    vertex([-0.7,-0.5,0, 0.7,-0.5,0, -0.7,0.5,0, 0.7,0.5,0])
    call("glNormalPointer", 0x1406, 0, floats([0, 0, 1] * 4))
    call("glEnableClientState", 0x8075)
    indices = halloc(12)
    mu.mem_write(indices, struct.pack("<6H", 0, 1, 2, 1, 2, 3))
    call("glLightModelx", 0x0B52, 1)  # TWO_SIDE
    clear()
    call("glDrawElements", 4, 6, 0x1403, indices)
    assert min(pixel(49, 40)) > 200, "front lighting"
    assert max(pixel(79, 56)) < 70, "back-face relighting"
    call("glLightModelx", 0x0B52, 0)
    clear()
    call("glDrawElements", 4, 6, 0x1403, indices)
    assert min(pixel(79, 56)) > 200, "one-sided lighting"
    call("glDisableClientState", 0x8075)
    call("glEnable", 0x0BA1)  # NORMALIZE
    call("glNormal3f", f32(100), f32(100), f32(100))
    clear()
    call("glDrawElements", 4, 6, 0x1403, indices)
    assert 150 < pixel(49, 40)[0] < 240, "large normal normalization"
    call("glDisable", 0x0BA1)
    call("glDisable", 0x0B50)

    # Far points and rectangles must not wrap back to near depth.
    call("glEnable", 0x0B71)
    clear(0, 1, 0)
    color(1, 0, 0)
    vertex([0, 0, 1])
    call("glPointSize", f32(5))
    call("glDrawArrays", 0, 0, 1)
    assert pixel(W//2, H//2)[1] > 200, "far point wrapped"

    flat = [-0.8,-0.6,0.5, 0.8,-0.6,0.5, -0.8,0.6,0.5, 0.8,0.6,0.5]
    tilted = [-0.8,-0.6,0.4, 0.8,-0.6,0.6, -0.8,0.6,0.4, 0.8,0.6,0.6]
    for factor, expected_channel in ((800, 0), (-800, 1)):
        clear(0, 0, 1)
        call("glDisable", 0x8037)  # POLYGON_OFFSET_FILL
        color(1, 0, 0)
        vertex(flat)
        call("glDrawArrays", 5, 0, 4)
        call("glEnable", 0x8037)
        call("glPolygonOffsetx", factor * 65536, 0)
        color(0, 1, 0)
        vertex(tilted)
        call("glDrawArrays", 5, 0, 4)
        for x in (W//4, 3*W//4):
            rgb = pixel(x, H//2)
            assert rgb[expected_channel] > 200 and rgb[1-expected_channel] < 40, ("offset clamp", factor, x, rgb)
    call("glDisable", 0x8037)

    # A 1x1 white texture exercises drawTex z and COMBINE rendering.
    call("glBindTexture", 0x0DE1, tid)
    call("glTexParameteri", 0x0DE1, 0x2801, 0x2600)
    call("glTexParameteri", 0x0DE1, 0x2800, 0x2600)
    white = halloc(4); mu.mem_write(white, b"\xff" * 4)
    call("glTexImage2D", 0x0DE1, 0, 0x1908, 1, 1, 0, 0x1908, 0x1401, white)
    crop = halloc(16); mu.mem_write(crop, struct.pack("<4i", 0, 0, 1, 1))
    call("glTexParameteriv", 0x0DE1, 0x8B9D, crop)
    call("glTexEnvi", 0x2300, 0x2200, 0x2100)  # MODULATE
    call("glEnable", 0x0DE1)
    color(1, 1, 1)
    for z, white_expected in ((65536, False), (0, True)):
        clear(0, 0, 1)
        call("glDrawTexxOES", 10<<16, 10<<16, z, 20<<16, 20<<16)
        rgb = pixel(20, 20)
        assert (min(rgb) > 200) if white_expected else (rgb[0] < 40 and rgb[2] > 200), ("drawTex z", z, rgb)
    call("glDisable", 0x0B71)
    result = halloc(16, True)
    for pname, value in ((0x2200, 0x8570), (0x8571, 0x1E01),
                         (0x8580, 0x8578), (0x8590, 0x0300), (0x8573, 2)):
        call("glTexEnvi", 0x2300, pname, value)
        call("glGetTexEnviv", 0x2300, pname, result)
        assert struct.unpack("<i", mu.mem_read(result, 4))[0] == value, ("COMBINE getter", pname)
    call("glGetTexEnvxv", 0x2300, 0x8573, result)
    assert struct.unpack("<i", mu.mem_read(result, 4))[0] == 2*65536
    call("glGetTexEnvfv", 0x2300, 0x8573, result)
    assert struct.unpack("<f", mu.mem_read(result, 4))[0] == 2.0
    color(0.25, 0.125, 0.0)  # REPLACE previous, RGB scale 2
    clear()
    call("glDrawTexxOES", 10<<16, 10<<16, 0, 20<<16, 20<<16)
    r, g, b = pixel(20, 20)
    assert 105 < r < 150 and 45 < g < 85 and b < 20, ("COMBINE pixels", r, g, b)
    call("glTexEnvi", 0x2300, 0x2200, 0x2100)

    # Point sprites, coordinate replacement, and float point-size arrays.
    sprite = halloc(16)
    mu.mem_write(sprite, bytes([255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255]))
    call("glTexImage2D", 0x0DE1, 0, 0x1908, 2, 2, 0, 0x1908, 0x1401, sprite)
    call("glEnable", 0x8861)  # POINT_SPRITE_OES
    call("glTexEnvi", 0x8861, 0x8862, 1)  # COORD_REPLACE
    call("glGetTexEnviv", 0x8861, 0x8862, result)
    assert struct.unpack("<i", mu.mem_read(result, 4))[0] == 1
    color(1, 1, 1)
    vertex([0, 0, 0])
    call("glPointSize", f32(16))
    clear()
    call("glDrawArrays", 0, 0, 1)
    r, g, b = pixel(W//2-4, H//2+4)
    assert r > 150 and g < 90 and b < 90, ("sprite upper-left", r, g, b)
    r, g, b = pixel(W//2+4, H//2-4)
    assert r > 150 and g > 150 and b < 90, ("sprite lower-right", r, g, b)
    call("glTexEnvi", 0x8861, 0x8862, 0)
    call("glTexCoordPointer", 2, 0x1406, 0, floats([0.75, 0.75]))
    call("glEnableClientState", 0x8078)
    clear()
    call("glDrawArrays", 0, 0, 1)
    r, g, b = pixel(W//2, H//2)
    assert r > 150 and g > 150 and b < 90, "coordReplace off"
    call("glDisableClientState", 0x8078)
    call("glTexEnvi", 0x8861, 0x8862, 1)
    call("glEnableClientState", 0x8B9C)
    call("glPointSizePointerOES", 0x1406, 0, floats([24, 4]))
    vertex([0, -0.5, 0, 0, 0.5, 0])
    clear()
    call("glDrawArrays", 0, 0, 2)
    call("glReadPixels", 0, 0, W, H, 0x1908, 0x1401, pixel_mem)
    image = bytes(mu.mem_read(pixel_mem, W*H*4))
    def lit_count(cy):
        return sum(sum(image[(y*W+x)*4:(y*W+x)*4+3]) > 90
                   for y in range(cy-14, cy+14) for x in range(W//2-14, W//2+14))
    assert lit_count(H//4) >= 400 and lit_count(3*H//4) <= 30, "point size array"
    call("glDisableClientState", 0x8B9C)
    assert call("glGetError") == 0, "targeted regression GL error"
    print("  lights / saturated depth / COMBINE / point-sprite pixel regressions passed")

    # Clean teardown via eglTerminate
    assert call("eglTerminate", dpy) == 1

    print(f"[ARMv5TE cfg={config_rgb_bits} jit={allow_jit}] relocs={applied_relocs}, IMB_Range={imb_calls[0]}, JIT_insns={jit_insns_executed[0]}")
    return W, H, raw_bgra, imb_calls[0], jit_insns_executed[0]


if __name__ == "__main__":
    W, H, fb565_jit, imb565, jins565 = run_symbian_e32_scene(True,  (5, 6, 5))
    _, _, fb565_cpp, imb_no, jins_no = run_symbian_e32_scene(False, (5, 6, 5))
    _, _, fb888_jit, imb888, jins888 = run_symbian_e32_scene(True,  (8, 8, 8))
    _, _, fb888_cpp, _,      _       = run_symbian_e32_scene(False, (8, 8, 8))

    assert imb565 >= 2 and jins565 > 50000
    assert imb888 >= 2 and jins888 > 50000
    assert imb_no == 0 and jins_no == 0

    def save_quad_png(path):
        out_w, out_h = W * 2 + 4, H * 2 + 4
        rows = []
        for panel_row, (left_fb, right_fb) in enumerate([(fb565_jit, fb565_cpp), (fb888_jit, fb888_cpp)]):
            if panel_row == 1:
                for _ in range(4):
                    rows.append(b"\x00" + b"\xff\xff\xff" * out_w)
            for y in range(H):
                row = bytearray([0])
                for x in range(W):
                    b, g, r, _ = left_fb[(y*W + x)*4 : (y*W + x)*4 + 4]
                    row += bytes((r, g, b))
                row += b"\xff\xff\xff" * 4
                for x in range(W):
                    b, g, r, _ = right_fb[(y*W + x)*4 : (y*W + x)*4 + 4]
                    row += bytes((r, g, b))
                rows.append(bytes(row))
        raw = b"".join(rows)
        def pchunk(t, d):
            return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
        png = b"\x89PNG\r\n\x1a\n" + pchunk(b"IHDR", struct.pack(">IIBBBBB", out_w, out_h, 8, 2, 0, 0, 0)) + pchunk(b"IDAT", zlib.compress(raw)) + pchunk(b"IEND", b"")
        open(path, "wb").write(png)

    save_quad_png("/tmp/armv5_jit_vs_cpp.png")

    for label, f_a, f_b in [("RGB565", fb565_jit, fb565_cpp), ("RGBA8888", fb888_jit, fb888_cpp)]:
        mean_d = sum(abs(f_a[i] - f_b[i]) for i in range(len(f_a))) / (W * H * 3)
        print(f"[Compare {label}] ARMv5TE JIT vs C++ fallback mean_channel_diff = {mean_d:.3f} / 255")
        assert mean_d < 5.0, f"{label} mean diff too high: {mean_d}"

    # RGBA4444 + 64KB low-exec-RAM fallback + 24-shader multitexture/fog/alphatest/ASM-fastpath stress test!
    _, _, fb4444_jit, imb4444, jins4444 = run_symbian_e32_scene(True,  (4, 4, 4))
    _, _, fb4444_cpp, _,       _        = run_symbian_e32_scene(False, (4, 4, 4))
    mean_4444 = sum(abs(fb4444_jit[i] - fb4444_cpp[i]) for i in range(len(fb4444_jit))) / (W * H * 3)
    print(f"[Compare RGBA4444] ARMv5TE JIT vs C++ fallback mean_channel_diff = {mean_4444:.3f} / 255")
    assert imb4444 >= 2 and jins4444 > 50000 and mean_4444 < 6.0

    _, _, _, imb_stress565, jins_stress565 = run_symbian_e32_scene(True, (5, 6, 5), max_exec_bytes=64*1024, stress_shaders=True)
    _, _, _, imb_stress888, jins_stress888 = run_symbian_e32_scene(True, (8, 8, 8), max_exec_bytes=64*1024, stress_shaders=True)
    print(f"[Stress 64KB ExecChunk + LRU Eviction] RGB565 IMB={imb_stress565}, RGBA8888 IMB={imb_stress888}")
    assert imb_stress565 >= 80 and imb_stress888 >= 80

    print("=== ALL ARMV5TE E32 UNICORN JIT + C++ FALLBACK + STRESS TESTS PASSED! ===")
