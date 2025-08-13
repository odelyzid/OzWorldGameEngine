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
CLEAN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug) BUILD_TYPE=Debug; shift ;;
    --clean) CLEAN=1; shift ;;
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    *) echo "Unknown arg: $1"; echo "Usage: $0 [--debug] [--clean] [--build-dir DIR]"; exit 1 ;;
  esac
done

# If cache belongs to a different source dir, clean automatically
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  SRC_DIR_LINE=$(grep -E "^CMAKE_HOME_DIRECTORY:INTERNAL=" "$BUILD_DIR/CMakeCache.txt" || true)
  THIS_DIR=$(pwd)
  if [[ -n "$SRC_DIR_LINE" ]]; then
    CACHE_DIR=${SRC_DIR_LINE#CMAKE_HOME_DIRECTORY:INTERNAL=}
    if [[ "$CACHE_DIR" != "$THIS_DIR" ]]; then
      echo "[build_win] Detected cache from different source ($CACHE_DIR). Cleaning '$BUILD_DIR'."
      CLEAN=1
    fi
  fi
fi

if [[ $CLEAN -eq 1 ]]; then
  rm -rf "$BUILD_DIR"
fi

cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

cmake --build "$BUILD_DIR" -j

echo "Windows build output: $BUILD_DIR"
