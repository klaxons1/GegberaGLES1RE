#!/usr/bin/env python3
"""Split docs/libGLES_CM.dll.c into fragments grouped by inferred source file.

Attribution uses assertion filenames, symbol names, call sites and proximity.
Normalized clone bodies are merged; differing bodies remain as alternates.
Writes reference fragments and an inventory under generated/. These are
analysis inputs, not compilable renderer sources.
"""

import collections
import hashlib
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DECOMP = os.path.join(ROOT, "docs", "libGLES_CM.dll.c")
OUT_DIR = os.path.join(ROOT, "generated")

# ---------------------------------------------------------------------------
# Decompiler spellings normalized for fragment readability; ABI annotations
# are discarded, so these fragments are not directly compilable source.
TYPE_MAP = [
    ("unsigned __int8", "unsigned char"),
    ("unsigned __int16", "unsigned short"),
    ("unsigned __int32", "unsigned int"),
    ("unsigned __int64", "unsigned long long"),
    ("__int8", "char"),
    ("__int16", "short"),
    ("__int32", "int"),
    ("__int64", "long long"),
    ("_BOOL", "int"),
    ("_BYTE", "char"),
    ("_WORD", "short"),
    ("_DWORD", "int"),
    ("_QWORD", "long long"),
    ("__ptr32 ", ""),
    ("__cdecl ", ""),
    ("__thiscall ", ""),
    ("__stdcall ", ""),
    ("__usercall ", ""),
    ("__userpurge ", ""),
    ("__noreturn ", ""),
    ("__hidden ", ""),
]

# Original project tree (recovered from assert strings and PE strings)
MODULES = {
    "egl":     "src/egl (EGL 1.x API - eglApi.inl, eglBase.inl, eglUtil.inl)",
    "egl_sym": "src/symbian (eglSymbian.inl - Symbian native window/pixmap glue)",
    "hg":      "src/hg (Hybrid Graphics support lib - hgDefs.h, hgInt32.inl, hgFloat.c, hgFill.c)",
    "imath":   "src/imath (fixed point math - imApi, IScalar, IVector, IMatrix, INormal)",
    "core":    "src/gerbera/core (rasterizer - gr*.inl, grSmall.c)",
    "grr":     "src/gerbera/core/rasterizer (grr*.inl)",
    "api":     "src/gerbera/api_opengl (GL API - ogl*.inl, oglFast.c)",
    "hc":      "src/gerbera/hc (pixel pipeline JIT compiler)",
    "sym":     "src/symbian (Symbian AVKON/WSERV/FBS glue)",
    "crt":     "src/platform (C runtime, exceptions, C++ support - reference only)",
    "unknown": "UNATTRIBUTED",
}

