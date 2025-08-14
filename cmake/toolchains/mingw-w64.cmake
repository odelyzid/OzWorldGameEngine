# MinGW-w64 toolchain for cross-compiling OzWorld on Linux to Windows (x86_64)
# Usage:
#   cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake \
#         -DCMAKE_BUILD_TYPE=RelWithDebInfo
#
# Expectation: MinGW-w64 toolchain is installed (e.g., packages: mingw-w64, gcc-mingw-w64-x86-64).
# Optional: SDL2, SDL2_mixer, and GTK3 development packages for MinGW are available and discoverable
# via CMake config packages or pkg-config using x86_64-w64-mingw32-pkg-config.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Compilers
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
# (We are a C project; set C++ as well in case some deps require it)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Root path where the cross toolchain and target libraries live
# Adjust these if your distro uses a different sysroot layout.
set(TOOLCHAIN_ROOT /usr/x86_64-w64-mingw32 CACHE PATH "MinGW-w64 root")
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_ROOT})

# Prefer target paths for libraries/headers, but allow host programs
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Help pkg-config find Windows (MinGW) .pc files when available
# You can override on the command line: -DPKG_CONFIG_EXECUTABLE=/path/to/x86_64-w64-mingw32-pkg-config
find_program(PKG_CONFIG_EXECUTABLE NAMES x86_64-w64-mingw32-pkg-config HINTS /usr/bin /usr/local/bin)

# If your MinGW SDK installs pc files in a non-default location, uncomment and adjust:
# set(ENV{PKG_CONFIG_LIBDIR} "${TOOLCHAIN_ROOT}/lib/pkgconfig:${TOOLCHAIN_ROOT}/share/pkgconfig")
# set(ENV{PKG_CONFIG_PATH} "${TOOLCHAIN_ROOT}/lib/pkgconfig:${TOOLCHAIN_ROOT}/share/pkgconfig")

# Tell CMake to produce Windows executables (.exe)
set(CMAKE_EXECUTABLE_SUFFIX ".exe")
