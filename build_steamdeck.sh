#!/bin/bash
# GoldenEye 007: Recompiled — Steam Deck / Steam Linux Runtime 3.0 ("sniper") build
# Usage:
#   ./build_steamdeck.sh          # standard build  -> build-sniper/GoldenRecomp
#   ./build_steamdeck.sh --clean  # "clean" build   -> build-sniper-clean/GoldenRecomp
#
# The binary from build_linux.sh links against the host's glibc, which on a current distro is far
# newer than the 2.31 in the sniper runtime (and than SteamOS). This compiles and links inside Valve's
# sniper SDK container instead, so the result only needs glibc 2.31.
#
# Needs podman. The generated sources (recompiled game code, patches, audio microcode, shaders) are
# produced on the host first by build_linux.sh: they don't depend on glibc, and the patch step needs
# clang + ld.lld, which the SDK doesn't ship.
set -e
cd "$(dirname "$0")"

IMAGE="registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest"
MODE_FLAG=""
BUILD_DIR="build-sniper"
HOST_BUILD_DIR="build"
if [ "$1" = "--clean" ]; then
    MODE_FLAG="-DLIVE_GAMECODE=ON"
    BUILD_DIR="build-sniper-clean"
    HOST_BUILD_DIR="build-clean"
fi

command -v podman >/dev/null 2>&1 || { echo "podman is required"; exit 1; }

echo "== [1/3] Generating sources on the host (build_linux.sh)"
./build_linux.sh "$@"

echo "== [2/3] Compiling in the sniper SDK container"
# Two host-built tools run during the build and need the host's newer glibc:
#  - ./N64Recomp regenerates the patches: build a sniper copy and mount it over ./N64Recomp.
#  - rt64's bundled dxc compiles shaders: its output doesn't depend on glibc, and the host build above
#    already produced every shader, so a stand-in copies the matching file from the host build dir.
# Both are mounted inside the container only; the host files are left alone.
DXC_BIN="lib/rt64/src/contrib/dxc/bin/x64/dxc-linux"
SHIM_DIR="$BUILD_DIR-tools"
mkdir -p "$SHIM_DIR"
cat > "$SHIM_DIR/dxc-shim" <<EOF
#!/bin/sh
# Stand-in for dxc inside the sniper container: copy the output the host build already produced.
out=""
while [ \$# -gt 0 ]; do
    case "\$1" in /Fo|-Fo) out="\$2"; shift ;; esac
    shift
done
[ -n "\$out" ] || { echo "dxc-shim: no /Fo output given" >&2; exit 1; }
src=\$(printf '%s' "\$out" | sed "s|/$BUILD_DIR/|/$HOST_BUILD_DIR/|")
[ -f "\$src" ] || { echo "dxc-shim: host build has no \$src, run build_linux.sh first" >&2; exit 1; }
cp "\$src" "\$out"
EOF
chmod +x "$SHIM_DIR/dxc-shim"

podman run --rm --userns=keep-id -v "$PWD:$PWD" -w "$PWD" "$IMAGE" bash -c "
    set -e
    cmake -S n64recomp-src -B n64recomp-src/build-sniper -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 >/dev/null
    ninja -C n64recomp-src/build-sniper N64Recomp
"
podman run --rm --userns=keep-id -v "$PWD:$PWD" -w "$PWD" \
    -v "$PWD/n64recomp-src/build-sniper/N64Recomp:$PWD/N64Recomp:ro" \
    -v "$PWD/$SHIM_DIR/dxc-shim:$PWD/$DXC_BIN:ro" "$IMAGE" bash -c "
    set -e
    # -ldl at the end of the link line: before glibc 2.34 dlopen/dlclose live in libdl, and CMake puts
    # -ldl ahead of librecomp.a, which uses them.
    cmake -S . -B '$BUILD_DIR' -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_CXX_STANDARD_LIBRARIES=-ldl $MODE_FLAG
    ninja -C '$BUILD_DIR' GoldenRecomp
"

echo "== [3/3] Checking glibc requirement"
MAX_GLIBC=$(objdump -T "$BUILD_DIR/GoldenRecomp" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1)
echo "Highest glibc symbol version needed: $MAX_GLIBC (sniper runtime has GLIBC_2.31)"

echo ""
echo "Done! Copy $BUILD_DIR/GoldenRecomp (plus assets) to the Deck."