# function name prefix -> (module, default original file)
PREFIX_RULES = [
    # imath
    (r"^IScalar_",        ("imath", "IScalar.c")),
    (r"^IUnitScalar_",    ("imath", "IUnitScalar.c")),
    (r"^IMatrix4_",       ("imath", "IMatrix4.c")),
    (r"^IMatrix3_",       ("imath", "IMatrix3.c")),
    (r"^IVector4_",       ("imath", "IVector4.c")),
    (r"^IVector3_",       ("imath", "IVector3.c")),
    (r"^INormal3_",       ("imath", "INormal3.c")),
    (r"^IMatrix4x4_",     ("imath", "IMatrix4.c")),
    (r"^IMatrix3x3_",     ("imath", "IMatrix3.c")),
    (r"^IQuaternion_",    ("imath", "IMath.c")),
    (r"^IFVector",        ("imath", "imInline.inl")),
    (r"^XScalar",         ("imath", "imInline.inl")),
    (r"^XVector",         ("imath", "imInline.inl")),
    (r"^XNormal",         ("imath", "imInline.inl")),
    (r"^XColor",          ("imath", "imInline.inl")),
    (r"^XPoint",          ("imath", "imInline.inl")),
    (r"^IFColor",         ("imath", "imInline.inl")),
    (r"^IMath",           ("imath", "IMath.c")),
    (r"^im",              ("imath", "imInline.inl")),
    # hc compiler
    (r"^HCBitSet",        ("hc", "hcBitSet.h")),
    (r"^HCAdjMtx",        ("hc", "hcAdjMtx.h")),
    (r"^HCAdjList",       ("hc", "hcAdjList.c")),
    (r"^HCArena",         ("hc", "hcMemory.h")),
    (r"^HCMemory",        ("hc", "hcMemory.h")),
    (r"^HCBucketDefs",    ("hc", "hcBucketDefs.h")),
    (r"^HC[BQ]ueue",      ("hc", "hcQueue.c")),
    (r"^HCStack",         ("hc", "hcStack.h")),
    (r"^HCMark",          ("hc", "hcCompiler.c")),
    (r"^HC_getOpcode|HC_opcode|HC_is(Branch|CondBranch|Load|Store)|HC_setOpcode", ("hc", "hcOpcode.h")),
    (r"^HCX86Backend|EncodeContext|isGeneralReg|isNegativeByte|^movi?FixSize|^movi?|^add$|^sub$|^lea|^and|^or$|^xor|^cmp|^test|^jmp|^j[a-z]+$|^push|^pop$|^ret$|^emit", ("hc", "hcX86Backend.c")),
    (r"^HC",              ("hc", None)),
    (r"^hc[A-Z]",         ("hc", None)),
    (r"^DUChains?",       ("hc", "hcDUChain.c")),
    (r"^UDChain",         ("hc", "hcDUChain.c")),
    (r"^LREntry",         ("hc", "hcBackend.h")),
    (r"^BasicBlock",      ("hc", "hcBasicBlock.h")),
    (r"^CopySet",         ("hc", "hcDUChain.c")),
    (r"^F_(MIN|MAX|NEG|ABS|CMP|ZCMP|MAKE|SUB|ADD|DIV|GET|SET)", ("hg", "hgFloat.inl")),
    (r"^hgFill",          ("hg", "hgFill.c")),
    (r"^hg",              ("hg", "hgInt32.inl")),
    # rasterizer
    (r"^grr",             ("grr", None)),
    # ogl / gl api
    (r"^gl[A-Z]",         ("api", "oglApi.inl")),
    (r"^oglTex",          ("api", "oglTexManager.inl")),
    (r"^oglBufObj",       ("api", "oglBufObjManager.inl")),
    (r"^oglGet(ter|Internal)", ("api", "oglGetter.inl")),
    (r"^oglSetGetter",    ("api", "oglGetter.inl")),
    (r"^oglMatrix",       ("api", "oglMatrixStack.inl")),
    (r"^oglFillDescriptor", ("api", "oglDescriptor.inl")),
    (r"^oglSetError",     ("api", "oglError.inl")),
    (r"^oglGetContext",   ("api", "oglContext.inl")),
    (r"^ogl",             ("api", "oglUtil.inl")),
    # gr core
    (r"^grRaster",        ("core", "grRasterizer.inl")),
    (r"^grScan",          ("core", "grScanConvert.inl")),
    (r"^grTess",          ("core", "grTessellator.inl")),
    (r"^grClip",          ("core", "grClipper.inl")),
    (r"^grFill",          ("core", "grFill.inl")),
    (r"^grVertex",        ("core", "grVertexPipeline.inl")),
    (r"^grPixel",         ("core", "grPixelPipeline.inl")),
    (r"^grPipeline",      ("core", "grPipelineCache.inl")),
    (r"^grInterp",        ("core", "grInterpreter.inl")),
    (r"^grApiFast",       ("core", "grApiFast.inl")),
    (r"^grApiSmall",      ("core", "grApiSmall.inl")),
    (r"^grUtilSmall",     ("core", "grUtilSmall.inl")),
    (r"^grUnpack|grPack|grSetRGBA|grGetRGBA", ("core", "grRGBA.h")),
    (r"^gr",              ("core", "grSmall.c")),
    # egl
    (r"^egli[A-Z]",       ("egl_sym", "eglSymbian.inl")),
    (r"^egl",             ("egl", "eglBase.inl")),
]

# Runtime and platform function prefixes.
CRT_PAT = re.compile(
    r"^(__msl|__utf8|__unicode|__mbtowc|__wctomb|__init_pool|__pool_free|__malloc|get_malloc"
    r"|__sys_free|__read_file|__write_file|__close_file|__delete_file|find_temp_info|__msl"
    r"|\$static_initializer|__destroy|__cxx|std::|operator|RChunk|RHandleBase|CBase|CCoeEnv"
    r"|CFbsBitmap|CWsBitmap|CWindowGc|TPoint|TSize|TLinearOrder|RArray|TEGL|XLeaveException"
    r"|TWin32SEHTrap|HandlerForHandler|MyFrameHandler|__CxxFrameHandler|_CxxThrowException"
    r"|Dll::|User::|UserSvr|UserHeap|TDisplayModeUtils|ExX86|__detect_cpu|__throw_catch"
    r"|__unexpected|dthandler|duhandler|terminate|__set_errno|__get_MSL|_CleanUpMSL"
    r"|__kill_critical|_InitializeThread|_DisposeThread|_GetThreadLocalData|__init_critical"
    r"|__end_critical|__begin_critical|raise|abort|exit|__exit"
    r"|Mem::|__noreturn)"
)


