# GoldenEye64Recomp

A native **macOS and Linux** port of **GoldenEye 007 (N64)**, built from the
[100%-complete decompilation](https://gitlab.com/kholdfuzion/goldeneye_src) via static
recompilation ([N64Recomp](https://github.com/N64Recomp/N64Recomp)) with the
[RT64](https://github.com/rt64/rt64) renderer — Metal on macOS, Vulkan on Linux.
Based on the [GoldenRecomp](https://github.com/kholdfuzion/goldenrecomp) project,
which was previously Windows-only.

**This is not an emulator.** The game's original MIPS code is translated into C (or
into machine code at launch, see below) and runs natively on your CPU — Apple
Silicon, Intel, or x86-64 Linux. This is believed to be the first native macOS
build of GoldenEye 007.

## No Nintendo assets

This repository contains **no game code, no game assets, and no ROM data.** You
must own a retail GoldenEye 007 NTSC-U cartridge dump.

What *is* committed is factual symbol data — function names, addresses and sizes
(`dump.toml`, `data_dump.toml`) — plus original port code and a delta patch that
rearranges *your own* dump into the modified "TLBFREE" ROM layout the
recompilation needs. Everything game-derived is produced on your machine, from
your ROM.

## Requirements

**Both platforms**

- Your own GoldenEye 007 **NTSC-U** ROM dump, big-endian `.z64`,
  sha1 `abe01e4aeb033b6c0836819f549c791b26cfde83`
  (byteswapped `.v64` / `.n64` dumps are accepted and converted too)

**macOS**

- macOS 13+ (Apple Silicon or Intel)
- Full Xcode, for the Metal shader compiler:
  `sudo xcode-select -s /Applications/Xcode.app`
  (Command Line Tools alone are not enough)
- [Homebrew](https://brew.sh) — the build script installs the rest

**Linux**

- A Vulkan-capable GPU and drivers
- `cmake ninja-build clang lld python3 libsdl2-dev libfreetype-dev libgtk-3-dev xdelta3`
  (Debian/Ubuntu names; the build script checks and tells you what's missing)

## Build

There are two build modes. Both need your ROM; they differ in *when* the game
code is generated.

### Standard build

The game code is generated from your ROM at build time and compiled into the
executable. Fastest to launch; the resulting binary is personal to you and
should not be redistributed.

```bash
git clone --recurse-submodules https://github.com/cblock85/GoldenEye64Recomp.git
cd GoldenEye64Recomp
cp /path/to/your/goldeneye.z64 baserom.u.z64

./build_macos.sh          # macOS
./build_linux.sh          # Linux
```

(If you already cloned without `--recurse-submodules`, don't worry — the build
scripts fetch what's missing. The `lib/ge` submodule is the decompilation, whose
headers the gameplay patches compile against.)

### Clean build

The executable ships with **zero game-derived CPU code**. At first launch it
recompiles all ~3,000 game functions from your ROM in memory — about two seconds —
using N64Recomp's live recompiler. This is the variant suitable for distributing
binaries, since the app itself contains only original code plus symbol facts.

```bash
./build_macos.sh --clean  # macOS
./build_linux.sh --clean  # Linux
```

(The audio microcode is still translated at build time, so a ROM is needed once
even for a clean build.)

### macOS app bundle

Optional, wraps the binary into a double-clickable `.app` with an icon:

```bash
./make_macos_app.sh                          # standard build
./make_macos_app.sh build-clean/GoldenRecomp # clean build
```

## Running

```bash
./build/GoldenRecomp          # standard build
./build-clean/GoldenRecomp    # clean build
```

or open **GoldenEye 007 Recompiled.app** on macOS.

On first launch, pick your ROM in the picker — a plain retail NTSC-U dump works
directly, and the app converts it for you. Settings and the stored ROM live in
`~/Library/Application Support/GoldenRecomp` (macOS) or `~/.config/GoldenRecomp`
(Linux); delete the stored `.z64` there if you ever want the ROM picker back.

## State

**Working:** the full intro (Nintendo and Rareware logos, gunbarrel), menus,
file select, briefings, missions, audio, controller support, frame interpolation
to your display's refresh rate, and widescreen.

**Known issues**, inherited from upstream GoldenRecomp and present in the Windows
build too: skyboxes render black and water is flat (the game draws them with
custom microcode commands RT64 doesn't implement); the multiplayer UI is
incomplete; some weapons fire too fast because the game runs at native 60Hz.

**Region:** only NTSC-U (US) is supported. Other regions are rejected — the
symbol data here is generated against the US ROM. The decompilation supports EU,
so PAL support is possible future work.

## What was fixed to get here

Beyond the macOS and Linux platform glue, this port surfaced and fixed a number
of bugs affecting all platforms. Drafted upstream reports live in
`docs/upstream-issues/`.

**N64ModernRuntime**

1. SP/DP task-completion messages were silently dropped when the game's scheduler
   queue was full, wedging GoldenEye's renderer. Fixed with a FIFO pending queue
   in `ultramodern/src/mesgqueue.cpp`.

**RT64**

2. Specialized (spec-constant) raster shaders produce broken output on the
   SPIRV/Metal shader paths, so any geometry using them silently vanished — no
   character models, no weapon in hand. Worked around by forcing the ubershader
   path (`ubershadersOnly` in `rt64_workload_queue.h`).
3. Frame interpolation interpolated *rotation* unconditionally (a `FIXME` in
   tree), so mismatched transform pairs swept garbage geometry across the screen
   for a frame every few seconds while moving. Added a teleport guard mirroring
   the existing translation heuristic (`rt64_rigid_body.cpp`).

**N64Recomp (static)**

4. The game's `cosf` is a 3-instruction stub that *falls through* into `sinf`
   (`cos(x) = sin(x + pi/2)`). N64Recomp truncates functions at the fallthrough
   boundary, so `cosf` returned stale garbage — breaking every CPU-built rotation
   matrix in the game (the Nintendo logo zoomed past the camera; Bond was a white
   triangle in the gunbarrel). Repaired post-generation in
   `tools_weaken_patched.py`.

**N64Recomp (live recompiler)** — found while making the clean build work:

5. Odd-indexed single FPR accesses (`mtc1 $f5`, …) hit a `TODO`/`assert` that
   compiles out in release builds, returning context offset `-1` and silently
   corrupting memory. Implemented properly via the runtime `f_odd` pointer.
6. `mtc1` emitted a full-width 64-bit move into a 32-bit float-word slot,
   clobbering the adjacent half of the FPR pair — corrupting any double built
   high-half-first, which is what GoldenEye's audio synthesizer does.
7. Jump-table analysis assumed jump tables live in the same section as the
   function using them; GoldenEye keeps game-code jump tables in its data
   segment, which crashed the analyzer.

## Credits

- The [GoldenEye decompilation](https://gitlab.com/kholdfuzion/goldeneye_src) team — 100% as of 2026-08-17
- [kholdfuzion](https://github.com/kholdfuzion)'s GoldenRecomp — the Windows recomp this port builds on
- [Mr-Wiseguy](https://github.com/Mr-Wiseguy) — N64Recomp, N64ModernRuntime, and the recomp technique
- [RT64](https://github.com/rt64/rt64) — the renderer
- macOS/Linux port, bug fixes, live-recompilation mode, and in-app ROM conversion

## License

The port code and build scripts follow the licenses of the projects they extend
(see `COPYING` and the license files under `lib/`). No Nintendo, Rare, Danjaq, or
EON material is included; GoldenEye 007 is their property. This project exists for
preservation and interoperability, and requires you to supply your own legally
obtained ROM.
