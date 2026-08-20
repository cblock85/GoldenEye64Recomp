# N64Recomp (live recompiler): odd single FPR accesses corrupt memory; mtc1 clobbers FPR pair halves; jump tables assumed in-section

**Target repo:** https://github.com/N64Recomp/N64Recomp
**Found while:** live-recompiling all of GoldenEye 007 (US) at runtime — the entire
game (2,984 functions) generated through `LiveGenerator` with
`use_lookup_for_all_function_calls`, as a way to ship a runtime that contains no
game-derived code. Fix commits are in
https://github.com/cblock85/GoldenEye64Recomp (vendored N64Recomp tree).

## 1. Odd single FPR accesses hit a TODO and silently corrupt memory in release builds

`LiveRecomp/live_generator.cpp`, `get_fpr_u32l_context_offset()`:

```cpp
if (fpr_index & 1) {
    // TODO implement odd floats.
    assert(false);
    return -1;
```

In release builds the `assert` compiles out and the function returns **-1**, so
`mtc1`/`mfc1`/`lwc1`/`swc1` on odd-indexed FPRs read and write
`ctx` at byte offset -1. No error is reported; the recompiled code runs and
corrupts adjacent context state. GoldenEye's audio synthesizer builds doubles by
writing the odd half of even/odd FPR pairs (`mtc1 at, $f5` / `mtc1 at, $f9`),
which is common IDO codegen, so any game of that era can hit this.

**Fix:** implement odd accesses the same way the C generator does
(`ctx->f_odd[(idx - 1) * 2]`): cache `ctx->f_odd` in a saved sljit register at
function entry (it is runtime state — it changes with the FR bit), address odd
u32l operands relative to it, and reload it after `emit_cop0_status_write`
(the FR bit may have changed). Costs one saved register (S5; available on
x86-64 SysV and ARM64).

## 2. `mtc1` emits a full-width move, clobbering the adjacent FPR pair word

`process_unary_op` maps `UnaryOpType::None` (used by `mtc1`) to `SLJIT_MOV` —
a 64-bit store into what is semantically a 32-bit float-word field. Writing the
low word of an FPR pair zeroes the high word. Any game that constructs a double
by writing the high (odd) half first and the low (even) half second gets a
corrupted double:

```mips
lui   at, 0x3FF0
mtc1  at, $f9      # high half of double 1.0
mtc1  zero, $f8    # low half — 64-bit MOV zeroes $f9's word again
sub.d $f16, $f0, $f8
```

GoldenEye's `_doModFunc`/`_loadOutputBuffer` (audio effects) do exactly this; the
result was a corrupted audio task list and an RSP microcode abort. Debugging note:
this only breaks hi-then-lo write order, so most code appears to work — it
surfaces as rare, game-specific corruption.

**Fix:** emit `SLJIT_MOV32` when the destination operand is `FdU32L`/`FsU32L`/`FtU32L`.

## 3. `analyze_function` assumes jump tables live in the function's own section

`src/analysis.cpp` computes the jump table's ROM address as
`jtbl.vram + func.rom - func.vram` (marked `// TODO this assumes that the jump
table is in the same section as the function itself`). GoldenEye stores game-code
jump tables in its data segment, producing a wildly out-of-range ROM offset and a
segfault while reading `context.rom`. kholdfuzion's N64Recomp fork already fixes
this for the CLI; the same fix applies here: search `context.sections` for the
section containing the jump table's vram and compute the ROM address from that
section's mapping.

## Repro

Any of the three can be reproduced with the GoldenEye 007 decompilation's
TLBFREE ROM and a symbol context, driving `recompile_function_live` over the
full function set. Happy to provide the exact context files and a minimal
harness.
