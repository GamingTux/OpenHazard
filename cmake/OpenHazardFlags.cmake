# OpenHazardFlags.cmake — Portable 64-bit compiler hardening & maximum safe optimization
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
  get_target_property(_tgt_type ${target} TYPE)

  set(_is_linkable TRUE)
  if(_tgt_type STREQUAL "OBJECT_LIBRARY" OR _tgt_type STREQUAL "STATIC_LIBRARY")
    set(_is_linkable FALSE)
  endif()

  if(MSVC)
    target_compile_definitions(${target} PRIVATE
      _CRT_SECURE_NO_WARNINGS
      _CRT_NONSTDC_NO_DEPRECATE
    )
    target_compile_options(${target} PRIVATE
      $<$<COMPILE_LANGUAGE:C>:/std:c11>
      $<$<COMPILE_LANGUAGE:C,CXX>:/MP>
      $<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<CONFIG:Release>>:/O2 /Oi /Ot /Gw /fp:contract>
    )
    if(_is_linkable)
      target_link_options(${target} PRIVATE
        $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>
      )
    endif()
  else()
    # CRITICAL on ARM64: plain `char` is unsigned by default on aarch64, whereas
    # idTech3 / Raven x86 code relies on signed `char` semantics.
    # -fvisibility=hidden + -fno-semantic-interposition (ELF) eliminate internal
    # PLT stubs and unlock full cross-TU inlining inside shared libraries.
    target_compile_options(${target} PRIVATE
      -fsigned-char
      -fno-strict-aliasing
      -ffile-prefix-map=${CMAKE_SOURCE_DIR}=/src
      -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=/src
      -fvisibility=hidden
      $<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>
    )
    if(NOT APPLE)
      target_compile_options(${target} PRIVATE -fno-semantic-interposition)
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        if(EF_ARM64_PORTABLE)
          # ARMv8.0-A baseline (runs on Pi 3/4 Cortex-A72, Pi 5 Cortex-A76, RK3588, etc.)
          target_compile_options(${target} PRIVATE -march=armv8-a -mtune=cortex-a76)
        endif()
        if(_is_linkable)
          # Safe PT_LOAD alignment across 4 KB (Pi 4), 16 KB (Pi 5), and 64 KB kernels
          target_link_options(${target} PRIVATE -Wl,-z,max-page-size=65536)
        endif()
      elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        target_compile_options(${target} PRIVATE -march=x86-64)
      endif()
    endif()

    if(EF_SANITIZERS)
      target_compile_options(${target} PRIVATE
        -O1 -g -fno-omit-frame-pointer
        -fsanitize=address,undefined
      )
      if(_is_linkable)
        target_link_options(${target} PRIVATE
          -fsanitize=address,undefined
        )
      endif()
    else()
      # Maximum safe Release flags:
      #  -O3                                  : full auto-vectorization & loop opts
      #  -fno-math-errno -fno-trapping-math   : direct 1-cycle hardware fsqrt (no sqrtf@plt)
      #  -ffp-contract=fast                   : hardware FMA (fmadd), IEEE/NaN-safe for BSP/physics
      #  -fomit-frame-pointer                 : frees x29 (ARM64) / rbp (x86_64) in hot loops
      #  -ffunction-sections -fdata-sections  : enables linker dead-code stripping & tight I-cache layout
      target_compile_options(${target} PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:-O3 -fno-math-errno -fno-trapping-math -ffp-contract=fast -fomit-frame-pointer -ffunction-sections -fdata-sections>
      )
      if(_is_linkable)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
          target_link_options(${target} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:-Wl,-Bsymbolic-functions -Wl,--gc-sections -Wl,--as-needed -Wl,-O1>
          )
        elseif(APPLE)
          target_link_options(${target} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:-Wl,-dead_strip>
          )
        endif()
      endif()
    endif()
  endif()
endfunction()

function(ef_apply_lto target)
  if(EF_IPO_SUPPORTED AND NOT EF_SANITIZERS)
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    get_target_property(_tgt_type ${target} TYPE)
    if(NOT _tgt_type STREQUAL "OBJECT_LIBRARY" AND NOT _tgt_type STREQUAL "STATIC_LIBRARY"
       AND (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
      # Optimize the entire binary/shared-object as a single LTO partition for
      # maximum cross-file inlining and interprocedural constant propagation.
      target_link_options(${target} PRIVATE $<$<CONFIG:Release>:-flto-partition=one>)
    endif()
  endif()
endfunction()
