# OpenHazardFlags.cmake — Portable 64-bit compiler hardening & maximum safe optimization
#
# Targets 64-bit Linux, macOS, and Windows across x86_64 and ARM64 while keeping
# portable CI builds strictly compatible (ARMv8.0-A on Linux ARM64 with 4/16/64 KB
# page safety, x86-64-v2 on Linux/macOS x86_64, Apple M1 baseline on macOS ARM64).
# Opt-in presets EF_NATIVE_CPU and EF_PGO unlock host-specific ISA & profile-guided
# optimization for local builds.

option(EF_ARM64_PORTABLE "Target universal ARMv8.0-A baseline on Linux ARM64 (Pi 3/4/5 & all SBCs)" ON)
option(EF_NATIVE_CPU "Optimize for the build host CPU (-mcpu=native / -march=native); disables portable ISA baseline" OFF)
option(EF_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(EF_LTO "Enable Full Link-Time Optimization (LTO/IPO) for Release builds" ON)

set(EF_PGO "OFF" CACHE STRING "Profile-Guided Optimization mode: OFF | GENERATE | USE")
set_property(CACHE EF_PGO PROPERTY STRINGS OFF GENERATE USE)
set(EF_PGO_DIR "${CMAKE_BINARY_DIR}/pgo-data" CACHE PATH "Directory for PGO profile data")
string(TOUPPER "${EF_PGO}" EF_PGO_UPPER)

if(EF_LTO AND NOT EF_SANITIZERS AND NOT APPLE)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT EF_IPO_SUPPORTED OUTPUT EF_IPO_ERROR)
  if(NOT EF_IPO_SUPPORTED)
    message(STATUS "OpenHazard LTO disabled (not supported by toolchain): ${EF_IPO_ERROR}")
  endif()
elseif(EF_LTO AND NOT EF_SANITIZERS AND APPLE)
  # AppleClang uses -flto=full directly in ef_apply_lto() instead of CMake's -flto=thin default.
  set(EF_IPO_SUPPORTED TRUE)
else()
  set(EF_IPO_SUPPORTED FALSE)
endif()

if(EF_PGO_UPPER STREQUAL "GENERATE" OR EF_PGO_UPPER STREQUAL "USE")
  message(STATUS "OpenHazard PGO mode: ${EF_PGO_UPPER} (dir=${EF_PGO_DIR})")
endif()

