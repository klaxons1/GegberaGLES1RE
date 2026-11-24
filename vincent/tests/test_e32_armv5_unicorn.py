#!/usr/bin/env python3
"""Vincent E32 DLL ARMv5TE CPU test with mocked Symbian OS imports.
Loader/OS fixture derived from the PixelFlinger harness in this repository.
This is not an actual Symbian kernel or phone test.
"""
import math
import os
import struct
import zlib
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import *

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DLL_PATH  = os.path.join(REPO_ROOT, "vincent", "out", "libgles_cm.dll")
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
assert (uid1,uid2,uid3)==(0x10000079,0x1000008d,0x101fcabd)
assert comp_type==0 and mod_ver==0x000a0000
for i in range(175):
    address=struct.unpack_from("<I",dll,export_dir_off+i*4)[0]
    assert code_base<=address<code_base+code_size, (i+1,address)

ord_by_name = {}
for line in open(DEF_PATH):
    parts = line.strip().split()
    if len(parts) >= 3 and parts[1] == "@":
        ord_by_name[parts[0]] = int(parts[2])


def run_symbian_e32_scene(config_rgb_bits=(5, 6, 5)):
    allow_jit = False
    max_exec_bytes = 0
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

    win_size[:] = [0, 0]
    dpy = call("eglGetDisplay", 0)
    assert call("eglInitialize", dpy, 0, 0) == 1
    rb, gb, bb = config_rgb_bits
    attrs = [0x3024,rb,0x3023,gb,0x3022,bb,0x3025,16,0x3038]
    attr = halloc(len(attrs)*4); mu.mem_write(attr,struct.pack("<%di"%len(attrs),*attrs))
    cfg_out=halloc(4,True); count=halloc(4,True)
    assert call("eglChooseConfig",dpy,attr,cfg_out,1,count)==1
    cfg=struct.unpack("<I",mu.mem_read(cfg_out,4))[0]; assert cfg
    surf=call("eglCreateWindowSurface",dpy,cfg,halloc(32,True),0); assert surf
    ctx=call("eglCreateContext",dpy,cfg,0,0); assert ctx
    assert call("eglMakeCurrent",dpy,surf,surf,ctx)==1
    win_size[:] = [97, 73]
    call("glViewport",0,0,97,73)
    call("glClearColor",f32(0),f32(0),f32(1),f32(1))
    call("glClearDepthf",f32(1)); call("glClear",0x4100)
    out=halloc(4,True)
    def pixel(x=48,y=36):
        call("glReadPixels",x,y,1,1,0x1908,0x1401,out)
        return tuple(mu.mem_read(out,4))
    assert pixel()[2]>240
    verts=[-.8,-.8,0,.8,-.8,0,0,.8,0]
    v=halloc(36);mu.mem_write(v,struct.pack("<9f",*verts))
    call("glEnableClientState",0x8074);call("glVertexPointer",3,0x1406,0,v)
    call("glColor4ub",255,0,0,255);call("glDrawArrays",4,0,3)
    assert pixel()[0]>240 and pixel()[2]<10, pixel()
    call("glEnable",0x0B71);call("glClear",0x4100)
    call("glTranslatef",f32(0),f32(0),f32(-.5))
    call("glColor4ub",0,255,0,255);call("glDrawArrays",4,0,3)
    call("glLoadIdentity");call("glColor4ub",255,0,0,255);call("glDrawArrays",4,0,3)
    assert pixel()[1]>240 and pixel()[0]<10,pixel()
    call("glDisable",0x0B71)
    # Exercise real native floating-point fog (freestanding exp) and lighting.
    call("glEnable",0x0B50);call("glEnable",0x4000);call("glNormal3f",f32(0),f32(0),f32(1))
    call("glDrawArrays",4,0,3);assert min(pixel()[:3])>100,pixel()
    call("glDisable",0x0B50);call("glEnable",0x0B60)
    call("glFogf",0x0B65,f32(0x0800));call("glFogf",0x0B62,f32(1))
    call("glTranslatef",f32(0),f32(0),f32(-.5));call("glDrawArrays",4,0,3)
    assert 100<pixel()[0]<220,pixel()
    call("glDisable",0x0B60);call("glLoadIdentity")
    # Texture REPLACE also omits the primary color varying.
    tex=halloc(4,True);call("glGenTextures",1,tex)
    tid=struct.unpack("<I",mu.mem_read(tex,4))[0];call("glBindTexture",0x0DE1,tid)
    texels=halloc(16);mu.mem_write(texels,bytes([0,255,0,255])*4)
    call("glTexImage2D",0x0DE1,0,0x1908,2,2,0,0x1908,0x1401,texels)
    call("glTexParameteri",0x0DE1,0x2801,0x2600);call("glTexParameteri",0x0DE1,0x2800,0x2600)
    call("glTexEnvi",0x2300,0x2200,0x1E01);call("glEnable",0x0DE1)
    call("glMultiTexCoord4f",0x84C0,f32(.5),f32(.5),0,f32(1))
    call("glDrawArrays",4,0,3);assert pixel()[1]>240 and pixel()[0]<10,pixel()
    assert call("eglSwapBuffers",dpy,surf)==1
    b=bitmaps[last_bitblt_bmp[0]];assert b["locked"]==0
    bgra=tuple(mu.mem_read(b["data"]+(36*b["w"]+48)*4,4));assert bgra[1]>240,bgra
    # Non-block-aligned RWindow resize and present.
    win_size[:]=[101,79];call("glViewport",0,0,101,79)
    call("glClear",0x4100);call("glDrawArrays",4,0,3)
    assert call("eglSwapBuffers",dpy,surf)==1
    b=bitmaps[last_bitblt_bmp[0]];assert (b["w"],b["h"])==(101,79)
    assert call("glGetError")==0
    assert call("eglDestroySurface",dpy,surf)==1
    assert call("eglDestroyContext",dpy,ctx)==1
    assert call("eglMakeCurrent",dpy,0,0,0)==1
    assert call("eglTerminate",dpy)==1
    assert jit_insns_executed[0]==0 and imb_calls[0]==0
    print("PASS ARMv5TE E32",config_rgb_bits,"relocations",applied_relocs,
          "clear/triangle/depth/lighting/fog/texture/present/resize/lifecycle")

if __name__=="__main__":
    for rgb in [(5,6,5),(4,4,4),(8,8,8)]: run_symbian_e32_scene(rgb)
