/*
 * mp_stubs.c — Clean no-op SP/SPUI hooks for OpenHazard Holomatch (openhazard-mp / openhazard-ded).
 *
 * Allows Singleplayer (openhazard-sp), Holomatch Multiplayer (openhazard-mp), and
 * Dedicated Server (openhazard-ded) to share the exact same engine core, Vulkan
 * renderer (renderer_vulkan.so), and 16-KB-page-hardened ARM64 QVM JIT (vm_aarch64.c).
 */
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#  include <dlfcn.h>
#  include <unwind.h>
#endif
#include "sp_log.h"

#if defined(_WIN32)
void SP_LogBacktrace( int sig ) {
	void *buf[40];
	USHORT n = CaptureStackBackTrace( 0, 40, buf, NULL );
	SP_LogE( "crash", "*** SIGNAL %d (%u frames) ***", sig, (unsigned)n );
	for ( USHORT i = 0; i < n; i++ ) {
		SP_LogE( "crash", "  #%02u  %p", (unsigned)i, buf[i] );
	}
}
#else
struct mp_bt { void **cur; void **end; };
static _Unwind_Reason_Code mp_bt_cb( struct _Unwind_Context *ctx, void *arg ) {
	struct mp_bt *s = (struct mp_bt *)arg;
	void *pc = (void *)_Unwind_GetIP( ctx );
	if ( pc ) {
		if ( s->cur == s->end ) return _URC_END_OF_STACK;
		*s->cur++ = pc;
	}
	return _URC_NO_REASON;
}

void SP_LogBacktrace( int sig ) {
	void *buf[40];
	struct mp_bt s = { buf, buf + 40 };
	_Unwind_Backtrace( mp_bt_cb, &s );
	int n = (int)( s.cur - buf );
	SP_LogE( "crash", "*** SIGNAL %d (%d frames) ***", sig, n );
	for ( int i = 0; i < n; i++ ) {
		Dl_info info;
		if ( dladdr( buf[i], &info ) && info.dli_sname )
			SP_LogE( "crash", "  #%02d  %s + 0x%lx  [%s]", i,
			         info.dli_sname, (unsigned long)( (char *)buf[i] - (char *)info.dli_saddr ),
			         info.dli_fname ? info.dli_fname : "?" );
		else if ( dladdr( buf[i], &info ) && info.dli_fbase )
			SP_LogE( "crash", "  #%02d  %p  [%s + 0x%lx]", i, buf[i],
			         info.dli_fname ? info.dli_fname : "?",
			         (unsigned long)( (char *)buf[i] - (char *)info.dli_fbase ) );
		else
			SP_LogE( "crash", "  #%02d  %p  ?", i, buf[i] );
	}
}
#endif

int  SP_IsActive( void )                                        { return 0; }
int  SP_IsLoading( void )                                       { return 0; }
int  SP_LoadPumping( void )                                     { return 0; }
int  SP_TakeMenuSuppress( void )                                { return 0; }
int  SP_GameConsoleCommand( void )                              { return 0; }
int  SP_FinishTransition( void )                                { return 0; }
void SP_RegisterCommands( void )                                {}
void SP_DrawFrame( int serverTime, int stereo )                 { (void)serverTime; (void)stereo; }
void SP_DrawCGameLoad( void )                                   {}
void SP_DrawLoadingScreen( void )                               {}
void SP_DropHM_Activate( void )                                 {}
void SP_LoadWatchdog( void )                                    {}
void SP_PumpLoadScreen( void )                                  {}

int  SPUI_Load( void )                                          { return 0; }
int  SPUI_IsActive( void )                                      { return 0; }
int  SPUI_IsFullscreen( void )                                  { return 0; }
int  SPUI_ConsoleCommand( void )                                { return 0; }
void SPUI_Shutdown( void )                                      {}
void SPUI_Refresh( int realtime )                               { (void)realtime; }
void SPUI_KeyEvent( int key, int down )                         { (void)key; (void)down; }
void SPUI_MouseEvent( int dx, int dy )                          { (void)dx; (void)dy; }
void SPUI_SetActiveMenu( const char *menu )                     { (void)menu; }
void SPUI_DrawConnect( const char *server, const char *status ) { (void)server; (void)status; }