# ef_apply_common_flags(<target> [O3|O2|Os])
function(ef_apply_common_flags target)
  set(_opt_level "O3")
  if(ARGC GREATER 1)
    set(_opt_level "${ARGV1}")
  endif()

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
    if(_opt_level STREQUAL "Os")
      set(_msvc_opt /O1)
    elseif(_opt_level STREQUAL "O2")
      set(_msvc_opt /O2 /Oi)
    else()
      set(_msvc_opt /O2 /Oi /Ot)
    endif()
    # /Gy enables function-level COMDATs required for /OPT:REF & /OPT:ICF;
    # /Gw enables data COMDATs; /Zc:inline strips unreferenced inline functions;
    # /GR- disables unused C++ RTTI.
    target_compile_options(${target} PRIVATE
      $<$<COMPILE_LANGUAGE:C>:/std:c11>
      $<$<COMPILE_LANGUAGE:C,CXX>:/MP>
      $<$<COMPILE_LANGUAGE:CXX>:/GR->
      $<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<CONFIG:Release>>:${_msvc_opt} /Gy /Gw /Zc:inline /fp:contract>
    )
    if(_is_linkable)
      target_link_options(${target} PRIVATE
        $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>
      )
      if(NOT EF_SANITIZERS)
        if(EF_PGO_UPPER STREQUAL "GENERATE")
          target_link_options(${target} PRIVATE $<$<CONFIG:Release>:/GENPROFILE>)
        elseif(EF_PGO_UPPER STREQUAL "USE")
          target_link_options(${target} PRIVATE $<$<CONFIG:Release>:/USEPROFILE>)
        endif()
      endif()
    endif()
  else()
    # CRITICAL on ARM64: plain `char` is unsigned by default on aarch64, whereas
    # idTech3 / Raven x86 code relies on signed `char` semantics.
    # -fvisibility=hidden + -fno-semantic-interposition + -fno-plt (ELF) eliminate
    # internal/external PLT stubs and unlock full cross-TU inlining.
    # -fno-rtti strips unused C++ typeinfo tables (zero dynamic_cast/typeid in repo).
    target_compile_options(${target} PRIVATE
      -pipe
      -fsigned-char
      -fno-strict-aliasing
      -ffile-prefix-map=${CMAKE_SOURCE_DIR}=/src
      -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=/src
      -fvisibility=hidden
      $<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden -fno-rtti>
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      target_compile_options(${target} PRIVATE
        -fno-semantic-interposition
        -fno-plt
      )
    elseif(NOT APPLE)
      target_compile_options(${target} PRIVATE -fno-semantic-interposition)
    endif()

    # ---- Target ISA selection (portable CI baseline vs host-native opt-in) ----
    if(EF_NATIVE_CPU)
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        target_compile_options(${target} PRIVATE -mcpu=native)
      else()
        target_compile_options(${target} PRIVATE -march=native)
      endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        if(EF_ARM64_PORTABLE)
          # ARMv8.0-A baseline (runs on Pi 3/4 Cortex-A72, Pi 5 Cortex-A76, RK3588, etc.)
          target_compile_options(${target} PRIVATE -march=armv8-a -mtune=cortex-a76)
        endif()
      elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        # x86-64-v2 (SSE4.2 / SSSE3 / POPCNT — universal across all post-2009 x86_64 CPUs)
        target_compile_options(${target} PRIVATE -march=x86-64-v2)
      endif()
    elseif(APPLE)
      list(LENGTH CMAKE_OSX_ARCHITECTURES _osx_arch_len)
      if(_osx_arch_len LESS 2)
        if(_osx_arch_len EQUAL 1)
          list(GET CMAKE_OSX_ARCHITECTURES 0 _osx_arch)
        else()
          set(_osx_arch "${CMAKE_SYSTEM_PROCESSOR}")
        endif()
        if(_osx_arch MATCHES "^(aarch64|arm64|ARM64)$")
          target_compile_options(${target} PRIVATE -mcpu=apple-m1)
        elseif(_osx_arch MATCHES "^(x86_64|amd64|AMD64)$")
          # macOS 12+ requires SSE4.2 (x86-64-v2)
          target_compile_options(${target} PRIVATE -march=x86-64-v2)
        endif()
      endif()
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
       AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$"
       AND _is_linkable)
      # Safe PT_LOAD alignment across 4 KB (Pi 4), 16 KB (Pi 5), and 64 KB kernels
      target_link_options(${target} PRIVATE -Wl,-z,max-page-size=65536)
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
      # Maximum safe Release flags (NOTE: .eh_frame unwind tables are intentionally
      # preserved so SP_LogBacktrace() / _Unwind_Backtrace() work with -fomit-frame-pointer):
      #  -O3/-O2/-Os                         : per-target codegen vs L1i cache balance
      #  -fno-math-errno -fno-trapping-math  : direct 1-cycle hardware fsqrt (zero sqrt@plt)
      #  -ffp-contract=fast                  : hardware FMA (overridden to =off on physics/ded)
      #  -fomit-frame-pointer                : frees x29 (ARM64) / rbp (x86_64) in hot loops
      #  -ffunction-sections -fdata-sections : linker dead-code stripping & tight I-cache layout
      target_compile_options(${target} PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:-${_opt_level} -fno-math-errno -fno-trapping-math -ffp-contract=fast -fomit-frame-pointer -ffunction-sections -fdata-sections>
      )
      if(_is_linkable)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
          target_link_options(${target} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:-Wl,-Bsymbolic-functions -Wl,--gc-sections -Wl,--as-needed -Wl,-O1 -Wl,--hash-style=gnu -Wl,-z,relro,-z,now>
            $<$<CONFIG:Release>:-Wl,-s>
          )
        elseif(APPLE)
          target_link_options(${target} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:-Wl,-dead_strip>
            $<$<CONFIG:Release>:-Wl,-x>
          )
        endif()
      endif()

      # ---- Optional Joint Profile-Guided Optimization (EF_PGO=GENERATE|USE) ----
      if(EF_PGO_UPPER STREQUAL "GENERATE")
        if(CMAKE_C_COMPILER_ID MATCHES "Clang")
          target_compile_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-fprofile-instr-generate=${EF_PGO_DIR}/%p-%m.profraw>)
          if(_is_linkable)
            target_link_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-fprofile-instr-generate=${EF_PGO_DIR}/%p-%m.profraw>)
          endif()
        else()
          target_compile_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-fprofile-generate=${EF_PGO_DIR} -fprofile-update=atomic>)
          if(_is_linkable)
            target_link_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-fprofile-generate=${EF_PGO_DIR} -fprofile-update=atomic>)
          endif()
        endif()
      elseif(EF_PGO_UPPER STREQUAL "USE")
        if(CMAKE_C_COMPILER_ID MATCHES "Clang")
          target_compile_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-fprofile-instr-use=${EF_PGO_DIR}/merged.profdata>)
          if(_is_linkable)
            target_link_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-fprofile-instr-use=${EF_PGO_DIR}/merged.profdata>)
          endif()
        else()
          target_compile_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-fprofile-use=${EF_PGO_DIR} -fprofile-correction>)
          if(_is_linkable)
            target_link_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-fprofile-use=${EF_PGO_DIR} -fprofile-correction>)
          endif()
        endif()
      endif()
    endif()
  endif()
endfunction()

function(ef_apply_lto target)
  if(EF_IPO_SUPPORTED AND NOT EF_SANITIZERS)
    get_target_property(_tgt_type ${target} TYPE)
    set(_is_linkable TRUE)
    if(_tgt_type STREQUAL "OBJECT_LIBRARY" OR _tgt_type STREQUAL "STATIC_LIBRARY")
      set(_is_linkable FALSE)
    endif()

    if(APPLE)
      # Force Full-LTO (-flto=full) on AppleClang instead of CMake's ThinLTO default.
      target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:-flto=full>)
      if(_is_linkable)
        target_link_options(${target} PRIVATE $<$<CONFIG:Release>:-flto=full>)
      endif()
    else()
      set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
      if(_is_linkable AND (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        # Optimize the entire binary/shared-object as a single LTO partition for
        # maximum cross-file inlining and interprocedural constant propagation.
        target_link_options(${target} PRIVATE $<$<CONFIG:Release>:-flto-partition=one>)
      endif()
    endif()
  endif()
endfunction()
