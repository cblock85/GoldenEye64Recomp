# GoldenEye64Recomp

A native macOS (and Linux) port of **GoldenEye 007 (N64)**, built from the [100%-complete
decompilation](https://gitlab.com/kholdfuzion/goldeneye_src) via static recompilation
([N64Recomp](https://github.com/N64Recomp/N64Recomp)) with the
[RT64](https://github.com/rt64/rt64) renderer running on Metal. Based on the
[GoldenRecomp](https://github.com/kholdfuzion/goldenrecomp) project, previously
Windows-only. This is believed to be the first native macOS build of the game —
no emulator involved: the game's MIPS code is translated to C and compiled to
native Apple Silicon (or Intel) machine code.

## No Nintendo assets

This repository contains **no game code, no game assets, and no ROM data**.
You must own a retail GoldenEye 007 NTSC-U cartridge dump. At build time, the
8,404 game functions and the audio microcode are generated **on your machine,
from your ROM**, using the committed symbol maps (`dump.toml` — names and
addresses only). At runtime the app converts your retail dump into the modified
"TLBFREE" ROM layout it needs, using a delta patch — you can also just pick your
retail `.z64` in the ROM picker and the app converts it for you.

## Requirements

- macOS 13+ (Apple Silicon or Intel)
- Full Xcode (for the Metal shader compiler): `sudo xcode-select -s /Applications/Xcode.app`
- [Homebrew](https://brew.sh)
- Your own GoldenEye 007 **NTSC-U** ROM dump, big-endian `.z64`,
  sha1 `abe01e4aeb033b6c0836819f549c791b26cfde83`

## Build

Two build modes:

**Standard build** — the game code is generated from your ROM at build time and
compiled into the executable (fastest at runtime; the binary is personal to you):

```bash
cp /path/to/your/goldeneye.z64 baserom.u.z64
./build_macos.sh          # installs deps, generates game code from your ROM, builds
./make_macos_app.sh       # optional: wraps it into a double-clickable .app bundle
```

**Clean build** — the executable ships with *zero game-derived code*. At first
launch it recompiles all 2,984 game functions from your ROM in memory (~2 seconds)
using N64Recomp's live recompiler. This is the variant suitable for distributing
binaries (e.g. GitHub releases), since the app contains only original code plus
symbol facts:

```bash
./build_macos.sh --clean
./make_macos_app.sh build-clean/GoldenRecomp
```

**Linux** builds the same way with `./build_linux.sh` (add `--clean` for the
distributable variant). Dependencies: `cmake ninja-build clang lld python3
libsdl2-dev libfreetype-dev libgtk-3-dev xdelta3`.

Run `./build/GoldenRecomp` (or open the generated **GoldenEye 007 Recompiled.app**).
On first launch, pick your ROM — retail NTSC-U dumps are converted automatically
(byteswapped `.v64`/`.n64` dumps are handled too). Config and the stored ROM live
in `~/Library/Application Support/GoldenRecomp`.

## State

Working: full intro (Nintendo/Rareware logos, gunbarrel), menus, briefings,
missions, audio, controller support, 60fps+ frame interpolation, widescreen.
Known upstream WIP: skyboxes render black and water is flat (broken in the
Windows build too); multiplayer UI incomplete.

Only the NTSC-U (US) version is supported. PAL/JP dumps are rejected — the
recompiled code is generated against the US symbols; other regions would need
their own recompilation pass (the decomp supports EU, so this is future work).

## What was fixed to get here

Beyond the macOS platform glue, this port surfaced and fixes several bugs that
affect all platforms (drafted upstream reports are in `docs/upstream-issues/`):

1. **N64ModernRuntime**: SP/DP task-completion messages were silently dropped
   when the game's scheduler queue was full, wedging GoldenEye's renderer
   (fixed with a FIFO pending queue in `ultramodern/src/mesgqueue.cpp`).
2. **RT64**: specialized (spec-constant) raster shaders produce broken output on
   the SPIRV/Metal shader paths — geometry using them vanished. Worked around by
   forcing the ubershader path (`rt64_workload_queue.h`, `ubershadersOnly`).
3. **RT64**: frame interpolation interpolated *rotation* unconditionally
   (`FIXME` in tree), so mismatched transform pairs swept garbage across the
   screen for a frame; added a teleport guard mirroring the translation
   heuristic (`rt64_rigid_body.cpp`).
4. **N64Recomp**: the game's `cosf` is a 3-instruction stub that falls through
   into `sinf`; N64Recomp truncates at the fallthrough, so `cosf` returned
   garbage — breaking every CPU-built rotation matrix (Nintendo logo, gunbarrel
   Bond, …). Worked around post-generation in `tools_weaken_patched.py`.

## Credits

- The [GoldenEye decompilation](https://gitlab.com/kholdfuzion/goldeneye_src) team — 100% as of 2026-08-17
- [kholdfuzion](https://github.com/kholdfuzion)'s GoldenRecomp — the Windows recomp this port builds on
- [Mr-Wiseguy](https://github.com/Mr-Wiseguy) — N64Recomp, N64ModernRuntime, and the recomp technique
- [rt64](https://github.com/rt64/rt64) — the renderer
- macOS port, bug fixes, and in-app ROM conversion

## License

The port code and build scripts follow the licenses of the projects they extend
(see `COPYING` and the license files in `lib/`). No Nintendo, Rare, Danjaq, or
EON material is included; GoldenEye 007 is their property. This project is for
preservation and interoperability; you must supply your own legally obtained ROM.
