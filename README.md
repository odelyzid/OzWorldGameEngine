OzWorld - Cross-Platform 90s-Style Game Engine

This repository scaffolds a cross-platform C/SDL2/OpenGL engine with a nostalgic 90s feel, designed for primary development on WSL2 while targeting both Linux and Windows. It includes:

- A tiny core engine library (`ozcore`) with logging, versioning, basic math, and a rudimentary BSP/map module for brush-based geometry
- An SDL2/OpenGL platform layer (`ozplatform_sdl`) providing windowing, GL context, input, timing
- A simple demo app (`oz_demo`) that renders basic BSP brushes (box, cylinder) with a free-move camera
- A GTK3-based editor (`oz_editor`) that can open/save `.ozone` map files (legacy `.ozmap` still loads), adjust primitive brush dimensions, and show a live viewport
- Asset conventions: textures `.oztex`, mesh bundles `.ozbag`, music `.ozmux` (MP3 content). Editor offers Import dialogs for these.
- A minimal HTTP server (`oz_server`) for future map-serving/authentication prototypes

Quick start
- Linux/WSL2: see scripts/setup_wsl2.sh or the sections below for package installation.
- Build: see the Build section. CMake and Makefile supported.

Directory layout
- `include/oz`: public headers for the core engine API
- `src/core`: engine core (logging, core, bsp/map)
- `src/platform`: platform abstraction implementations per OS (SDL2/OpenGL)
- `apps/oz_demo`: example app using the engine/platform to render brushes
- `apps/oz_server`: simple HTTP server prototype for map loading and auth
- `editor`: GTK3-based map editor with a GL or software viewport fallback
- `cmake/toolchains`: toolchain files (e.g., MinGW for Windows cross-compile)
- `scripts`: helper scripts (setup, run, etc.)

Build
- CMake (recommended):
  - Native Linux: `cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo`
  - Cross-compile to Windows (WSL2): `cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo`
  - Build: `cmake --build build -j` or `cmake --build build-win -j`
- Makefile shim: `make` will call into CMake.

Run
- Demo: `./build/oz_demo`
  - Controls: WASD move, QE up/down, arrows rotate camera
- Editor: `./build/oz_editor`
- File → Open/Save: load/save `.ozone`
  - BSP → Brushes → Box... / Cylinder...: set dimensions, adds a brush at origin
  - Viewport: uses GtkGLArea when available; falls back to software (Cairo) rendering if GL is not available
- Server (UNIX): `./build/oz_server`
- GET `/map?name=sample.ozone` returns JSON; POST `/auth/login` returns a dummy token

Current engine/core
- Logging: `oz_log` with colored levels
- Core: version query via `oz_core_version`
- Platform (SDL2/OpenGL): window, GL context, clear/swap, time, sleep, key input snapshot
- BSP/Map: primitive brushes (box, cylinder), dynamic `OzMap`, simple text save/load format (`OZONE 1`, loads legacy `OZMAP 1`)
  - Example lines:
    - `box cx cy cz sx sy sz`
    - `cyl cx cy cz rx ry h segments`

Planned
- Plane-based convex brushes and CSG operations (add/sub/intersect)
- Editor selection/manipulation and proper 3D camera controls
- Server-backed map loading/auth

WSL2 setup
- Run `sudo scripts/setup_wsl2.sh` inside WSL2 to install packages and configure X11.
- Windows 10 + VcXsrv:
  - Install VcXsrv on Windows.
  - Launch with: Multi-window, Clipboard, Primary selection, Disable access control, Native OpenGL unchecked.
  - Open a new WSL shell; the script creates `/etc/profile.d/wsl2-x11.sh` that sets `DISPLAY` and `LIBGL_ALWAYS_INDIRECT`.
  - Test: ensure VcXsrv is running, then run `glxinfo -B` and `glxgears` in WSL.

Troubleshooting

Build fails
- Ensure dependencies are installed (SDL2, GTK3, OpenGL, build tools). On Debian/Ubuntu:
  - `sudo apt install build-essential cmake pkg-config libsdl2-dev libgtk-3-dev mesa-utils libgl1-mesa-dev`
- If linking errors for math functions occur, ensure `-lm` is linked (CMake handles this for UNIX).

Editor viewport not visible / GL errors
- If you see “No available configurations for the given RGBA pixel format”, your X server or GLX stack can’t provide a GL context.
  - The editor automatically falls back to a software (Cairo) viewport so you can still work.
  - To force software fallback: `GDK_GL=disable ./build/oz_editor`
  - On WSL2+VcXsrv: start VcXsrv with Native OpenGL unchecked; set `DISPLAY` and `LIBGL_ALWAYS_INDIRECT=1` as per setup script.
  - Verify GLX in WSL: `glxinfo -B`. If it fails, GLArea won’t work; use the fallback or run natively on Linux.

SDL2/OpenGL demo issues
- If the demo window is black or stretched, ensure your X server is running and `DISPLAY` is set.
- Tearing or poor performance on WSL2 is common; try disabling vsync via the window config (vsync=false) or prefer native Linux.

GTK runtime warnings
- Transient/parent warnings can occur if dialogs are created without a parent; the editor uses the main window as parent already.
- If you still see GTK warnings, they are typically benign under WSL2.

General WSL2 tips
- Prefer latest Windows + WSL2, keep your distro updated.
- For UI apps, an X server is required (VcXsrv, X410, or GWSL). VcXsrv settings: disable Native OpenGL.
- Check network/firewall rules if the X server doesn’t accept connections (Disable access control in VcXsrv when testing).

License
MIT