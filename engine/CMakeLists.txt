# engine/CMakeLists.txt — Portable 64-bit Vulkan Engine (openhazard-sp, openhazard-mp, openhazard-ded, renderer_vulkan)

find_package(SDL2 2.0.22 REQUIRED)
find_package(PkgConfig QUIET)
find_package(CURL QUIET)

set(EF  "${CMAKE_CURRENT_SOURCE_DIR}/efcode")
set(SP  "${CMAKE_CURRENT_SOURCE_DIR}/sp")
set(EFG "${CMAKE_SOURCE_DIR}/game-sp")

# ---- Architecture detection (64-bit ARM64 / x86_64) ----
if(OH_PKG_ARCH STREQUAL "arm64")
  if(WIN32 OR APPLE)
    set(EF_ARCH_STRING "arm64")
  else()
    set(EF_ARCH_STRING "aarch64")
  endif()
  set(EF_IS_ARM64 TRUE)
  set(EF_IS_X86_64 FALSE)
elseif(OH_PKG_ARCH STREQUAL "x86_64")
  set(EF_ARCH_STRING "x86_64")
  set(EF_IS_ARM64 FALSE)
  set(EF_IS_X86_64 TRUE)
else()
  set(EF_ARCH_STRING "${OH_PKG_ARCH}")
  set(EF_IS_ARM64 FALSE)
  set(EF_IS_X86_64 FALSE)
endif()

# ---- Optional / auto-detected FFmpeg Bink (.bik) decoder (Singleplayer only) ----
set(EF_BINK_FFMPEG_PREFIX "${CMAKE_SOURCE_DIR}/third_party/ffmpeg-bink" CACHE PATH
    "Prefix of self-contained minimal static Bink-only FFmpeg (built via cmake/build-bink-ffmpeg.sh)")

set(EF_BINK_MODE "NONE")
if(NOT EF_ENABLE_BINK STREQUAL "OFF")
  if(EXISTS "${EF_BINK_FFMPEG_PREFIX}/lib/libavformat.a" AND
     EXISTS "${EF_BINK_FFMPEG_PREFIX}/lib/libavcodec.a" AND
     EXISTS "${EF_BINK_FFMPEG_PREFIX}/lib/libswscale.a" AND
     EXISTS "${EF_BINK_FFMPEG_PREFIX}/lib/libavutil.a" AND
     EXISTS "${EF_BINK_FFMPEG_PREFIX}/include/libavformat/avformat.h")
    set(EF_BINK_MODE "STATIC")
  elseif(PkgConfig_FOUND)
    pkg_check_modules(AVCODEC QUIET IMPORTED_TARGET
      libavformat libavcodec libswscale libavutil)
    if(AVCODEC_FOUND)
      set(EF_BINK_MODE "SYSTEM")
    endif()
  endif()

  if(EF_BINK_MODE STREQUAL "NONE" AND EF_ENABLE_BINK STREQUAL "ON")
    message(FATAL_ERROR
      "EF_ENABLE_BINK=ON requested, but neither static Bink FFmpeg (${EF_BINK_FFMPEG_PREFIX}) "
      "nor system FFmpeg dev headers were found.\n"
      "Run ./cmake/build-bink-ffmpeg.sh (recommended, self-contained) or install libav*-dev.")
  endif()
endif()

if(EF_BINK_MODE STREQUAL "STATIC")
  message(STATUS "Bink (.bik) video playback (SP): ENABLED (statically linked minimal FFmpeg from ${EF_BINK_FFMPEG_PREFIX})")
  set(SP_BINK_SRC "${SP}/bink_ff.c")
elseif(EF_BINK_MODE STREQUAL "SYSTEM")
  message(STATUS "Bink (.bik) video playback (SP): ENABLED (system FFmpeg)")
  set(SP_BINK_SRC "${SP}/bink_ff.c")
else()
  message(STATUS "Bink (.bik) video playback (SP): stub (build static FFmpeg via cmake/build-bink-ffmpeg.sh to enable)")
  set(SP_BINK_SRC "${SP}/bink_stub.c")
