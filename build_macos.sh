#!/bin/bash
# GoldenEye 007: Recompiled — macOS build script
# Produces build/GoldenRecomp. Tested layout: this kit extracted anywhere, run from the kit root.
set -e
cd "$(dirname "$0")"

echo "== [1/6] Checking prerequisites"
if ! xcode-select -p >/dev/null 2>&1; then
    echo "ERROR: Xcode is required (for the Metal shader compiler)."
    echo "Install Xcode from the App Store, then run: sudo xcode-select -s /Applications/Xcode.app"
    exit 1
fi
if ! command -v brew >/dev/null 2>&1; then
    echo "ERROR: Homebrew is required. Install from https://brew.sh"
    exit 1
fi

echo "== [2/6] Installing dependencies (brew)"
brew list cmake >/dev/null 2>&1 || brew install cmake
brew list ninja >/dev/null 2>&1 || brew install ninja
brew list sdl2 >/dev/null 2>&1 || brew install sdl2
brew list freetype >/dev/null 2>&1 || brew install freetype
brew list llvm >/dev/null 2>&1 || brew install llvm
brew list lld >/dev/null 2>&1 || brew install lld
brew list xdelta >/dev/null 2>&1 || brew install xdelta

# CMake 4.x compat for old third-party cmake_minimum_required declarations
export CMAKE_POLICY_VERSION_MINIMUM=3.5
# The bundled fmt library's consteval format checking breaks on newer Apple clang
export CXXFLAGS="-DFMT_USE_CONSTEVAL=0 ${CXXFLAGS:-}"

LLVM_BIN="$(brew --prefix llvm)/bin"
LLD_BIN="$(brew --prefix lld)/bin"
export PATH="$LLVM_BIN:$LLD_BIN:$PATH"

echo "== [3/6] Preparing the TLBFREE ROM (needed to generate the game code)"
if [ ! -f ge007.tlbfree.z64 ]; then
    if [ -f baserom.u.z64 ]; then
        echo "Converting your retail NTSC-U ROM..."
        xdelta3 -d -s baserom.u.z64 vanilla_to_tlbfree.xdelta ge007.tlbfree.z64
    else
        echo "ERROR: place your NTSC-U GoldenEye ROM here as baserom.u.z64"
        echo "(sha1 must be abe01e4aeb033b6c0836819f549c791b26cfde83)"
        echo "The generated game code is derived from YOUR ROM — this repo ships none of it."
        exit 1
    fi
fi

echo "== [4/6] Building the N64Recomp tools"
if [ ! -f ./N64Recomp ] || [ -d ./N64Recomp ]; then
    rm -rf ./N64Recomp 2>/dev/null || true
    cmake -S n64recomp-src -B n64recomp-src/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    ninja -C n64recomp-src/build N64Recomp RSPRecomp
    cp n64recomp-src/build/N64Recomp ./N64Recomp
    cp n64recomp-src/build/RSPRecomp ./RSPRecomp 2>/dev/null || true
fi

echo "== [4b/6] Generating the recompiled game code from your ROM"
if [ ! -d RecompiledFuncs ]; then
    ./N64Recomp us.toml
    python3 tools_weaken_patched.py
fi
if [ ! -f rsp/aspMain.cpp ]; then
    ./RSPRecomp aspMain.us.toml
fi

echo "== [5/6] Un-quarantining bundled shader compiler"
chmod +x lib/rt64/src/contrib/dxc/bin/arm64/dxc-macos 2>/dev/null || true
xattr -dr com.apple.quarantine lib/rt64/src/contrib/dxc 2>/dev/null || true

echo "== [6/6] Building GoldenRecomp"
# Pass --clean to build the distributable variant: it ships NO game code and
# recompiles everything from the user's ROM in-memory at launch.
MODE_FLAG=""
BUILD_DIR="build"
if [ "$1" = "--clean" ]; then
    MODE_FLAG="-DLIVE_GAMECODE=ON"
    BUILD_DIR="build-clean"
fi
# The patches step cross-compiles MIPS patch code with brew clang + ld.lld.
cmake -S . -B $BUILD_DIR -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 $MODE_FLAG
ninja -C $BUILD_DIR GoldenRecomp

echo ""
echo "Done! Run with:  ./$BUILD_DIR/GoldenRecomp"
echo "On first launch pick your retail NTSC-U .z64 in the ROM picker —"
echo "the app converts it automatically (ge007.tlbfree.z64 also works)."
