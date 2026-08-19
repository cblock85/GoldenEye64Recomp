# [RT64] Specialized (spec-constant) raster shaders render incorrectly on SPIRV and Metal backends

**Repo:** rt64

## Summary

Draw calls that use specialized raster shaders from `RasterShaderCache`
(spec-constant compiled) produce no/incorrect output on the Vulkan (SPIRV-Cross)
and Metal backends, while the ubershader path renders the same calls correctly.
On D3D12 both paths are fine, which is presumably why this hasn't been seen on
Windows.

## Symptoms

In GoldenEye 007 (statically recompiled, RT64 HLE): all character models, the
player's gun/hands, and shaded logo geometry simply do not render (a handful of
triangles show as untextured white). Terrain, UI, and any call that happened to
run on the ubershader rendered correctly. Identical behavior on Linux (Vulkan /
lavapipe) and macOS (Metal), on two unrelated GPUs/drivers.

## Evidence it's the spec-constant pipeline

Setting `WorkloadQueue::ubershadersOnly = true` (same flag as the debugger
checkbox) fully fixes rendering on both backends — models, gun, and logos all
appear, with no other change. The display lists were verified complete (the
game emits all geometry; instrumentation showed ~0x700 bytes/frame of character
dlist that never reached the screen).

## Workaround in our tree

```cpp
// rt64_workload_queue.h
std::atomic<bool> ubershadersOnly = true; // WORKAROUND: spec-constant shaders broken on SPIRV/Metal
```

Performance cost is acceptable but obviously not ideal. Happy to help debug the
spec-constant path (suspect: specialization constant handling in the
SPIRV-Cross → MSL conversion, since both non-D3D12 backends share it).
