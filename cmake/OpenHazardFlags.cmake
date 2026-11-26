# OpenHazardFlags.cmake — Portable 64-bit compiler hardening & tuning
#
# Targets 64-bit Linux, macOS, and Windows across x86_64 and ARM64 while keeping
# ARM64 Linux binaries strictly ARMv8.0-A compatible (Raspberry Pi 3/4/5, RK3588,
# Jetson, Snapdragon, Ampere) and 4 KB / 16 KB / 64 KB page-size safe.

option(EF_ARM64_PORTABLE "Target universal ARMv8.0-A baseline on Linux ARM64 (Pi 4/5 & all SBCs)" ON)
option(EF_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(EF_LTO "Enable Link-Time Optimization (IPO) for Release builds" ON)

if(EF_LTO AND NOT EF_SANITIZERS)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT EF_IPO_SUPPORTED OUTPUT EF_IPO_ERROR)
  if(NOT EF_IPO_SUPPORTED)
    message(STATUS "OpenHazard LTO disabled (not supported by toolchain): ${EF_IPO_ERROR}")
  endif()
else()
  set(EF_IPO_SUPPORTED FALSE)
endif()

function(ef_apply_common_flags target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      /MP
      /D_CRT_SECURE_NO_WARNINGS
      /D_CRT_NONSTDC_NO_DEPRECATE
      $<$<CONFIG:Release>:/O2>
    )
  else()
    # CRITICAL on ARM64: plain `char` is unsigned by default on aarch64, whereas
    # idTech3 / Raven x86 code relies on signed `char` semantics.
    target_compile_options(${target} PRIVATE
      -fsigned-char
      -fno-strict-aliasing
      -ffile-prefix-map=${CMAKE_SOURCE_DIR}=/src
      -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=/src
    )

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        if(EF_ARM64_PORTABLE)
          # ARMv8.0-A baseline (runs on Pi 3/4 Cortex-A72, Pi 5 Cortex-A76, RK3588, etc.)
          target_compile_options(${target} PRIVATE -march=armv8-a -mtune=cortex-a76)
        endif()
        # Safe PT_LOAD alignment across 4 KB (Pi 4), 16 KB (Pi 5), and 64 KB kernels
        target_link_options(${target} PRIVATE -Wl,-z,max-page-size=65536)
      elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        target_compile_options(${target} PRIVATE -march=x86-64)
      endif()
    endif()

    if(EF_SANITIZERS)
      target_compile_options(${target} PRIVATE
        -O1 -g -fno-omit-frame-pointer
        -fsanitize=address,undefined
      )
      target_link_options(${target} PRIVATE
        -fsanitize=address,undefined
      )
    else()
      target_compile_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-O2>)
    endif()
  endif()
endfunction()

function(ef_apply_lto target)
  if(EF_IPO_SUPPORTED AND NOT EF_SANITIZERS)
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  endif()
endfunction()