endif()

# ---- Shared PIC object libraries (compiled once, reused across SP / MP / DED / renderer) ----
file(GLOB ZLIB_SRC "${EF}/zlib/*.c")
add_library(ef_zlib_obj OBJECT ${ZLIB_SRC})
set_target_properties(ef_zlib_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ef_zlib_obj PRIVATE "${EF}/zlib")
ef_apply_common_flags(ef_zlib_obj)
ef_apply_lto(ef_zlib_obj)

file(GLOB JPEG_SRC "${EF}/jpeg-8c/*.c")
add_library(ef_jpeg_obj OBJECT ${JPEG_SRC})
set_target_properties(ef_jpeg_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ef_jpeg_obj PRIVATE "${EF}/jpeg-8c")
ef_apply_common_flags(ef_jpeg_obj)
ef_apply_lto(ef_jpeg_obj)

set(LIBMAD_SRC
  ${EF}/libmad-0.15.1b/bit.c     ${EF}/libmad-0.15.1b/decoder.c
  ${EF}/libmad-0.15.1b/fixed.c   ${EF}/libmad-0.15.1b/frame.c
  ${EF}/libmad-0.15.1b/huffman.c ${EF}/libmad-0.15.1b/layer12.c
  ${EF}/libmad-0.15.1b/layer3.c  ${EF}/libmad-0.15.1b/stream.c
  ${EF}/libmad-0.15.1b/synth.c   ${EF}/libmad-0.15.1b/timer.c
  ${EF}/libmad-0.15.1b/version.c
)
add_library(ef_libmad_obj OBJECT ${LIBMAD_SRC})
set_target_properties(ef_libmad_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ef_libmad_obj PRIVATE
  "${EF}/libmad-0.15.1b"
  "${EF}/libmad-0.15.1b/include"
)
target_compile_definitions(ef_libmad_obj PRIVATE
  FPM_64BIT
  ARCH_STRING="${EF_ARCH_STRING}"
)
ef_apply_common_flags(ef_libmad_obj)
ef_apply_lto(ef_libmad_obj)

file(GLOB BOTLIB_SRC "${EF}/botlib/*.c")
add_library(ef_botlib_obj OBJECT ${BOTLIB_SRC})
set_target_properties(ef_botlib_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ef_botlib_obj PRIVATE "${EF}" "${EF}/zlib")
target_compile_definitions(ef_botlib_obj PRIVATE
  ELITEFORCE
  BOTLIB
  USE_LOCAL_HEADERS=1
  ARCH_STRING="${EF_ARCH_STRING}"
)
ef_apply_common_flags(ef_botlib_obj)
ef_apply_lto(ef_botlib_obj)

# ---- Engine core sources ----
set(CLIENT_SRC
  ${EF}/client/cl_cgame.c    ${EF}/client/cl_cin.c      ${EF}/client/cl_console.c
  ${EF}/client/cl_input.c    ${EF}/client/cl_keys.c     ${EF}/client/cl_main.c
  ${EF}/client/cl_net_chan.c ${EF}/client/cl_parse.c    ${EF}/client/cl_scrn.c
  ${EF}/client/cl_ui.c       ${EF}/client/cl_avi.c
  ${EF}/client/snd_adpcm.c   ${EF}/client/snd_dma.c     ${EF}/client/snd_mem.c
  ${EF}/client/snd_mix.c     ${EF}/client/snd_wavelet.c
  ${EF}/client/snd_main.c    ${EF}/client/snd_codec.c   ${EF}/client/snd_codec_wav.c
  ${EF}/client/snd_codec_mp3.c
  ${EF}/client/qal.c         ${EF}/client/snd_openal.c
)

