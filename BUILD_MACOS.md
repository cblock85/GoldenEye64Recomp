# GoldenEye 007: Recompiled — macOS Build Kit

A native port of GoldenEye 007, built from the 100%-complete decompilation via static
recompilation (N64Recomp) with the RT64 renderer (Metal on macOS). This kit contains
no Nintendo-copyrighted assets: you supply your own retail NTSC-U ROM dump, which gets
converted locally into the modified "TLBFREE" ROM the recompilation needs.

## What's in this kit

- `GoldenRecomp` source tree (game code is NOT included — all 8,404 functions and the
  audio microcode are generated from your own ROM by `build_macos.sh`)
- `n64recomp/` — source for the N64Recomp tool (built automatically; needed by the
  patches step of the build)
- `vanilla_to_tlbfree.xdelta` — converts your retail ROM into the TLBFREE ROM
- `patches/` — gameplay/rendering patches (widescreen, 60fps interpolation, skybox fixes…)
- `build_macos.sh` — one-shot build script

## Requirements

- macOS 13+ (Apple Silicon or Intel; the bundled shader compiler is universal)
- Xcode (full install, for the Metal shader compiler — not just Command Line Tools)
- Homebrew
- Your own GoldenEye 007 NTSC-U ROM dump, big-endian `.z64`,
  sha1 `abe01e4aeb033b6c0836819f549c791b26cfde83`

## Build

```bash
cp /path/to/your/goldeneye.z64 baserom.u.z64
./build_macos.sh
./build/GoldenRecomp
```

On first launch, choose your retail NTSC-U `.z64` in the ROM picker — the app detects
it and converts it to the TLBFREE ROM automatically (a pre-converted
`ge007.tlbfree.z64` is also accepted). Byteswapped `.v64`/`.n64` dumps are handled too.

## Known state / expectations

This is pioneer-territory software: the upstream GoldenRecomp project is itself WIP
(multiplayer UI incomplete, some sky/water effects still being reworked), and this is
the first time it has been built anywhere other than Windows. The Linux build from the
same tree boots, renders the intro, and plays audio. The macOS platform glue
(`CMakeLists.txt` APPLE branches) is wired but has not yet been compile-tested on a
real Mac — expect the first build to surface a few small errors. Fix trivially or
report back for help.

Troubleshooting notes:

- `clang: unsupported option '-target mips'` during the patches step → the build used
  Apple clang instead of Homebrew LLVM. Re-run via `./build_macos.sh` (it puts brew
  llvm/lld first in PATH), or export PATH yourself.
- `xcrun: unable to find utility "metal"` → full Xcode isn't installed/selected:
  `sudo xcode-select -s /Applications/Xcode.app`
- dxc "cannot be opened because the developer cannot be verified" → run
  `xattr -dr com.apple.quarantine lib/rt64/src/contrib/dxc` (the script does this).
- Regenerating the recompiled game code (only needed if you change the decomp):
  `./N64Recomp us.toml && ./N64Recomp us.toml --dump-context && python3 tools_weaken_patched.py`

## Provenance of the recompilation (what was done upstream of this kit)

1. The GoldenEye decomp (100% as of 2026-08-17) was modified to remove all TLB usage:
   the game's 907KB code segment, originally demand-paged through the CPU's TLB at
   virtual `0x7F000000`, is now fully resident at `0x80400000` (Expansion Pak memory),
   and the compressed data segment is stored raw in ROM. This recreates the private
   `TBLFREE_NOCOMPRESSION` branch that GoldenRecomp requires.
2. The modified ROM was verified to boot and behave identically to retail in an
   instrumented emulator.
3. N64Recomp converted all 8,404 functions of the resulting ELF into C; RSPRecomp
   converted the audio microcode.
4. Two upstream runtime bugs exposed by this game were fixed in `lib/N64ModernRuntime`
   (silently dropped SP/DP-completion messages wedging GoldenEye's scheduler).
