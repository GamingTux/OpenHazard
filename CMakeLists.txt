cmake_minimum_required(VERSION 3.16...3.31)

project(OpenHazard
  VERSION 1.0.0
  DESCRIPTION "OpenHazard: Portable 64-bit Vulkan port of Star Trek: Voyager - Elite Force"
  LANGUAGES C CXX
)

if(APPLE)
  enable_language(OBJC)
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "OpenHazard targets 64-bit platforms only (x86_64 / ARM64).")
endif()

# Default to Release if invoked without -DCMAKE_BUILD_TYPE
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

option(BUILD_SP "Build OpenHazard Singleplayer client & modules" ON)
option(BUILD_MP "Build OpenHazard Holomatch multiplayer client & dedicated server (shared Vulkan engine)" ON)
option(EF_STATIC_RENDERER "Statically link Vulkan renderer into clients instead of loadable module" OFF)

set(EF_ENABLE_BINK "AUTO" CACHE STRING "Bink (.bik) cutscene decoding via FFmpeg: AUTO | ON | OFF")
set_property(CACHE EF_ENABLE_BINK PROPERTY STRINGS AUTO ON OFF)

# Unified output directory: all binaries and shared modules land together.
set(EF_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin" CACHE PATH "Unified output directory for binaries & modules")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(OpenHazardFlags)

# Normalized OS & 64-bit arch tags for portable release packages
if(WIN32)
  set(OH_PKG_OS "windows")
elseif(APPLE)
  set(OH_PKG_OS "macos")
else()
  set(OH_PKG_OS "linux")
endif()

if(CMAKE_OSX_ARCHITECTURES)
  list(GET CMAKE_OSX_ARCHITECTURES 0 OH_RAW_ARCH)
elseif(MSVC AND CMAKE_GENERATOR_PLATFORM)
  set(OH_RAW_ARCH "${CMAKE_GENERATOR_PLATFORM}")
else()
  set(OH_RAW_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(OH_RAW_ARCH MATCHES "^(aarch64|arm64|ARM64)$")
  set(OH_PKG_ARCH "arm64")
elseif(OH_RAW_ARCH MATCHES "^(x86_64|amd64|AMD64|x64)$")
  set(OH_PKG_ARCH "x86_64")
else()
  set(OH_PKG_ARCH "${OH_RAW_ARCH}")
endif()

message(STATUS "OpenHazard ${PROJECT_VERSION} | target=${OH_PKG_OS}-${OH_PKG_ARCH} | type=${CMAKE_BUILD_TYPE} | SP=${BUILD_SP} MP=${BUILD_MP} | output=${EF_OUTPUT_DIR}")

if(BUILD_SP)
  add_subdirectory(game-sp)
endif()

if(BUILD_SP OR BUILD_MP)
  add_subdirectory(engine)
endif()

# ---- Self-contained flat portable install layout (no system .deb needed) ----
if(BUILD_SP)
  install(TARGETS openhazard-sp
    RUNTIME DESTINATION "."
  )
  install(TARGETS efgame efui
    RUNTIME DESTINATION "."
    LIBRARY DESTINATION "."
  )
endif()

if(BUILD_MP)
  install(TARGETS openhazard-mp openhazard-ded
    RUNTIME DESTINATION "."
  )
endif()

if((BUILD_SP OR BUILD_MP) AND NOT EF_STATIC_RENDERER)
  install(TARGETS renderer_vulkan
    RUNTIME DESTINATION "."
    LIBRARY DESTINATION "."
  )
endif()

if(UNIX)
  if(BUILD_SP)
    install(PROGRAMS packaging/openhazard.sh DESTINATION ".")
  endif()
  if(BUILD_MP)
    install(PROGRAMS packaging/openhazard-mp.sh DESTINATION ".")
    install(PROGRAMS packaging/openhazard-ded.sh DESTINATION ".")
  endif()
  install(PROGRAMS packaging/openhazard-import-data.sh DESTINATION ".")
endif()

install(FILES
  LICENSE
  CREDITS.md
  README.md
  DESTINATION "."
)

# ---- CPack: portable self-contained archives (.tar.gz on Linux/macOS, .zip on Windows) ----
set(CPACK_PACKAGE_NAME "OpenHazard")
set(CPACK_PACKAGE_VENDOR "OpenHazard Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OpenHazard: Portable 64-bit Vulkan port of Star Trek: Voyager - Elite Force")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "OpenHazard-${PROJECT_VERSION}-${OH_PKG_OS}-${OH_PKG_ARCH}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
if(WIN32)
  set(CPACK_GENERATOR "ZIP")
else()
  set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