set(QCOMMON_SRC
  ${EF}/qcommon/cm_load.c   ${EF}/qcommon/cm_patch.c  ${EF}/qcommon/cm_polylib.c
  ${EF}/qcommon/cm_test.c   ${EF}/qcommon/cm_trace.c
  ${EF}/qcommon/cmd.c       ${EF}/qcommon/common.c    ${EF}/qcommon/cvar.c
  ${EF}/qcommon/files.c     ${EF}/qcommon/md4.c       ${EF}/qcommon/md5.c
  ${EF}/qcommon/msg.c       ${EF}/qcommon/net_chan.c  ${EF}/qcommon/net_ip.c
  ${EF}/qcommon/huffman.c   ${EF}/qcommon/q_math.c    ${EF}/qcommon/q_shared.c
  ${EF}/qcommon/unzip.c     ${EF}/qcommon/ioapi.c     ${EF}/qcommon/puff.c
  ${EF}/qcommon/vm.c        ${EF}/qcommon/vm_interpreted.c
)

# ---- QVM JIT selection ----
# ARM64 (Linux / macOS / Windows): 16-KB-page-hardened vm_aarch64.c
# x86_64 (Linux / macOS / MinGW): vm_x86.c
# Windows MSVC x64: vm_x86.c + MASM vm_x86_64.asm (or interpreter fallback if MASM unavailable)
set(EF_VM_JIT FALSE)
if(EF_IS_ARM64)
  list(APPEND QCOMMON_SRC ${EF}/qcommon/vm_aarch64.c)
  set(EF_VM_JIT TRUE)
elseif(EF_IS_X86_64 AND NOT MSVC)
  list(APPEND QCOMMON_SRC ${EF}/qcommon/vm_x86.c)
  set(EF_VM_JIT TRUE)
elseif(EF_IS_X86_64 AND MSVC)
  include(CheckLanguage)
  check_language(ASM_MASM)
  if(CMAKE_ASM_MASM_COMPILER)
    enable_language(ASM_MASM)
    list(APPEND QCOMMON_SRC ${EF}/qcommon/vm_x86.c ${EF}/asm/vm_x86_64.asm)
    set(EF_VM_JIT TRUE)
  endif()
endif()
message(STATUS "QVM JIT compiler: ${EF_VM_JIT} (${OH_PKG_OS}-${OH_PKG_ARCH})")

set(SERVER_SRC
  ${EF}/server/sv_bot.c      ${EF}/server/sv_ccmds.c    ${EF}/server/sv_client.c
  ${EF}/server/sv_game.c     ${EF}/server/sv_init.c     ${EF}/server/sv_main.c
  ${EF}/server/sv_net_chan.c ${EF}/server/sv_snapshot.c ${EF}/server/sv_world.c
)

set(SDL_SRC
  ${EF}/sdl/sdl_input.c ${EF}/sdl/sdl_snd.c ${EF}/sdl/sdl_gamma.c ${EF}/sdl/sdl_glimp.c
)

# ---- OS-specific system backend ----
set(SYS_SRC
  ${EF}/sys/con_log.c ${EF}/sys/sys_autoupdater.c ${EF}/sys/sys_main.c
)
if(WIN32)
  list(APPEND SYS_SRC ${EF}/sys/con_win32.c ${EF}/sys/sys_win32.c)
elseif(APPLE)
  list(APPEND SYS_SRC ${EF}/sys/con_passive.c ${EF}/sys/sys_unix.c ${EF}/sys/sys_osx.m)
else()
  list(APPEND SYS_SRC ${EF}/sys/con_passive.c ${EF}/sys/sys_unix.c)
endif()

if(EF_IS_X86_64 AND NOT MSVC)
  list(APPEND SYS_SRC ${EF}/asm/ftola.c ${EF}/asm/snapvector.c)
elseif(EF_IS_X86_64 AND MSVC AND CMAKE_ASM_MASM_COMPILER)
  set_source_files_properties(
    ${EF}/asm/ftola.asm
    ${EF}/asm/snapvector.asm
    PROPERTIES COMPILE_DEFINITIONS "idx64"
  )
  list(APPEND SYS_SRC ${EF}/asm/ftola.asm ${EF}/asm/snapvector.asm)
