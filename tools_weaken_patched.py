#!/usr/bin/env python3
"""Mark recompiled originals of RECOMP_PATCH'd functions as weak symbols.

Run this after `./N64Recomp us.toml` regenerates RecompiledFuncs/. It lets the
strong definitions in PatchesLib override the originals portably (GNU ld, lld,
and Apple ld64), without --allow-multiple-definition.
"""
import re, glob, sys, os

os.chdir(os.path.dirname(os.path.abspath(__file__)))

# Collect RECOMP_PATCH'd function names from the patch sources.
names = set()
for path in glob.glob("patches/*.c"):
    src = open(path).read()
    for m in re.finditer(r"RECOMP_PATCH\s+[A-Za-z0-9_*\s]+?([A-Za-z_][A-Za-z0-9_]*)\s*\(", src):
        names.add(m.group(1))

names.add("strtol_recomp")  # helper duplicated by patches

if not names:
    sys.exit("no RECOMP_PATCH functions found")

changed = 0
for path in glob.glob("RecompiledFuncs/funcs_*.c"):
    src = open(path).read()
    orig = src
    for n in sorted(names):
        # N64Recomp appends _recomp when the name collides with a C standard function
        for cand in (n, n + "_recomp"):
            src = src.replace(
                f"RECOMP_FUNC void {cand}(uint8_t* rdram, recomp_context* ctx)",
                f"RECOMP_FUNC __attribute__((weak)) void {cand}(uint8_t* rdram, recomp_context* ctx)")
    if src != orig:
        open(path, "w").write(src)
        changed += 1

print(f"weakened {len(names)} patched functions across {changed} files")

# --- Fallthrough fix for cosf ---------------------------------------------
# The game's cosf (0x80457EA0) is a 3-instruction stub that adds pi/2 to the
# argument and falls through into sinf (0x80457EAC). N64Recomp truncates the
# function at the boundary, so the recompiled cosf returns stale $f0 garbage.
# Append an explicit continuation into sinf.
OLD_TAIL = """    ctx->f12.fl = ctx->f12.fl + ctx->f10.fl;
;}"""
NEW_TAIL = """    ctx->f12.fl = ctx->f12.fl + ctx->f10.fl;
    // FALLTHROUGH FIX: original assembly falls through into sinf (cos(x) = sin(x + pi/2))
    sinf_recomp(rdram, ctx);
;}"""
for path in glob.glob("RecompiledFuncs/funcs_*.c"):
    src = open(path).read()
    if "void cosf_recomp(" in src and "FALLTHROUGH FIX" not in src:
        idx = src.find("void cosf_recomp(")
        end = src.find(";}", idx)
        chunk = src[idx:end+2]
        if OLD_TAIL in chunk:
            src = src[:idx] + chunk.replace(OLD_TAIL, NEW_TAIL) + src[end+2:]
            open(path, "w").write(src)
            print(f"applied cosf fallthrough fix in {path}")
        else:
            print(f"WARNING: cosf_recomp found in {path} but tail pattern didn't match")
