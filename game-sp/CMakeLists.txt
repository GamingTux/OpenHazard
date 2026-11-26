# game-sp/CMakeLists.txt — Raven STEF Singleplayer SDK -> efgame & efui shared modules
#
# Exports clean extern "C" entry points (OH_GetGameAPI / OH_dllEntry / OH_vmMain
# for efgame; OH_GetUIAPI for efui) across Linux, macOS, and Windows so internal
# symbols never collide with each other or the engine.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC    "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(COMPAT "${CMAKE_CURRENT_SOURCE_DIR}/compat")

set(EF_INCLUDES
  "${COMPAT}"
  "${SRC}/game"
  "${SRC}/cgame"
  "${SRC}/ui"
  "${SRC}/icarus"
  "${SRC}/qcommon"
  "${SRC}/client"
  "${SRC}/renderer"
  "${SRC}/speedrun"
  "${SRC}/speedrun/strafe_helper"
  "${SRC}/speedrun/overbounce_prediction"
)

if(MSVC)
  set(EF_COPTS /W0 /Zc:strictStrings-)
else()
  set(EF_COPTS
    -include "${COMPAT}/ef_linux_compat.h"
    -fno-threadsafe-statics
    -fcommon
  )
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    list(APPEND EF_COPTS -w $<$<COMPILE_LANGUAGE:CXX>:-Wno-narrowing>)
  else()
    list(APPEND EF_COPTS -Wno-everything)
  endif()
endif()

file(GLOB EF_SHARED_SRC
  "${SRC}/game/q_math.cpp"
  "${SRC}/game/q_shared.cpp"
  "${SRC}/game/q_color.cpp"
)

# ===================== efgame (game + cgame + icarus) =====================
file(GLOB_RECURSE EFGAME_SRC
  "${SRC}/game/*.cpp"
  "${SRC}/cgame/*.cpp"
  "${SRC}/icarus/*.cpp"
  "${SRC}/speedrun/*.cpp"
  "${SRC}/speedrun/*.c"
  "${SRC}/speedrun/strafe_helper/*.c"
)
list(APPEND EFGAME_SRC
  "${SRC}/ui/gameinfo.cpp"
  "${COMPAT}/ef_exports.cpp"
)
list(REMOVE_ITEM EFGAME_SRC
  "${SRC}/game/bg_lib.cpp"
  "${SRC}/icarus/Interface.cpp"
  "${SRC}/icarus/Interpreter.cpp"
  "${SRC}/icarus/Tokenizer.cpp"
)
if(WIN32)
  list(APPEND EFGAME_SRC "${COMPAT}/efgame.def")
endif()

add_library(efgame SHARED ${EFGAME_SRC})
target_include_directories(efgame PRIVATE ${EF_INCLUDES})
target_compile_options(efgame PRIVATE ${EF_COPTS})
target_compile_definitions(efgame PRIVATE NDEBUG)
ef_apply_common_flags(efgame)

if(APPLE)
  target_link_options(efgame PRIVATE
    "-Wl,-exported_symbols_list,${COMPAT}/efgame.exp"
    "-Wl,-undefined,dynamic_lookup"
  )
elseif(UNIX)
  target_link_options(efgame PRIVATE
    "-Wl,--version-script=${COMPAT}/efgame.ver"
    -Wl,--unresolved-symbols=ignore-all
  )
  target_link_libraries(efgame PRIVATE m)
elseif(MSVC)
  target_link_options(efgame PRIVATE /FORCE:UNRESOLVED)
elseif(MINGW)
  target_link_options(efgame PRIVATE -Wl,--unresolved-symbols=ignore-all)
endif()

# ===================== efui (ui) =====================
file(GLOB EFUI_SRC "${SRC}/ui/*.cpp")
list(APPEND EFUI_SRC
  "${SRC}/game/g_weaponLoad.cpp"
  "${COMPAT}/ef_exports.cpp"
)
if(WIN32)
  list(APPEND EFUI_SRC "${COMPAT}/efui.def")
endif()

add_library(efui SHARED ${EFUI_SRC} ${EF_SHARED_SRC})
target_include_directories(efui PRIVATE ${EF_INCLUDES})
target_compile_options(efui PRIVATE ${EF_COPTS})
target_compile_definitions(efui PRIVATE NDEBUG _USRDLL UI_EXPORTS)
ef_apply_common_flags(efui)

if(APPLE)
  target_link_options(efui PRIVATE
    "-Wl,-exported_symbols_list,${COMPAT}/efui.exp"
    "-Wl,-undefined,dynamic_lookup"
  )
elseif(UNIX)
  target_link_options(efui PRIVATE
    "-Wl,--version-script=${COMPAT}/efui.ver"
    -Wl,--unresolved-symbols=ignore-all
  )
  target_link_libraries(efui PRIVATE m)
elseif(MSVC)
  target_link_options(efui PRIVATE /FORCE:UNRESOLVED)
elseif(MINGW)
  target_link_options(efui PRIVATE -Wl,--unresolved-symbols=ignore-all)
endif()

set_target_properties(efgame efui PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${EF_OUTPUT_DIR}"
  LIBRARY_OUTPUT_DIRECTORY "${EF_OUTPUT_DIR}"
)
if(WIN32)
  set_target_properties(efgame efui PROPERTIES PREFIX "")
endif()
