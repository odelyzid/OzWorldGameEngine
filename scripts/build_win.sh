#!/usr/bin/env bash
set -euo pipefail

# Cross-compile OzWorld for Windows (x86_64) using MinGW-w64 toolchain.
# Requirements:
#   - MinGW-w64 compilers (x86_64-w64-mingw32-gcc/g++)
#   - (Optional) x86_64-w64-mingw32-pkg-config and MinGW SDL2/SDL2_mixer/GTK3 dev packages for Windows
# Usage:
#   ./scripts/build_win.sh [--debug]

BUILD_TYPE=RelWithDebInfo
BUILD_DIR=build-win

if [[ "${1:-}" == "--debug" ]]; then
  BUILD_TYPE=Debug
fi

cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

cmake --build "$BUILD_DIR" -j

echo "Windows build output: $BUILD_DIR"
