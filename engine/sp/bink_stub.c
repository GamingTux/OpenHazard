/*
 * bink_stub.c — Optional no-FFmpeg fallback for Bink (.bik) cinematics.
 *
 * Used automatically when libavformat/libavcodec/libswscale/libavutil dev
 * headers are not installed at configure time. Installing those packages and
 * re-running CMake switches transparently to bink_ff.c for full .bik playback.
 */
#include "bink_ff.h"
#include "sp_log.h"
#include "../efcode/qcommon/q_shared.h"
#include <string.h>

static int g_binkStubWarned = 0;

int Bink_IsBinkFile( const char *name ) {
	size_t n = name ? strlen( name ) : 0;
	return ( n > 4 && !Q_stricmp( name + n - 4, ".bik" ) ) ? 1 : 0;
}

int Bink_Open( int handle, const char *name, int *outW, int *outH ) {
	(void)handle;
	(void)outW;
	(void)outH;
	if ( !g_binkStubWarned ) {
		SP_LogI( "bink", "FFmpeg dev headers were not present at build time; skipping .bik video (%s). "
		                 "Install libavformat-dev libavcodec-dev libswscale-dev libavutil-dev and rebuild to enable Bink cutscenes.",
		                 name ? name : "<null>" );
		g_binkStubWarned = 1;
	}
	return 0;
}

int Bink_Update( int handle, int startMs, int nowMs, int silent, unsigned char **outBuf ) {
	(void)handle;
	(void)startMs;
	(void)nowMs;
	(void)silent;
	if ( outBuf ) *outBuf = NULL;
	return 0;
}

void Bink_Restart( int handle ) {
	(void)handle;
}

void Bink_Stop( int handle ) {
	(void)handle;
}
