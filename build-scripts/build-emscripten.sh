#!/bin/bash
# Build the SDL3 browser target with a pinned Emscripten SDK.
set -exo pipefail

CCACHE=${CCACHE:-0}
EMSDK_VERSION=${EMSDK_VERSION:-6.0.8}
if command -v nproc >/dev/null 2>&1; then
    DEFAULT_JOBS=$(nproc)
else
    DEFAULT_JOBS=$(sysctl -n hw.logicalcpu)
fi
JOBS=${JOBS:-$DEFAULT_JOBS}
BUILD_TARGET=${BUILD_TARGET:-cataclysm-tiles.js}

emsdk install "$EMSDK_VERSION"
emsdk activate "$EMSDK_VERSION"

make -j"$JOBS" NATIVE=emscripten CLANG=1 BACKTRACE=0 TILES=1 TESTS=0 RUNTESTS=0 RELEASE=1 CCACHE="$CCACHE" LINTJSON=0 "$BUILD_TARGET"
