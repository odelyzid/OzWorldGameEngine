#!/usr/bin/env bash

set -euo pipefail

# Persistent config
CONFIG_FILE="$HOME/.ozconf"

# Repo root (script is in scripts/)
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

DEFAULT_APP="oz_demo"
DEFAULT_RENDER_MODE="gl"
DEFAULT_DEBUG_MODE="off"
DEFAULT_DEBUG_LEVEL="0"

APP="$DEFAULT_APP"
RENDER_MODE="$DEFAULT_RENDER_MODE"
DEBUG_MODE="$DEFAULT_DEBUG_MODE"
DEBUG_LEVEL="$DEFAULT_DEBUG_LEVEL"

info() { echo "[ozworld] $*"; }
warn() { echo "[ozworld] WARNING: $*" >&2; }
err()  { echo "[ozworld] ERROR: $*" >&2; exit 1; }

load_config() {
	if [[ -f "$CONFIG_FILE" ]]; then
		# shellcheck source=/dev/null
		. "$CONFIG_FILE" || true
 
 		# Fallbacks if keys missing
 		APP="${APP:-$DEFAULT_APP}"
 		RENDER_MODE="${RENDER_MODE:-$DEFAULT_RENDER_MODE}"
 		DEBUG_MODE="${DEBUG_MODE:-$DEFAULT_DEBUG_MODE}"
 		DEBUG_LEVEL="${DEBUG_LEVEL:-$DEFAULT_DEBUG_LEVEL}"
 	fi
}

save_config() {
	cat >"$CONFIG_FILE" <<EOF
APP=$APP
RENDER_MODE=$RENDER_MODE
DEBUG_MODE=$DEBUG_MODE
DEBUG_LEVEL=$DEBUG_LEVEL
EOF
}

lower() { tr '[:upper:]' '[:lower:]'; }

prompt_with_default() {
	# $1: prompt text, $2: current value, $3: valid values (space-separated)
	local prompt current valid input
	prompt="$1"
 	current="$2"
 	valid="$3"
 	while true; do
 		read -r -p "$prompt [$valid] (default: $current): " input || true
 		input="${input:-$current}"
 		input="$(printf '%s' "$input" | lower)"
 		for v in $valid; do
 			if [[ "$input" == "$v" ]]; then
 				printf '%s' "$input"
 				return 0
 			fi
 		done
 		warn "Invalid selection: '$input'. Valid: $valid"
 	done
}

ensure_display() {
	if [[ -z "${DISPLAY:-}" ]]; then
 		# Best-effort: infer Windows host IP from resolv.conf
 		if host_ip=$(awk '/^nameserver /{print $2; exit}' /etc/resolv.conf 2>/dev/null); then
 			export DISPLAY="$host_ip:0"
 			info "DISPLAY was not set; using DISPLAY=$DISPLAY"
 		else
 			warn "DISPLAY is not set; GUI may not work. Start VcXsrv and set DISPLAY."
 		fi
 	fi
}

apply_render_env() {
	case "$RENDER_MODE" in
 		software)
 			export OZ_FORCE_SOFTWARE=1
 			export SDL_VIDEODRIVER=x11
 			export SDL_RENDER_DRIVER=software
 			export SDL_VIDEO_X11_FORCE_EGL=1
 			export LIBGL_ALWAYS_INDIRECT=1
 			export LIBGL_ALWAYS_SOFTWARE=1
 			info "Configured software rendering environment (ensure VcXsrv started with -nowgl -ac)"
 			;;
 		gl)
 			unset OZ_FORCE_SOFTWARE || true
 			unset SDL_VIDEODRIVER || true
 			unset SDL_RENDER_DRIVER || true
 			unset SDL_VIDEO_X11_FORCE_EGL || true
 			unset LIBGL_ALWAYS_INDIRECT || true
 			unset LIBGL_ALWAYS_SOFTWARE || true
 			info "Configured OpenGL rendering environment"
 			;;
 		*) err "Unknown render mode: $RENDER_MODE";;
 	esac
}

build_project() {
	pushd "$ROOT_DIR" >/dev/null
 	if [[ "$DEBUG_MODE" == "on" ]]; then
 		info "Building (Debug) via 'make debug'"
 		make debug -j"$(nproc)"
 	else
 		info "Building (RelWithDebInfo) via 'make'"
 		make -j"$(nproc)"
 	fi
 	popd >/dev/null
}

resolve_binary() {
	local app="$1" mode="$2"
 	local bin
 	case "$app" in
 		oz_demo)
 			if [[ "$mode" == "software" ]]; then
 				bin="oz_demo_sw"
 			else
 				bin="oz_demo"
 			fi
 			;;
 		oz_demo_sw)
 			bin="oz_demo_sw"
 			RENDER_MODE="software" # enforce
 			;;
 		oz_editor)
 			bin="oz_editor"
 			;;
 		*) err "Unknown app: $app";;
 	esac
 	printf '%s' "$bin"
}

launch_app() {
	local bin_path="$1"
 	export OZ_DEBUG_LEVEL="$DEBUG_LEVEL"
 	info "Launching: $bin_path (OZ_DEBUG_LEVEL=$OZ_DEBUG_LEVEL)"
 	exec "$bin_path"
}

main() {
	load_config

 	info "OzWorld Launcher"
 	info "Repo: $ROOT_DIR"
 	info "Previous settings -> APP=$APP, RENDER_MODE=$RENDER_MODE, DEBUG_MODE=$DEBUG_MODE, DEBUG_LEVEL=$DEBUG_LEVEL"

 	APP="$(prompt_with_default "Select application" "$APP" "oz_demo oz_demo_sw oz_editor")"
 	RENDER_MODE="$(prompt_with_default "Select render mode" "$RENDER_MODE" "gl software")"
 	DEBUG_MODE="$(prompt_with_default "Enable debug build?" "$DEBUG_MODE" "on off")"
 	DEBUG_LEVEL="$(prompt_with_default "Select debug level" "$DEBUG_LEVEL" "0 1 2")"

    # Map to binary (may enforce software mode for oz_demo_sw)
    local binary_name
    binary_name="$(resolve_binary "$APP" "$RENDER_MODE")"
    if [[ "$APP" == "oz_demo_sw" && "$RENDER_MODE" != "software" ]]; then
        info "Enforcing software render mode for oz_demo_sw"
    fi

    # Apply env and basic checks (after enforcement)
    ensure_display
    apply_render_env

    # Persist final (possibly enforced) settings
    save_config
    info "Saved settings to $CONFIG_FILE"

 	# Build
 	build_project

 	# Verify executable exists
 	local exe_path="$BUILD_DIR/$binary_name"
 	if [[ ! -x "$exe_path" ]]; then
 		# Some generators may produce without +x, or build may have failed
 		if [[ -f "$exe_path" ]]; then chmod +x "$exe_path" || true; fi
 	fi
 	[[ -x "$exe_path" ]] || err "Executable not found or not executable: $exe_path"

 	# Launch
 	launch_app "$exe_path"
}

main "$@"


