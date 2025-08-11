#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Please run as root: sudo $0"
  exit 1
fi

apt-get update
apt-get install -y \
  build-essential cmake ninja-build pkg-config \
  libsdl2-dev libgl1-mesa-dev \
  libgtk-3-dev \
  xorg xauth x11-apps mesa-utils \
  mingw-w64

# Configure WSL2 external X server (VcXsrv) environment for Windows 10
# Creates a profile script that sets DISPLAY and related vars at login
install -d -m 0755 /etc/profile.d
cat > /etc/profile.d/wsl2-x11.sh <<'EOS'
# Auto-configure DISPLAY for WSL2 with external X server (e.g., VcXsrv)
if grep -qi microsoft /proc/version 2>/dev/null; then
  # If WSLg (Windows 11) is present, do nothing
  if [ ! -S "/mnt/wslg/PulseServer" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    # Use Windows host IP from resolv.conf (works when VcXsrv listens beyond localhost)
    windows_host_ip=$(awk '/nameserver/ {print $2; exit}' /etc/resolv.conf 2>/dev/null)
    if [ -n "$windows_host_ip" ] && [ -z "${DISPLAY:-}" ]; then
      export DISPLAY="$windows_host_ip:0.0"
    fi
    # Do not force indirect GL globally; let server mode decide
    :
    # Ensure XDG runtime dir exists to avoid GTK warnings
    if [ -z "${XDG_RUNTIME_DIR:-}" ]; then
      export XDG_RUNTIME_DIR="/tmp/xdg-$(id -u)"
      [ -d "$XDG_RUNTIME_DIR" ] || mkdir -p -m 700 "$XDG_RUNTIME_DIR"
    fi
  fi
fi
EOS

cat <<'EOF'
WSL2 notes:
- For GUI (SDL2/GTK3/OpenGL) on Windows 10, install and start VcXsrv on Windows with options: Multi-window, Clipboard, Disable access control, and with Native OpenGL unchecked.
- This script installed `/etc/profile.d/wsl2-x11.sh` to auto-export DISPLAY=<WindowsHostIP>:0.0 and LIBGL_ALWAYS_INDIRECT=1 for VcXsrv. Open a new shell to load it.
- Validate OpenGL: run `glxinfo -B` and `glxgears` while VcXsrv is running.
- Cross-compilation: configure with
    cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake
EOF