endif()

file(GLOB RENDERCOMMON_SRC "${EF}/renderercommon/*.c")
file(GLOB RENDERER_VK_SRC "${EF}/renderervk/*.c")
list(REMOVE_ITEM RENDERER_VK_SRC
  "${EF}/renderervk/tr_subs.c"
  "${EF}/renderervk/shaders/spirv/shader_data.c"
)

set(RENDERER_LIB_SRC
  ${EF}/qcommon/q_shared.c
  ${EF}/qcommon/q_math.c
)
if(EF_IS_X86_64 AND NOT MSVC)
  list(APPEND RENDERER_LIB_SRC ${EF}/asm/ftola.c ${EF}/asm/snapvector.c)
elseif(EF_IS_X86_64 AND MSVC AND CMAKE_ASM_MASM_COMPILER)
  list(APPEND RENDERER_LIB_SRC ${EF}/asm/ftola.asm ${EF}/asm/snapvector.asm)
endif()

# ---- Platform libraries & RPATH ----
if(WIN32)
  set(EF_SYS_LIBS ws2_32 winmm psapi advapi32 shell32)
  set(EF_SDL_LIBS SDL2::SDL2)
  if(TARGET SDL2::SDL2main)
    list(APPEND EF_SDL_LIBS SDL2::SDL2main)
  endif()
  set(EF_RPATH "")
elseif(APPLE)
  find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
  set(EF_SYS_LIBS ${COCOA_FRAMEWORK} dl m pthread)
  set(EF_SDL_LIBS SDL2::SDL2)
  set(EF_RPATH "@loader_path")
else()
  set(EF_SYS_LIBS dl m pthread)
  set(EF_SDL_LIBS SDL2::SDL2)
  set(EF_RPATH "$ORIGIN")
endif()

# ---- Singleplayer C++ bridge ----
if(BUILD_SP)
  add_library(sp_game_bridge STATIC
    ${SP}/sp_bridge.cpp
    ${SP}/sp_ui_bridge.cpp
    ${SP}/sp_clink.c
  )
  target_include_directories(sp_game_bridge PRIVATE
    "${EFG}/src/game"
    "${EFG}/src/cgame"
    "${EFG}/src/ui"
    "${EFG}/src/client"
    "${EFG}/src/renderer"
    "${EFG}/src/qcommon"
    "${SP}"
  )
  if(NOT WIN32)
    target_include_directories(sp_game_bridge BEFORE PRIVATE "${EFG}/compat")
  endif()
  if(MSVC)
    target_compile_options(sp_game_bridge PRIVATE /W0 /Zc:strictStrings-)
  else()
    target_compile_options(sp_game_bridge PRIVATE
      -include "${EFG}/compat/ef_linux_compat.h"
      -fno-threadsafe-statics
      -fcommon
      -w
    )
  endif()
  target_compile_definitions(sp_game_bridge PRIVATE NDEBUG)
  ef_apply_common_flags(sp_game_bridge)
  ef_apply_lto(sp_game_bridge)
endif()

# ---- Shared engine compile setup helper for SP & Holomatch MP clients ----
set(EF_CLIENT_COMMON_SRC
  ${CLIENT_SRC} ${QCOMMON_SRC} ${SERVER_SRC}
  ${SDL_SRC} ${SYS_SRC}
  ${SP}/data_import.c
  $<TARGET_OBJECTS:ef_botlib_obj>
  $<TARGET_OBJECTS:ef_zlib_obj>
  $<TARGET_OBJECTS:ef_jpeg_obj>
  $<TARGET_OBJECTS:ef_libmad_obj>
)

if(EF_STATIC_RENDERER)
  list(APPEND EF_CLIENT_COMMON_SRC ${RENDERER_VK_SRC} ${RENDERCOMMON_SRC})
endif()

