# [N64Recomp] Functions that fall through into the next function are silently truncated

**Repo:** N64Recomp

## Summary

A function that ends without a terminal jump — falling through into the next
function in memory — is recompiled as only its own instructions, with no
continuation. The recompiled function then "returns" with whatever happened to
be in the return registers, i.e. garbage, and nothing warns about it.

## Real-world case

GoldenEye 007's `cosf` (0x80457EA0, 12 bytes) is the classic trick:

```asm
lui   $at, %hi(pi_over_2)
lwc1  $f10, %lo(pi_over_2)($at)
add.s $f12, $f12, $f10        ; x += pi/2
; falls through into sinf at 0x80457EAC
```

The recompiled `cosf` performs the addition and returns — `$f0` is stale, so
every caller gets garbage cosine values. In-game this broke every CPU-built
rotation matrix: the Nintendo boot logo rendered zoomed ~4x through the camera,
and Bond in the gunbarrel intro was a single corrupted triangle. It took a
while to trace because nothing fails at recompile time.

## Suggested behavior

When a function's last instruction is not an unconditional terminal branch and
the next byte belongs to another known function, either:

1. emit a tail-call to the recompiled next function (what we patch in
   post-generation — semantically exact), or
2. at minimum, emit a warning naming the function so porters know to handle it.

Our post-generation workaround simply appends `sinf_recomp(rdram, ctx);` to the
truncated `cosf`. A general implementation could do the same for any
fallthrough boundary where the successor is a known function start.
