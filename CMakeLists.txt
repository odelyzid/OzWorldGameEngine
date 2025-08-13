cmake_minimum_required(VERSION 3.16)

project(OzWorld VERSION 0.1.0 LANGUAGES C)

# Options
option(OZ_BUILD_DEMO "Build the SDL2/OpenGL demo" ON)
option(OZ_BUILD_EDITOR "Build the GTK3 editor" ON)
option(OZ_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(OZ_WITH_AUDIO "Enable SDL_mixer-based audio playback" ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Warnings
if(MSVC)
  add_compile_options(/W4)
  if(OZ_WARNINGS_AS_ERRORS)
    add_compile_options(/WX)
  endif()
else()
  add_compile_options(-Wall -Wextra -Wshadow -Wconversion -Wstrict-prototypes -Wpedantic)
  if(OZ_WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
  endif()
endif()

# Core library (no external deps)
add_library(ozcore
  src/core/oz_log.c
  src/core/oz_core.c
  src/core/oz_bsp.c
  src/core/oz_camera.c
  src/core/oz_debug.c
  src/core/oz_assets.c
  src/core/oz_tex.c
  src/core/oz_bundle.c
  src/core/oz_audio_sdlmixer.c
  src/render/oz_render_common.c
)

target_include_directories(ozcore PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
if(UNIX)
  target_link_libraries(ozcore PUBLIC m)
endif()
if(OZ_HAVE_SDL_MIXER)
  target_compile_definitions(ozcore PUBLIC OZ_HAVE_SDL_MIXER=1)
  if(TARGET SDL2_mixer::SDL2_mixer)
    target_link_libraries(ozcore PUBLIC SDL2_mixer::SDL2_mixer)
  elseif(TARGET PkgConfig::SDL2_MIXER)
    target_link_libraries(ozcore PUBLIC PkgConfig::SDL2_MIXER)
  endif()
endif()

# Dependencies discovery helpers
include(CheckIncludeFile)
find_package(PkgConfig QUIET)

# Cross target detection
set(OZ_CROSS_WINDOWS OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(OZ_CROSS_WINDOWS ON)
endif()

# Try to find SDL2 (either via CMake config or pkg-config)
set(OZ_HAVE_SDL2 OFF)
set(OZ_HAVE_OPENGL OFF)
set(OZ_HAVE_GTK3 OFF)
set(OZ_HAVE_SDL_MIXER OFF)

# OpenGL
find_package(OpenGL QUIET)
if(OpenGL_FOUND)
  set(OZ_HAVE_OPENGL ON)
endif()

# SDL2 (prefer pkg-config when cross-compiling to Windows)
if(OZ_CROSS_WINDOWS AND PkgConfig_FOUND)
  pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
  if(TARGET PkgConfig::SDL2)
    set(OZ_HAVE_SDL2 ON)
  endif()
endif()
if(NOT OZ_HAVE_SDL2)
  find_package(SDL2 QUIET CONFIG)
  if(TARGET SDL2::SDL2)
    set(OZ_HAVE_SDL2 ON)
  elseif(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    if(TARGET PkgConfig::SDL2)
      set(OZ_HAVE_SDL2 ON)
    endif()
  endif()
endif()

# SDL2_mixer (audio) — prefer pkg-config when cross-compiling to Windows
if(OZ_WITH_AUDIO)
  if(OZ_CROSS_WINDOWS AND PkgConfig_FOUND)
    pkg_check_modules(SDL2_MIXER QUIET IMPORTED_TARGET SDL2_mixer)
    if(TARGET PkgConfig::SDL2_MIXER)
      set(OZ_HAVE_SDL_MIXER ON)
    endif()
  endif()
  if(NOT OZ_HAVE_SDL_MIXER)
    find_package(SDL2_mixer QUIET CONFIG)
    if(TARGET SDL2_mixer::SDL2_mixer)
      set(OZ_HAVE_SDL_MIXER ON)
    elseif(PkgConfig_FOUND)
      pkg_check_modules(SDL2_MIXER QUIET IMPORTED_TARGET SDL2_mixer)
      if(TARGET PkgConfig::SDL2_MIXER)
        set(OZ_HAVE_SDL_MIXER ON)
      endif()
    endif()
  endif()
endif()

# GTK3
if(PkgConfig_FOUND)
  pkg_check_modules(GTK3 QUIET IMPORTED_TARGET gtk+-3.0)
  if(TARGET PkgConfig::GTK3)
    set(OZ_HAVE_GTK3 ON)
  endif()
endif()

# Platform layer (SDL2). Build both GL and SW backends; choose at link time
if(OZ_BUILD_DEMO AND OZ_HAVE_SDL2)
  add_library(ozplatform_sdl_gl
    src/platform/sdl/oz_platform_sdl.c
  )
  target_include_directories(ozplatform_sdl_gl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_definitions(ozplatform_sdl_gl PUBLIC OZ_HAVE_SDL2=1)
  if(TARGET SDL2::SDL2)
    target_link_libraries(ozplatform_sdl_gl PUBLIC SDL2::SDL2)
  elseif(TARGET PkgConfig::SDL2)
    target_link_libraries(ozplatform_sdl_gl PUBLIC PkgConfig::SDL2)
  endif()
  if(OpenGL_FOUND)
    target_link_libraries(ozplatform_sdl_gl PUBLIC OpenGL::GL)
    set_target_properties(ozplatform_sdl_gl PROPERTIES OUTPUT_NAME ozplatform_sdl)
  endif()

  add_library(ozplatform_sdl_sw
    src/platform/sdl/oz_platform_sdl_sw.c
  )
  target_include_directories(ozplatform_sdl_sw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_definitions(ozplatform_sdl_sw PUBLIC OZ_HAVE_SDL2=1)
  if(TARGET SDL2::SDL2)
    target_link_libraries(ozplatform_sdl_sw PUBLIC SDL2::SDL2)
  elseif(TARGET PkgConfig::SDL2)
    target_link_libraries(ozplatform_sdl_sw PUBLIC PkgConfig::SDL2)
  endif()
  target_link_libraries(ozplatform_sdl_sw PUBLIC ozcore)
endif()

# Demo app
if(OZ_BUILD_DEMO AND OZ_HAVE_SDL2)
  add_executable(oz_demo
    apps/oz_demo/main.c
    src/render/oz_render_gl.c
    src/render/oz_render_soft.c
  )
  if(OZ_HAVE_OPENGL)
    target_link_libraries(oz_demo PRIVATE ozcore ozplatform_sdl_gl)
  else()
    target_link_libraries(oz_demo PRIVATE ozcore ozplatform_sdl_sw)
  endif()
  # Link Cairo for software rendering fallback
  if(PkgConfig_FOUND)
    pkg_check_modules(CAIRO QUIET IMPORTED_TARGET cairo)
    if(TARGET PkgConfig::CAIRO)
      target_link_libraries(oz_demo PRIVATE PkgConfig::CAIRO)
    endif()
  endif()
  # Link math library for trig functions used by oz_demo
  if(UNIX)
    target_link_libraries(oz_demo PRIVATE m)
  endif()
  target_include_directories(oz_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
else()
  message(STATUS "oz_demo disabled (SDL2/OpenGL not found or OZ_BUILD_DEMO=OFF)")
endif()

# Software-only demo that never links OpenGL backend (safe for remote X)
if(OZ_BUILD_DEMO AND OZ_HAVE_SDL2)
  add_executable(oz_demo_sw
    apps/oz_demo/main_sw.c
    src/render/oz_render_soft.c
  )
  target_link_libraries(oz_demo_sw PRIVATE ozcore ozplatform_sdl_sw)
  if(PkgConfig_FOUND)
    if(TARGET PkgConfig::CAIRO)
      target_link_libraries(oz_demo_sw PRIVATE PkgConfig::CAIRO)
    else()
      pkg_check_modules(CAIRO QUIET IMPORTED_TARGET cairo)
      if(TARGET PkgConfig::CAIRO)
        target_link_libraries(oz_demo_sw PRIVATE PkgConfig::CAIRO)
      endif()
    endif()
  endif()
  if(UNIX)
    target_link_libraries(oz_demo_sw PRIVATE m)
  endif()
  target_include_directories(oz_demo_sw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

# Editor (GTK3)
if(OZ_BUILD_EDITOR AND OZ_HAVE_GTK3)
  add_executable(oz_editor
    src/editor/app.c
    src/editor/input.c
    src/editor/ui/ui.c
    editor/main.c
    src/render/oz_render_soft.c
    src/render/oz_render_gl.c
  )
  if(TARGET PkgConfig::GTK3)
    target_link_libraries(oz_editor PRIVATE PkgConfig::GTK3)
  endif()
  # Allow including editor headers like "editor.h"
  target_include_directories(oz_editor PRIVATE ${CMAKE_SOURCE_DIR}/editor)
  target_link_libraries(oz_editor PRIVATE ozcore)
  if(OpenGL_FOUND)
    target_link_libraries(oz_editor PRIVATE OpenGL::GL)
  endif()
  if(UNIX)
    target_link_libraries(oz_editor PRIVATE m)
  endif()
  target_include_directories(oz_editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
else()
  message(STATUS "oz_editor disabled (GTK3 not found or OZ_BUILD_EDITOR=OFF)")
endif()

# Server (UNIX only, simple HTTP)
option(OZ_BUILD_SERVER "Build the simple HTTP server" ON)
if(OZ_BUILD_SERVER AND UNIX)
  add_executable(oz_server apps/oz_server/main.c)
  target_link_libraries(oz_server PRIVATE ozcore)
  target_include_directories(oz_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
else()
  message(STATUS "oz_server disabled (non-UNIX or OZ_BUILD_SERVER=OFF)")
endif()

# Summary output
message(STATUS "OzWorld configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  SDL2: ${OZ_HAVE_SDL2}")
message(STATUS "  OpenGL: ${OZ_HAVE_OPENGL}")
message(STATUS "  GTK3: ${OZ_HAVE_GTK3}")
message(STATUS "  Demo: ${OZ_BUILD_DEMO}")
message(STATUS "  Editor: ${OZ_BUILD_EDITOR}")
