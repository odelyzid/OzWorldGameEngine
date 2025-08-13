#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<EOF
Usage: $0 [editor|game|server] [--debug] [--build-dir DIR]

Commands:
  editor   Build and run the GTK editor
  game     Build and run the SDL2 game demo
  server   Placeholder (not implemented yet)

Options:
  --debug         Use Debug build type (default RelWithDebInfo)
  --build-dir DIR Use custom build directory (default: build)
EOF
}

cmd="editor"
build_type="RelWithDebInfo"
build_dir="build"

while [[ $# -gt 0 ]]; do
  case "$1" in
    editor|game|server) cmd="$1"; shift ;;
    --debug) build_type="Debug"; shift ;;
    --build-dir) build_dir="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown arg: $1"; usage; exit 1 ;;
  esac

done

configure() {
  cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type"
}

build() {
  cmake --build "$build_dir" -j
}

run_editor() {
  if [[ ! -f "$build_dir/oz_editor" ]]; then
    echo "Editor not built or GTK3 not found."
    exit 1
  fi
  # Always prefer OpenGL path. To force software, export GDK_GL=disable explicitly.
  "$build_dir/oz_editor"
}

run_game() {
  if [[ ! -f "$build_dir/oz_demo" ]]; then
    echo "Game demo not built or SDL2/OpenGL not found."
    exit 1
  fi
  # If running over remote X or indirect GL, use the SW-only demo that never touches GLX
  if [[ -n "${DISPLAY:-}" && "${DISPLAY:0:1}" != ":" ]] || [[ -n "${LIBGL_ALWAYS_INDIRECT:-}" ]]; then
    if [[ -f "$build_dir/oz_demo_sw" ]]; then
      "$build_dir/oz_demo_sw"
      return
    fi
  fi
  "$build_dir/oz_demo"
}

case "$cmd" in
  editor)
    configure; build; run_editor ;;
  game)
    configure; build; run_game ;;
  server)
    echo "Server is not yet implemented."; exit 2 ;;
  *)
    usage; exit 1 ;;
 esac