def clean_line(line):
    """Normalize decompiler type spellings and clone suffixes."""
    out = line
    for a, b in TYPE_MAP:
        out = out.replace(a, b)
    # clone suffixes on identifiers
    out = re.sub(r"__cv_[0-9A-Fa-f]+", "", out)
    return out


def strip_stack_comments(text):
    text = re.sub(r"\s*// \[esp[^\]]*\].*$", "", text, flags=re.M)
    return text


def normalize_body(text):
    """Body normalization used to compare clones across TUs."""
    t = strip_stack_comments(text)
    for a, b in TYPE_MAP:
        t = t.replace(a, b)
    t = re.sub(r"__cv_[0-9A-Fa-f]+", "", t)
    # Ignore translation-unit-specific anonymous enum IDs.
    t = re.sub(r"_enum_[0-9]+", "_enum_", t)
    # Treat enum spellings as interchangeable in this heuristic comparison.
    t = re.sub(r"\bGR_[A-Z0-9_]+\b", "GR_SYM", t)
    t = re.sub(r"\bOGL_[A-Z0-9_]+\b", "OGL_SYM", t)
    t = re.sub(r"\bHCOP_[A-Z0-9_]+\b", "HCOP_SYM", t)
    t = re.sub(r"\s+", " ", t)
    return t


def parse_sections(lines):
    decl_start = next(i for i, l in enumerate(lines) if l.startswith("// Function declarations"))
    data_start = next(i for i, l in enumerate(lines) if l.startswith("// Data declarations"))
    return lines[:data_start], lines[data_start:]


def find_bodies(lines):
    """Yield (sig_line_start, open_brace_line, close_brace_line, signature) for
    every top-level function definition."""
    n = len(lines)
    i = 0
    while i < n:
        ln = lines[i]
        if re.match(r"^[A-Za-z_][A-Za-z0-9_ \*]*\(", ln) and not ln.lstrip().startswith("//"):
            j, found = i, None
            while j < min(i + 30, n):
                if lines[j] == "{":
                    found = j
                    break
                if j > i and re.match(r"^[A-Za-z_][A-Za-z0-9_ \*]*\(", lines[j]):
                    break
                j += 1
            if found is not None:
                sig = " ".join(s.strip() for s in lines[i:found])
                # false positives: data declarations with function-pointer
                # initializers followed by an array initializer block
                if ";" in sig or " = " in sig:
                    i = found + 1
                    continue
                # brace match from found
                k, depth = found, 0
                while k < n:
                    if lines[k].startswith("}"):
                        depth -= 1
                        if depth == 0:
                            break
                    else:
                        depth += lines[k].count("{") - lines[k].count("}")
                        if depth == 0 and lines[k].strip().endswith("}"):
                            break
                    k += 1
                yield (i, found, k, sig)
                i = k + 1
                continue
        i += 1


