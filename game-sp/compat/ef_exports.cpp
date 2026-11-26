/*
 * ef_exports.cpp — Cross-platform extern "C" entry-point wrappers for Raven's
 * STEF Singleplayer game (efgame) and UI (efui) modules.
 *
 * Avoids relying on compiler-specific C++ name mangling (Itanium vs MSVC ABI)
 * while leaving Raven's upstream .cpp files unmodified.
 */
#include "q_shared.h"
#include "tr_types.h"

#if defined(_WIN32)
#  define OH_EXPORT __declspec(dllexport)
#else
#  define OH_EXPORT __attribute__((visibility("default")))
#endif

#ifdef UI_EXPORTS

#include "ui_public.h"
uiexport_t *GetUIAPI( void );

extern "C" OH_EXPORT uiexport_t *OH_GetUIAPI( void ) {
	return GetUIAPI();
}

#else

#include "g_public.h"
game_export_t *GetGameAPI( game_import_t *import );
void dllEntry( int (*syscallptr)( int arg, ... ) );
int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7 );

extern "C" OH_EXPORT game_export_t *OH_GetGameAPI( game_import_t *import ) {
	return GetGameAPI( import );
}

extern "C" OH_EXPORT void OH_dllEntry( int (*syscallptr)( int arg, ... ) ) {
	dllEntry( syscallptr );
}

extern "C" OH_EXPORT int OH_vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7 ) {
	return vmMain( command, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7 );
}

#endif