function(ef_configure_client_target target with_bink)
  target_include_directories(${target} PRIVATE
    "${EF}"
    "${EF}/zlib"
    "${EF}/jpeg-8c"
    "${EF}/AL"
    "${EF}/libmad-0.15.1b"
    "${EF}/libmad-0.15.1b/include"
    "${SP}"
    ${SDL2_INCLUDE_DIRS}
  )
  target_compile_definitions(${target} PRIVATE
    ELITEFORCE
    BOTLIB
    USE_CODEC_MP3
    USE_INTERNAL_MP3
    FPM_64BIT
    $<$<NOT:$<BOOL:${EF_VM_JIT}>>:NO_VM_COMPILED>
    USE_VULKAN
    USE_VULKAN_API
    $<$<NOT:$<BOOL:${EF_STATIC_RENDERER}>>:USE_RENDERER_DLOPEN>
    USE_OPENAL
    USE_OPENAL_DLOPEN
    PRODUCT_VERSION="${PROJECT_VERSION}"
    ARCH_STRING="${EF_ARCH_STRING}"
    USE_LOCAL_HEADERS=1
  )
  if(NOT MSVC)
    target_compile_options(${target} PRIVATE
      -Werror=implicit-function-declaration
    )
  endif()
  ef_apply_common_flags(${target})
  ef_apply_lto(${target})
  target_link_libraries(${target} PRIVATE ${EF_SDL_LIBS} ${EF_SYS_LIBS})

  if(with_bink)
    if(EF_BINK_MODE STREQUAL "STATIC")
      target_include_directories(${target} PRIVATE "${EF_BINK_FFMPEG_PREFIX}/include")
      if(APPLE)
        target_link_libraries(${target} PRIVATE
          "${EF_BINK_FFMPEG_PREFIX}/lib/libavformat.a"
          "${EF_BINK_FFMPEG_PREFIX}/lib/libavcodec.a"
          "${EF_BINK_FFMPEG_PREFIX}/lib/libswscale.a"
          "${EF_BINK_FFMPEG_PREFIX}/lib/libavutil.a"
        )
      elseif(UNIX)
        target_link_libraries(${target} PRIVATE
          -Wl,--start-group
          "${EF_BINK_FFMPEG_PREFIX}/lib/libavformat.a"
          "${EF_BINK_FFMPEG_PREFIX}/lib/libavcodec.a"
          "${EF_BINK_FFMPEG_PREFIX}/lib/libswscale.a"
          "${EF_BINK_FFMPEG_PREFIX}/lib/libavutil.a"
          -Wl,--end-group
        )
      else()
        target_link_libraries(${target} PRIVATE
          "${EF_BINK_FFMPEG_PREFIX}/lib/libavformat.a"
          "${EF_BINK_FFMPEG_PREFIX}/lib/libavcodec.a"
          "${EF_BINK_FFMPEG_PREFIX}/lib/libswscale.a"
          "${EF_BINK_FFMPEG_PREFIX}/lib/libavutil.a"
        )
      endif()
    elseif(EF_BINK_MODE STREQUAL "SYSTEM")
      target_link_libraries(${target} PRIVATE PkgConfig::AVCODEC)
    endif()
  endif()

  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${EF_OUTPUT_DIR}"
  )
  if(EF_RPATH)
    set_target_properties(${target} PROPERTIES
      BUILD_RPATH "${EF_RPATH}"
      INSTALL_RPATH "${EF_RPATH}"
    )
  endif()
endfunction()

# ---- Singleplayer executable: openhazard-sp (with embedded Bink .bik decoder) ----
if(BUILD_SP)
  add_executable(openhazard-sp ${EF_CLIENT_COMMON_SRC} ${SP_BINK_SRC} ${SP}/sp_integration.c)
  ef_configure_client_target(openhazard-sp TRUE)
  target_link_libraries(openhazard-sp PRIVATE sp_game_bridge)
endif()