def func_name(sig):
    m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)(__cv_[0-9A-Fa-f]+)?\s*\(", sig)
    if not m:
        return None, None, None
    base = re.sub(r"__cv_[0-9A-Fa-f]+$", "", m.group(1))
    clone = m.group(2) or ""
    if base in ("int", "void", "char", "unsigned", "long", "short", "float", "double", "signed"):
        return None, None, None
    return base, clone, m.group(1)


def assert_files(body):
    """File names referenced by grAssert/imAssert calls in the body."""
    files = re.findall(r'grAssert\("[^"]*",\s*"([^"]+)",|imAssert\("[^"]*",\s*"([^"]+)",', body)
    return [a or b for a, b in files]


def assert_lines(body):
    out = []
    for m in re.finditer(r'(?:grAssert|imAssert)\("[^"]*",\s*"[^"]+",\s*(\d+)\)', body):
        out.append(int(m.group(1)))
    return out


def prefix_lookup(name):
    for pat, (module, file) in PREFIX_RULES:
        if re.match(pat, name):
            return module, file
    if CRT_PAT.match(name):
        return "crt", None
    return "unknown", None


def pick_by_asserts(files_seen):
    if not files_seen:
        return None
    c = collections.Counter(files_seen)
    return c.most_common(1)[0][0]


def basefile_for(name, files_seen, assert_hint):
    """Decide the original source file for a function."""
    if assert_hint:
        return assert_hint, "assert"
    module, default_file = prefix_lookup(name)
    if files_seen:
        f = pick_by_asserts(files_seen)
        if f and module_for_file(f) == module:
            return f, "assert"
    if default_file:
        return default_file, "prefix"
    if module != "unknown":
        return "(%s helper)" % module, "prefix"
    return None, "none"


def module_for_file(fname):
    if not fname:
        return "unknown"
    if fname.startswith(("hg", "hgDefs", "hgFloat", "hgInt", "hgFill", "hgMemory")):
        return "hg"
    if fname.startswith(("I", "im", "imApi")):
        return "imath"
    if fname.startswith("grr"):
        return "grr"
    if fname.startswith("gr"):
        return "core"
    if fname.startswith("ogl") or fname.startswith("gl"):
        return "api"
    if fname.startswith("eglSymbian"):
        return "egl_sym"
    if fname.startswith("egl"):
        return "egl"
    if fname.startswith("hc"):
        return "hc"
    if fname.startswith(("win32", "EPOC")):
        return "sym"
    return "unknown"


def main():
    src = open(SRC_DECOMP, "r", errors="replace").read()
    lines = src.split("\n")
    decl_lines, data_lines = parse_sections(lines)

    fr_dir = os.path.join(OUT_DIR, "fragments")
    for d in set(list(MODULES.keys())):
        os.makedirs(os.path.join(fr_dir, d), exist_ok=True)
    os.makedirs(os.path.join(fr_dir, "data"), exist_ok=True)

    manifest = []
    fragment_groups = collections.defaultdict(list)  # (module,file) -> list of text chunks

    body_count = 0
    for (sigln, openln, closeln, sig) in find_bodies(lines):
        base, clone, full = func_name(sig)
        if not base:
            continue
        body = "\n".join(lines[openln:closeln + 1])
        body_count += 1
        files_seen = assert_files(body)
        alines = assert_lines(body)
        hint = pick_by_asserts([f for f in files_seen if not f.startswith("hc") or base.startswith(("hc", "HC"))])
        # keep first hint even for hc prefixes (they assert into their own files)
        if not hint and files_seen:
            hint = pick_by_asserts(files_seen)
        orig, how = basefile_for(base, files_seen, hint)
        module = module_for_file(orig)
        if CRT_PAT.match(base):
            module, orig = "crt", (orig or "crt")
        if orig is None:
            # prefix-only attribution without a file name (grr/hc helpers)
            if module in ("grr", "hc"):
                orig = "(%s helper)" % module
            else:
                orig = None
        manifest.append({
            "name": base,
            "decomp_name": full,
            "clone": bool(clone),
            "module": module,
            "file": orig or "UNKNOWN",
            "how": how,
            "decomp_lines": [sigln + 1, closeln + 1],
            "assert_lines": alines,
            "n_copies": 1,
        })

    # call-site voting for still-unknown functions: attribute by who calls them.
    # transitive: repeat a few passes so chains of unknown callers resolve.
    for _pass in range(4):
        call_sites = collections.defaultdict(collections.Counter)
        for rec in manifest:
            if rec["module"] == "unknown":
                continue
            s, e = rec["decomp_lines"]
            body = "\n".join(lines[s - 1:e])
            for callee in re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", body):
                if callee in ("if", "for", "while", "switch", "return", "sizeof", "memset", "memcpy", "memcmp"):
                    continue
                call_sites[callee][rec["module"]] += 1
        resolved = 0
        for rec in manifest:
            if rec["module"] == "unknown":
                votes = call_sites.get(rec["name"])
                if votes:
                    mod, cnt = votes.most_common(1)[0]
                    rec["module"] = mod
                    rec["file"] = "(via call sites: %s)" % mod
                    rec["how"] = "callsite"
                    resolved += 1
        if resolved == 0:
            break

    # content-based fallback: struct types referenced by the body pin the module
    TYPE_MODULE = [
        (r"HCInstr_s|HCMark_s|HCBucketDefs_s|hcOpcode|HCOP_", "hc"),
        (r"BasicBlock_s|DUChain_s|UDChainStub_s|LREntry_s|CopySet_s|HCArena_s|HCMemory_s|HCBackend_s|HCAdjList_s|HCAdjMtx_s", "hc"),
        (r"eglState_s|eglDisplay_s|eglSurface_s|eglContext_s|eglConfig|eglRefCount", "egl"),
        (r"oglContext_s|oglTextureManager|oglBufObj|_oglGetter|oglSurfaceDescriptor", "api"),
        (r"Gerbera_s|grContext_s|grRenderTarget|grPrimitive", "core"),
        (r"IMatrix|IVector|INormal|IScalar|IFVector|XScalar|XColor|XNormal|XVector", "imath"),
        (r"HCBitSet|HCQueue|HCStack|HCChain|HC_rand|HC_max|HC_min|HC_clamp", "hc"),
    ]
    for rec in manifest:
        if rec["module"] != "unknown":
            continue
        s, e = rec["decomp_lines"]
        body = "\n".join(lines[s - 1:e])
        for pat, mod in TYPE_MODULE:
            if re.search(pat, body):
                rec["module"] = mod
                rec["file"] = "(body types: %s)" % mod
                rec["how"] = "content"
                break

    # Use proximity as a final attribution heuristic: functions from the same
    # translation unit often occupy adjacent addresses.
    attributed = [r for r in manifest if r["module"] != "unknown"]
    attributed.sort(key=lambda r: r["decomp_lines"][0])
    for rec in manifest:
        if rec["module"] != "unknown":
            continue
        pos = rec["decomp_lines"][0]
        best, bestd = None, 10 ** 9
        for r in attributed:
            d = abs(r["decomp_lines"][0] - pos)
            if d < bestd:
                best, bestd = r, d
        if best and bestd <= 600:
            rec["module"] = best["module"]
            rec["file"] = "(nearest: %s @%d)" % (best["file"], best["decomp_lines"][0])
            rec["how"] = "nearest"

    # merge clones: group by (base name) using normalized body equality
    by_name = collections.defaultdict(list)
    for rec in manifest:
        by_name[rec["name"]].append(rec)
    dedup_manifest = []
    clone_diff = []
    for name, recs in by_name.items():
        # normalized bodies
        norm_hashes = collections.defaultdict(list)
        for r in recs:
            s, e = r["decomp_lines"]
            body = "\n".join(lines[s - 1:e])
            norm_hashes[hashlib.md5(normalize_body(body).encode()).hexdigest()].append(r)
        canon = recs[0]
        canon["n_copies"] = len(recs)
        canon["clone_variants"] = len(norm_hashes)
        if len(norm_hashes) > 1:
            clone_diff.append(name)
        dedup_manifest.append(canon)

    # write fragments: one file per (module, orig file), functions in decomp order
    def frag_filename(module, fname):
        if fname and not fname.startswith("("):
            return re.sub(r"[^A-Za-z0-9_.]", "_", fname)
        # pseudo attribution: group into a single helpers file per module
        return "_helpers_%s.c" % module

    seen = set()
    for rec in dedup_manifest:
        key = (rec["module"], rec["file"])
        if key in seen:
            continue
        seen.add(key)
    group_chunks = collections.defaultdict(list)
    for rec in dedup_manifest:
        s, e = rec["decomp_lines"]
        chunk = clean_line("\n".join(lines[s - 1:e]))
        chunk = strip_stack_comments(chunk)
        hdr = (
            f"// ---- {rec['name']}  (decompiled: docs/libGLES_CM.dll.c:{s}-{e}, "
            f"{rec['n_copies']} TU copies, {rec['clone_variants']} variants)\n"
            f"// original file: {rec['file']}  attribution: {rec['how']}"
        )
        group_chunks[(rec["module"], rec["file"])].append(hdr + "\n" + chunk + "\n")

    for (module, fname), chunks in sorted(group_chunks.items()):
        safe = frag_filename(module, fname)
        path = os.path.join(fr_dir, module, safe + ".fragments.c")
        with open(path, "w") as fh:
            fh.write(
                f"/* fragments attributed to original file: {fname}\n"
                f"   module: {module}  ({MODULES[module]})\n"
                f"   NOTE: decompiled material - reference for porting into src/,\n"
                f"   not compiled as-is. */\n\n" + "\n".join(chunks))

    # data declarations: from data_start to the first function body
    data_start = next(i for i, l in enumerate(lines) if l.startswith("// Data declarations"))
    first_body = min(r["decomp_lines"][0] for r in manifest) - 1
    i = data_start + 1
    n = first_body
    data_chunks = collections.defaultdict(list)
    cur = []

    def flush(cur):
        text = "\n".join(cur).strip()
        if not text:
            return
        m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*(\[[^\]]*\])?\s*=", cur[0])
        name = m.group(1) if m else None
        if not name:
            return
        module, _ = prefix_lookup(name)
        if CRT_PAT.match(name):
            module = "crt"
        if module == "unknown":
            # gr core globals use g_*, ogl getter tables use s_*
            if name.startswith("g_"):
                module = "core"
            elif name.startswith("s_"):
                module = "api"
        data_recs.append({"name": name, "module": module, "text": text})

    # vote on data globals by which attributed functions reference them
    def resolve_data_modules(recs):
        for rec in recs:
            if rec["module"] != "unknown":
                continue
            votes = collections.Counter()
            pat = re.compile(r"\b%s\b" % re.escape(rec["name"]))
            for r in manifest:
                if r["module"] == "unknown":
                    continue
                s, e = r["decomp_lines"]
                if pat.search("\n".join(lines[s - 1:e])):
                    votes[r["module"]] += 1
            if votes:
                rec["module"] = votes.most_common(1)[0][0]

    data_recs = []

    while i < n:
        ln = lines[i]
        if ln.startswith("// Data declarations") or ln.startswith("_UNKNOWN loc_"):
            i += 1
            continue
        if ln and not ln.startswith((" ", "\t", "{", "}")):
            # possible start of a top-level declaration
            cur = [ln]

            def decl_done(line):
                code = re.sub(r"//.*$", "", line).rstrip()
                return code.endswith(";")

            depth = ln.count("{") - ln.count("}")
            while cur and (depth > 0 or not decl_done(cur[-1])):
                i += 1
                if i >= n:
                    break
                ln2 = lines[i]
                cur.append(ln2)
                depth += ln2.count("{") - ln2.count("}")
            flush(cur)
            cur = []
        i += 1
    resolve_data_modules(data_recs)
    for rec in data_recs:
        data_chunks[(rec["module"], "data")].append(
            "// ---- data: %s\n%s\n" % (rec["name"], clean_line(rec["text"])))
    for (module, kind), chunks in sorted(data_chunks.items()):
        path = os.path.join(fr_dir, module, "data.fragments.c")
        with open(path, "w") as fh:
            fh.write("/* data declarations attributed to module: %s */\n\n" % module + "\n".join(chunks))

    # manifest outputs
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, "manifest.json"), "w") as fh:
        json.dump({"functions": dedup_manifest}, fh, indent=1)

    # summary
    stat = collections.defaultdict(lambda: collections.defaultdict(int))
    for rec in dedup_manifest:
        s = stat[rec["module"]]
        s["n"] += 1
        s["lines"] += rec["decomp_lines"][1] - rec["decomp_lines"][0] + 1
        s[rec["how"] if rec["how"] in ("assert", "prefix", "none") else "other"] += 1
        s["clones"] += rec["n_copies"] - 1
    with open(os.path.join(OUT_DIR, "MANIFEST.md"), "w") as fh:
        fh.write("# Function inventory (deduped)\n\n")
        fh.write("Total function definitions in monolith: %d\n" % body_count)
        fh.write("Unique functions after clone dedupe: %d\n\n" % len(dedup_manifest))
        fh.write("| module | functions | decomp lines | TU clones | assert | prefix | callsite | none |\n")
        fh.write("|---|---|---|---|---|---|---|---|\n")
        for m in sorted(stat):
            s = stat[m]
            fh.write(f"| {m} | {s['n']} | {s['lines']} | {s['clones']} | {s['assert']} | {s['prefix']} | {s['other']} | {s['none']} |\n")
        fh.write("\n## Functions with divergent TU clones (need manual review)\n\n")
        for name in sorted(clone_diff):
            fh.write(f"- {name}\n")
        fh.write("\n## Unattributed functions\n\n")
        for rec in dedup_manifest:
            if rec["module"] == "unknown":
                fh.write(f"- {rec['name']} (decomp {rec['decomp_lines'][0]})\n")

    print("function bodies parsed:", body_count)
    print("unique functions:", len(dedup_manifest))
    print("divergent clone groups:", len(clone_diff))
    print("fragments dir:", fr_dir)


if __name__ == "__main__":
    main()
