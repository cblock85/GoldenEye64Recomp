#!/bin/bash
# GoldenEye 007: Recompiled — Linux build script
# Usage:
#   ./build_linux.sh          # standard build (game code compiled in at build time; needs ROM present)
#   ./build_linux.sh --clean  # "clean" build: ships NO game code; recompiles from your ROM at launch
set -e
cd "$(dirname "$0")"

MODE_FLAG=""
BUILD_DIR="build"
if [ "$1" = "--clean" ]; then
    MODE_FLAG="-DLIVE_GAMECODE=ON"
    BUILD_DIR="build-clean"
fi

echo "== [1/4] Checking dependencies"
MISSING=""
for tool in cmake ninja clang ld.lld python3; do
    command -v $tool >/dev/null 2>&1 || MISSING="$MISSING $tool"
done
if [ -n "$MISSING" ]; then
    echo "Missing tools:$MISSING"
    echo "On Debian/Ubuntu: sudo apt install cmake ninja-build clang lld python3 \\"
    echo "    libsdl2-dev libfreetype-dev libgtk-3-dev xdelta3"
    exit 1
fi

echo "== [2/4] ROM preparation"
if [ "$BUILD_DIR" = "build" ]; then
    # Standard build needs the TLBFREE ROM at build time to generate the game code.
    if [ ! -f ge007.tlbfree.z64 ]; then
        if [ -f baserom.u.z64 ]; then
            xdelta3 -d -s baserom.u.z64 vanilla_to_tlbfree.xdelta ge007.tlbfree.z64
        else
            echo "ERROR: place your NTSC-U GoldenEye ROM here as baserom.u.z64"
            exit 1
        fi
    fi
    if [ ! -f RecompiledFuncs/funcs_0.c ]; then
        echo "Generating recompiled game code from the ROM..."
        [ -f ./N64Recomp ] || { cmake -S n64recomp-src -B n64recomp-src/build -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null && ninja -C n64recomp-src/build N64Recomp && cp n64recomp-src/build/N64Recomp ./N64Recomp; }
        ./N64Recomp us.toml
        python3 tools_weaken_patched.py
    fi
else
    # Clean build: the CPU game code is generated at app launch from the user's
    # ROM, but the audio microcode (aspMain) is still translated at build time.
    if [ ! -f rsp/aspMain.cpp ]; then
        if [ ! -f ge007.tlbfree.z64 ]; then
            if [ -f baserom.u.z64 ]; then
                xdelta3 -d -s baserom.u.z64 vanilla_to_tlbfree.xdelta ge007.tlbfree.z64
            else
                echo "ERROR: place your NTSC-U GoldenEye ROM here as baserom.u.z64"
                echo "(needed once, to translate the audio microcode)"
                exit 1
            fi
        fi
        if [ ! -f ./RSPRecomp ]; then
            cmake -S n64recomp-src -B n64recomp-src/build -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
            ninja -C n64recomp-src/build RSPRecomp
            cp n64recomp-src/build/RSPRecomp ./RSPRecomp
        fi
        ./RSPRecomp aspMain.us.toml
    fi
fi

# The patches build step needs the N64Recomp tool in all modes.
if [ ! -f ./N64Recomp ]; then
    cmake -S n64recomp-src -B n64recomp-src/build -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
    ninja -C n64recomp-src/build N64Recomp
    cp n64recomp-src/build/N64Recomp ./N64Recomp
fi

echo "== [3/4] Configuring"
cmake -S . -B $BUILD_DIR -G Ninja -DCMAKE_BUILD_TYPE=Release $MODE_FLAG

echo "== [4/4] Building"
ninja -C $BUILD_DIR GoldenRecomp

echo ""
echo "Done! Run with:  ./$BUILD_DIR/GoldenRecomp"
echo "Pick your retail NTSC-U .z64 in the ROM picker — it converts automatically."