# ---- Holomatch Multiplayer executable: openhazard-mp (shared Vulkan engine, no FFmpeg bloat) ----
if(BUILD_MP)
  set(MP_EXTRA_SRC "")
  if(CURL_FOUND)
    list(APPEND MP_EXTRA_SRC ${EF}/client/cl_curl.c)
    message(STATUS "HTTP fast map download (MP): ENABLED (dlopen libcurl, zero hard runtime dependency)")
  else()
    message(STATUS "HTTP fast map download (MP): disabled (libcurl dev headers not found)")
  endif()
  add_executable(openhazard-mp ${EF_CLIENT_COMMON_SRC} ${MP_EXTRA_SRC} ${SP}/bink_stub.c ${SP}/mp_stubs.c)
  ef_configure_client_target(openhazard-mp FALSE)
  target_compile_definitions(openhazard-mp PRIVATE OPENHAZARD_MP)
  if(CURL_FOUND)
    target_include_directories(openhazard-mp PRIVATE ${CURL_INCLUDE_DIRS})
    target_compile_definitions(openhazard-mp PRIVATE USE_CURL USE_CURL_DLOPEN USE_SYSTEM_CURL_HEADERS)
  endif()

  # ---- Dedicated Server: openhazard-ded (headless, zero SDL/Vulkan dependency) ----
  add_executable(openhazard-ded
    ${QCOMMON_SRC} ${SERVER_SRC}
    ${SYS_SRC}
    $<TARGET_OBJECTS:ef_botlib_obj>
    $<TARGET_OBJECTS:ef_zlib_obj>
    ${EF}/null/null_client.c ${EF}/null/null_input.c ${EF}/null/null_snddma.c
    ${SP}/data_import.c ${SP}/mp_stubs.c
  )
  target_include_directories(openhazard-ded PRIVATE
    "${EF}" "${EF}/zlib" "${SP}"
  )
  target_compile_definitions(openhazard-ded PRIVATE
    ELITEFORCE
    OPENHAZARD_MP
    DEDICATED
    BOTLIB
    $<$<NOT:$<BOOL:${EF_VM_JIT}>>:NO_VM_COMPILED>
    PRODUCT_VERSION="${PROJECT_VERSION}"
    ARCH_STRING="${EF_ARCH_STRING}"
    USE_LOCAL_HEADERS=1
  )
  if(NOT MSVC)
    target_compile_options(openhazard-ded PRIVATE
      -Werror=implicit-function-declaration
    )
  endif()
  ef_apply_common_flags(openhazard-ded)
  ef_apply_lto(openhazard-ded)
  target_link_libraries(openhazard-ded PRIVATE ${EF_SYS_LIBS})
  set_target_properties(openhazard-ded PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${EF_OUTPUT_DIR}"
  )
endif()

# ---- Loadable Vulkan renderer module: renderer_vulkan.{so,dylib,dll} ----
if(NOT EF_STATIC_RENDERER)
  add_library(renderer_vulkan SHARED
    ${RENDERER_VK_SRC}
    ${RENDERCOMMON_SRC}
    ${RENDERER_LIB_SRC}
    ${EF}/qcommon/puff.c
    $<TARGET_OBJECTS:ef_jpeg_obj>
  )
  target_include_directories(renderer_vulkan PRIVATE
    "${EF}"
    "${EF}/zlib"
    "${EF}/jpeg-8c"
    ${SDL2_INCLUDE_DIRS}
  )
  target_compile_definitions(renderer_vulkan PRIVATE
    ELITEFORCE
    USE_VULKAN
    USE_RENDERER_DLOPEN
    USE_LOCAL_HEADERS=1
    PRODUCT_VERSION="1.40"
    ARCH_STRING="${EF_ARCH_STRING}"
  )
  if(NOT MSVC)
    target_compile_options(renderer_vulkan PRIVATE
      -Werror=implicit-function-declaration
    )
  endif()
  ef_apply_common_flags(renderer_vulkan)
  ef_apply_lto(renderer_vulkan)
  target_link_libraries(renderer_vulkan PRIVATE ${EF_SYS_LIBS})
  set_target_properties(renderer_vulkan PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${EF_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${EF_OUTPUT_DIR}"
  )
endif()
