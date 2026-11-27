#include "ui_local.h"

//===================================================================
//
// OpenHazard Settings Menu (Vulkan / Desktop FX / HD / Widescreen / HRTF)
//
// Accessible from the 6th Setup top-bar slot (directly under DEFAULTS /
// STANDARD) across all Singleplayer Setup screens. Organized into two
// authentic LCARS left-column sub-tabs:
//   - Page 0: GRAPHICS / FX (MSAA, Render Path, Bloom, SSAO, HDR, Tonemap, CAS, SSAA, Dlights)
//   - Page 1: SYSTEM / AUDIO (Sync/Present, ASTC, LOD Bias, Widescreen FOV, GPU Auto-Tune, HRTF)
//
//===================================================================

static struct
{
	qhandle_t	swooshTop;
	qhandle_t	swooshBottom;
} oh_cache;

static menuframework_s	s_oh_menu;
static menuaction_s		s_oh_apply_action;
static menubitmap_s		s_oh_tab_gfx;
static menubitmap_s		s_oh_tab_sys;

// Page 0: Graphics / FX
static menulist_s		s_oh_msaa_list;
static menulist_s		s_oh_fbo_list;
static menulist_s		s_oh_bloom_list;
static menulist_s		s_oh_ssao_list;
static menulist_s		s_oh_hdr_list;
static menulist_s		s_oh_tonemap_list;
static menulist_s		s_oh_sharpen_list;
static menulist_s		s_oh_ssaa_list;
static menulist_s		s_oh_dlight_list;

// Page 1: System / Audio
static menulist_s		s_oh_present_list;
static menulist_s		s_oh_astc_list;
static menulist_s		s_oh_lodbias_list;
static menulist_s		s_oh_fov_list;
static menulist_s		s_oh_autotune_list;
static menulist_s		s_oh_hrtf_list;

#define ID_OH_MSAA			201
#define ID_OH_FBO			202
#define ID_OH_BLOOM			203
#define ID_OH_SSAO			204
#define ID_OH_HDR			205
#define ID_OH_TONEMAP		206
#define ID_OH_SHARPEN		207
#define ID_OH_SSAA			208
#define ID_OH_DLIGHT		209
#define ID_OH_PRESENT		210
#define ID_OH_ASTC			211
#define ID_OH_LODBIAS		212
#define ID_OH_FOV			213
#define ID_OH_AUTOTUNE		214
#define ID_OH_HRTF			215
#define ID_OH_APPLY			216
#define ID_OH_TAB_GFX		217
#define ID_OH_TAB_SYS		218

typedef struct
{
	int	msaa;
	int	fbo;
	int	bloom;
	int	ssao;
	int	hdr;
	int	tonemap;
	int	sharpen;
	int	ssaa;
	int	dlight;
	int	present;
	int	astc;
	int	lodbias;
	int	fov;
	int	autotune;
	int	hrtf;
} ohValues_t;

static ohValues_t	s_oh_initial;
static ohValues_t	s_oh_current;
static int			s_oh_page = 0;

static const char *s_oh_msaa_en[]     = { "OFF", "2X", "4X", "8X", 0 };
static const char *s_oh_msaa_de[]     = { "AUS", "2X", "4X", "8X", 0 };

static const char *s_oh_fbo_en[]      = { "DIRECT (MAX FPS)", "FBO (FX / HDR)", 0 };
static const char *s_oh_fbo_de[]      = { "DIRECT (MAX FPS)", "FBO (FX / HDR)", 0 };

static const char *s_oh_bloom_en[]    = { "OFF", "1/4-RES (FAST)", "1/2-RES (HIGH)", 0 };
static const char *s_oh_bloom_de[]    = { "AUS", "1/4-RES (SCHNELL)", "1/2-RES (HOCH)", 0 };

static const char *s_oh_ssao_en[]     = { "OFF", "ON (DESKTOP)", 0 };
static const char *s_oh_ssao_de[]     = { "AUS", "AN (DESKTOP)", 0 };

static const char *s_oh_hdr_en[]      = { "OFF (8-BIT)", "ON (16-BIT HDR)", 0 };
static const char *s_oh_hdr_de[]      = { "AUS (8-BIT)", "AN (16-BIT HDR)", 0 };

static const char *s_oh_tonemap_en[]  = { "OFF (CLASSIC)", "FILMIC", 0 };
static const char *s_oh_tonemap_de[]  = { "AUS (KLASSISCH)", "FILMIC", 0 };

static const char *s_oh_sharpen_en[]  = { "OFF", "SUBTLE (0.35)", "CRISP (0.70)", 0 };
static const char *s_oh_sharpen_de[]  = { "AUS", "FEIN (0.35)", "STARK (0.70)", 0 };

static const char *s_oh_ssaa_en[]     = { "OFF", "ON (DESKTOP)", 0 };
static const char *s_oh_ssaa_de[]     = { "AUS", "AN (DESKTOP)", 0 };

static const char *s_oh_dlight_en[]   = { "CLASSIC (0.5)", "ENHANCED (0.9)", "MAX (1.0)", 0 };
static const char *s_oh_dlight_de[]   = { "KLASSISCH (0.5)", "STARK (0.9)", "MAX (1.0)", 0 };

static const char *s_oh_present_en[]  = { "OFF (IMMEDIATE)", "VSYNC (FIFO)", "MAILBOX", 0 };
static const char *s_oh_present_de[]  = { "AUS (MAX FPS)", "VSYNC (FIFO)", "MAILBOX", 0 };

static const char *s_oh_astc_en[]     = { "OFF (RGBA8)", "ON (KTX1 ASTC)", 0 };
static const char *s_oh_astc_de[]     = { "AUS (RGBA8)", "AN (KTX1 ASTC)", 0 };

static const char *s_oh_lod_en[]      = { "NEUTRAL (0.0)", "SHARP (-0.35)", "ULTRA (-0.70)", 0 };
static const char *s_oh_lod_de[]      = { "NEUTRAL (0.0)", "SCHARF (-0.35)", "ULTRA (-0.70)", 0 };

static const char *s_oh_fov_en[]      = { "4:3 (CLASSIC)", "HOR+ (16:9)", 0 };
static const char *s_oh_fov_de[]      = { "4:3 (KLASSISCH)", "HOR+ (16:9)", 0 };

static const char *s_oh_auto_en[]     = { "MANUAL", "AUTO (PI / SBC)", 0 };
static const char *s_oh_auto_de[]     = { "MANUELL", "AUTO (PI / SBC)", 0 };

static const char *s_oh_hrtf_en[]     = { "AUTO", "OFF (STEREO)", "ON (3D AUDIO)", 0 };
static const char *s_oh_hrtf_de[]     = { "AUTO", "AUS (STEREO)", "AN (3D-AUDIO)", 0 };

static void OpenHazard_RebuildMenu( qboolean reloadCvars );

static qboolean OpenHazard_IsGerman( void )
{
	char lang[32];
	ui.Cvar_VariableStringBuffer( "g_language", lang, sizeof( lang ) );
	return ( Q_stricmp( lang, "DEUTSCH" ) == 0 || Q_stricmp( lang, "GERMAN" ) == 0 ) ? qtrue : qfalse;
}

static char *OH_Str( qboolean de, const char *ger, const char *eng )
{
	return (char *)( de ? ger : eng );
}

void UI_OpenHazard_InitLabels( void )
{
	const qboolean de = OpenHazard_IsGerman();

	menu_normal_text[MNT_OH_FOOTNOTE] = (char *)"OPENHAZARD SETUP";

	menu_button_text[MBT_OPENHAZARD][0] = (char *)"OPENHAZARD";
	menu_button_text[MBT_OPENHAZARD][1] = OH_Str( de,
		"OPENHAZARD VULKAN-, DESKTOP-FX-, HD-, WIDESCREEN- UND 3D-AUDIO-OPTIONEN",
		"OPENHAZARD VULKAN, DESKTOP FX, HD, WIDESCREEN AND 3D AUDIO OPTIONS" );

	menu_button_text[MBT_OH_TAB_GFX][0] = OH_Str( de, "GRAFIK / FX", "GRAPHICS / FX" );
	menu_button_text[MBT_OH_TAB_GFX][1] = OH_Str( de,
		"MSAA, RENDER-PFAD, BLOOM, SSAO, 16-BIT HDR, TONEMAPPING & CAS SHARPEN",
		"MSAA, RENDER PATH, BLOOM, SSAO, 16-BIT HDR, TONEMAPPING & CAS SHARPEN" );

	menu_button_text[MBT_OH_TAB_SYS][0] = OH_Str( de, "SYSTEM / AUDIO", "SYSTEM / AUDIO" );
	menu_button_text[MBT_OH_TAB_SYS][1] = OH_Str( de,
		"SYNC / MAILBOX, ASTC-TEXTUREN, LOD-SCHAERFE, HOR+ FOV, AUTO-TUNE & HRTF",
		"SYNC / MAILBOX, ASTC TEXTURES, LOD BIAS, HOR+ FOV, AUTO-TUNE & HRTF" );

	menu_button_text[MBT_OH_MSAA][0] = (char *)"MSAA";
	menu_button_text[MBT_OH_MSAA][1] = OH_Str( de,
		"KANTENGLAETTUNG (4X DIRECT-TO-SWAPCHAIN ON-CHIP AUF PI 5)",
		"MULTI-SAMPLE ANTI-ALIASING (WORKS DIRECT-TO-SWAPCHAIN ON R_FBO 0)" );

	menu_button_text[MBT_OH_RENDERPATH][0] = OH_Str( de, "RENDER-PFAD", "RENDER PATH" );
	menu_button_text[MBT_OH_RENDERPATH][1] = OH_Str( de,
		"DIRECT = MAXIMALE FPS OHNE FBO-KOPIE, FBO = FUER BLOOM / SSAO / HDR",
		"DIRECT = MAXIMUM FPS WITHOUT FBO COPY, FBO = REQUIRED FOR BLOOM / SSAO / HDR" );

	menu_button_text[MBT_OH_BLOOM][0] = (char *)"BLOOM (GLOW)";
	menu_button_text[MBT_OH_BLOOM][1] = OH_Str( de,
		"LEUCHTEFFEKT FUER PHASER & LCARS (1/4-RES SCHNELL AUF PI 5)",
		"GLOW POST-PROCESSING FOR PHASERS & LCARS (1/4-RES RECOMMENDED ON PI 5)" );

	menu_button_text[MBT_OH_SSAO][0] = (char *)"SSAO";
	menu_button_text[MBT_OH_SSAO][1] = OH_Str( de,
		"KONTAKTSCHATTEN IN ECKEN & NISHEN (DESKTOP-GPUS; PI AUTO-TUNE SCHALTET AUS)",
		"SCREEN-SPACE AMBIENT OCCLUSION CONTACT SHADOWS (DESKTOP GPUS; OFF ON PI AUTO-TUNE)" );

	menu_button_text[MBT_OH_HDR][0] = (char *)"16-BIT HDR";
	menu_button_text[MBT_OH_HDR][1] = OH_Str( de,
		"16-BIT FRAMEBUFFER GEGEN FARB-BANDING IN DUNKLEN KORRIDOREN (DESKTOP)",
		"16-BIT COLOR BUFFER ELIMINATES BANDING IN DARK CORRIDORS (DESKTOP GPUS)" );

	menu_button_text[MBT_OH_TONEMAP][0] = (char *)"TONEMAPPING";
	menu_button_text[MBT_OH_TONEMAP][1] = OH_Str( de,
		"FILMIC HIGHLIGHT ROLL-OFF: PHASER & BLOOM BLEIBEN SATT STATT WEISS",
		"FILMIC HIGHLIGHT ROLL-OFF KEEPS PHASERS & BLOOM RICH INSTEAD OF CLIPPING WHITE" );

	menu_button_text[MBT_OH_SHARPEN][0] = (char *)"CAS SHARPEN";
	menu_button_text[MBT_OH_SHARPEN][1] = OH_Str( de,
		"AMD FIDELITYFX CONTRAST ADAPTIVE SHARPENING FUER KNACKIGE HD-TEXTUREN",
		"AMD FIDELITYFX CONTRAST ADAPTIVE SHARPENING FOR CRISP HD TEXTURES" );

	menu_button_text[MBT_OH_SSAA][0] = (char *)"SUPERSAMPLING";
	menu_button_text[MBT_OH_SSAA][1] = OH_Str( de,
		"ZUSAETZLICHES SUPERSAMPLING ANTI-ALIASING FUER STARKE DESKTOP-GPUS",
		"EXTRA SUPERSAMPLE ANTI-ALIASING FOR HIGH-END DESKTOP GPUS" );

	menu_button_text[MBT_OH_DLIGHT][0] = OH_Str( de, "DYNAM. LICHT", "DYNAMIC LIGHTS" );
	menu_button_text[MBT_OH_DLIGHT][1] = OH_Str( de,
		"RADIUS DER PER-PIXEL-LICHTER FUER PHASER, BORG & EXPLOSIONEN (0 MS)",
		"PER-PIXEL DYNAMIC LIGHT RADIUS FOR PHASERS, BORG ALCOVES & EXPLOSIONS (0 MS)" );

	menu_button_text[MBT_OH_PRESENT][0] = (char *)"SYNC / PRESENT";
	menu_button_text[MBT_OH_PRESENT][1] = OH_Str( de,
		"MAILBOX = KEIN TEARING & MINIMALER LAG, AUS = UNBEGRENZTE FPS",
		"MAILBOX = TEAR-FREE LOW LATENCY, OFF = UNCAPPED FPS, VSYNC = 60 HZ" );

	menu_button_text[MBT_OH_ASTC][0] = OH_Str( de, "ASTC-TEXTUREN", "ASTC TEXTURES" );
	menu_button_text[MBT_OH_ASTC][1] = OH_Str( de,
		"HARDWARE-ASTC KTX1 HD-PACKS (~70% WENIGER VRAM AUF PI/ARM64/APPLE)",
		"HARDWARE-ASTC KTX1 HD PACKS (~70% LESS VRAM ON PI / ARM64 / APPLE)" );

	menu_button_text[MBT_OH_LODBIAS][0] = OH_Str( de, "TEXTUR-LOD", "TEXTURE LOD" );
	menu_button_text[MBT_OH_LODBIAS][1] = OH_Str( de,
		"HARDWARE-MIPMAP-SCHAERFE BEI ANISOTROPISCHER FILTERUNG (0 MS)",
		"HARDWARE SAMPLER MIPMAP SHARPNESS WITH ANISOTROPIC FILTERING (0 MS)" );

	menu_button_text[MBT_OH_FOV][0] = (char *)"WIDESCREEN FOV";
	menu_button_text[MBT_OH_FOV][1] = OH_Str( de,
		"HOR+ BEHAELT 4:3-BILDHOEHE BEI 16:9/21:9 (KEIN TUNNELBLICK)",
		"HOR+ PRESERVES RETAIL VERTICAL FOV ON 16:9 / 21:9 DISPLAYS" );

	menu_button_text[MBT_OH_AUTOTUNE][0] = (char *)"GPU AUTO-TUNE";
	menu_button_text[MBT_OH_AUTOTUNE][1] = OH_Str( de,
		"AUTOMATISCHE OPTIMIERUNG FUER RASPBERRY PI 4/5 & SBC-GPUS",
		"AUTOMATIC MSAA / AF / BLOOM / SSAO TUNING FOR RASPBERRY PI 4/5 & SBC GPUS" );

	menu_button_text[MBT_OH_HRTF][0] = (char *)"OPENAL 3D HRTF";
	menu_button_text[MBT_OH_HRTF][1] = OH_Str( de,
		"BINAURALER 3D-RAUMKLANG FUER KOPFHOERER (AUTO / AUS / AN)",
		"OPENAL SOFT BINAURAL 3D HEADPHONE SPATIALIZATION (AUTO / OFF / ON)" );
}

static void OpenHazard_ReadCvars( ohValues_t *v )
{
	int msaa     = (int)ui.Cvar_VariableValue( "r_ext_multisample" );
	int fbo      = (int)ui.Cvar_VariableValue( "r_fbo" );
	int bloom    = (int)ui.Cvar_VariableValue( "r_bloom" );
	int bdown    = (int)ui.Cvar_VariableValue( "r_bloom_downscale" );
	int ssao     = (int)ui.Cvar_VariableValue( "r_ssao" );
	int hdr      = (int)ui.Cvar_VariableValue( "r_hdr" );
	int tonemap  = (int)ui.Cvar_VariableValue( "r_tonemap" );
	float sharp  = ui.Cvar_VariableValue( "r_sharpen" );
	int ssaa     = (int)ui.Cvar_VariableValue( "r_ext_supersample" );
	float dl     = ui.Cvar_VariableValue( "r_dlightScale" );
	int swap     = (int)ui.Cvar_VariableValue( "r_swapInterval" );
	int astc     = (int)ui.Cvar_VariableValue( "r_ext_compressed_textures" );
	float lod    = ui.Cvar_VariableValue( "r_mipLodBias" );
	int fov      = (int)ui.Cvar_VariableValue( "cg_fovAspectAdjust" );
	int autotune = (int)ui.Cvar_VariableValue( "r_gpuAutoTune" );
	int hrtf     = (int)ui.Cvar_VariableValue( "s_alHrtf" );

	if ( msaa >= 8 )      v->msaa = 3;
	else if ( msaa >= 4 ) v->msaa = 2;
	else if ( msaa >= 2 ) v->msaa = 1;
	else                  v->msaa = 0;

	v->fbo = fbo ? 1 : 0;

	if ( !bloom || !fbo )     v->bloom = 0;
	else if ( bdown == 2 )    v->bloom = 2;
	else                      v->bloom = 1;

	v->ssao    = ( ssao && fbo ) ? 1 : 0;
	v->hdr     = ( hdr > 0 && fbo ) ? 1 : 0;
	v->tonemap = ( tonemap && fbo ) ? 1 : 0;

	if ( !fbo || sharp < 0.10f ) v->sharpen = 0;
	else if ( sharp >= 0.55f )   v->sharpen = 2;
	else                         v->sharpen = 1;

	v->ssaa = ( ssaa && fbo ) ? 1 : 0;

	if ( dl >= 0.95f )        v->dlight = 2;
	else if ( dl >= 0.70f )   v->dlight = 1;
	else                      v->dlight = 0;

	if ( swap >= 2 )          v->present = 2;
	else if ( swap == 1 )     v->present = 1;
	else                      v->present = 0;

	v->astc = astc ? 1 : 0;

	if ( lod <= -0.55f )      v->lodbias = 2;
	else if ( lod <= -0.15f ) v->lodbias = 1;
	else                      v->lodbias = 0;

	v->fov = fov ? 1 : 0;
	v->autotune = autotune ? 1 : 0;

	if ( hrtf > 0 )           v->hrtf = 2;
	else if ( hrtf == 0 )     v->hrtf = 1;
	else                      v->hrtf = 0;
}

static void OpenHazard_SyncControlsFromCurrent( void )
{
	s_oh_msaa_list.curvalue     = s_oh_current.msaa;
	s_oh_fbo_list.curvalue      = s_oh_current.fbo;
	s_oh_bloom_list.curvalue    = s_oh_current.bloom;
	s_oh_ssao_list.curvalue     = s_oh_current.ssao;
	s_oh_hdr_list.curvalue      = s_oh_current.hdr;
	s_oh_tonemap_list.curvalue  = s_oh_current.tonemap;
	s_oh_sharpen_list.curvalue  = s_oh_current.sharpen;
	s_oh_ssaa_list.curvalue     = s_oh_current.ssaa;
	s_oh_dlight_list.curvalue   = s_oh_current.dlight;
	s_oh_present_list.curvalue  = s_oh_current.present;
	s_oh_astc_list.curvalue     = s_oh_current.astc;
	s_oh_lodbias_list.curvalue  = s_oh_current.lodbias;
	s_oh_fov_list.curvalue      = s_oh_current.fov;
	s_oh_autotune_list.curvalue = s_oh_current.autotune;
	s_oh_hrtf_list.curvalue     = s_oh_current.hrtf;
}

static void OpenHazard_UpdateApplyState( void )
{
	qboolean changed = ( memcmp( &s_oh_current, &s_oh_initial, sizeof( s_oh_current ) ) != 0 ) ? qtrue : qfalse;

	if ( changed )
	{
		s_oh_apply_action.generic.flags &= ~QMF_GRAYED;
		s_oh_apply_action.generic.flags |= QMF_BLINK;
	}
	else
	{
		s_oh_apply_action.generic.flags |= QMF_GRAYED;
		s_oh_apply_action.generic.flags &= ~QMF_BLINK;
	}
}

static void OpenHazard_TabCallback( void *ptr, int notification )
{
	int id;
	if ( notification != QM_ACTIVATED )
		return;

	id = ((menucommon_s *)ptr)->id;
	if ( id == ID_OH_TAB_GFX && s_oh_page != 0 )
	{
		s_oh_page = 0;
		OpenHazard_RebuildMenu( qfalse );
	}
	else if ( id == ID_OH_TAB_SYS && s_oh_page != 1 )
	{
		s_oh_page = 1;
		OpenHazard_RebuildMenu( qfalse );
	}
}

static void OpenHazard_ControlCallback( void *ptr, int notification )
{
	int id;
	if ( notification != QM_ACTIVATED )
		return;

	id = ((menucommon_s *)ptr)->id;

	switch ( id )
	{
		case ID_OH_MSAA:     s_oh_current.msaa     = s_oh_msaa_list.curvalue; break;
		case ID_OH_FBO:      s_oh_current.fbo      = s_oh_fbo_list.curvalue; break;
		case ID_OH_BLOOM:    s_oh_current.bloom    = s_oh_bloom_list.curvalue; break;
		case ID_OH_SSAO:     s_oh_current.ssao     = s_oh_ssao_list.curvalue; break;
		case ID_OH_HDR:      s_oh_current.hdr      = s_oh_hdr_list.curvalue; break;
		case ID_OH_TONEMAP:  s_oh_current.tonemap  = s_oh_tonemap_list.curvalue; break;
		case ID_OH_SHARPEN:  s_oh_current.sharpen  = s_oh_sharpen_list.curvalue; break;
		case ID_OH_SSAA:     s_oh_current.ssaa     = s_oh_ssaa_list.curvalue; break;
		case ID_OH_DLIGHT:   s_oh_current.dlight   = s_oh_dlight_list.curvalue; break;
		case ID_OH_PRESENT:  s_oh_current.present  = s_oh_present_list.curvalue; break;
		case ID_OH_ASTC:     s_oh_current.astc     = s_oh_astc_list.curvalue; break;
		case ID_OH_LODBIAS:  s_oh_current.lodbias  = s_oh_lodbias_list.curvalue; break;
		case ID_OH_FOV:      s_oh_current.fov      = s_oh_fov_list.curvalue; break;
		case ID_OH_AUTOTUNE: s_oh_current.autotune = s_oh_autotune_list.curvalue; break;
		case ID_OH_HRTF:     s_oh_current.hrtf     = s_oh_hrtf_list.curvalue; break;
	}

	// Keep FBO-dependent FX and Render Path consistent
	if ( id == ID_OH_FBO && s_oh_current.fbo == 0 )
	{
		s_oh_current.bloom   = 0;
		s_oh_current.ssao    = 0;
		s_oh_current.hdr     = 0;
		s_oh_current.tonemap = 0;
		s_oh_current.sharpen = 0;
		s_oh_current.ssaa    = 0;
	}
	else if ( s_oh_current.fbo == 0 &&
	          ( s_oh_current.bloom > 0 || s_oh_current.ssao > 0 || s_oh_current.hdr > 0 ||
	            s_oh_current.tonemap > 0 || s_oh_current.sharpen > 0 || s_oh_current.ssaa > 0 ) )
	{
		s_oh_current.fbo = 1;
	}

	OpenHazard_SyncControlsFromCurrent();
	OpenHazard_UpdateApplyState();
}

static void OpenHazard_ApplyChanges( void *unused, int notification )
{
	static const int   msaaVals[]    = { 0, 2, 4, 8 };
	static const float sharpVals[]   = { 0.0f, 0.35f, 0.70f };
	static const float lodVals[]     = { 0.0f, -0.35f, -0.70f };
	static const float dlightVals[]  = { 0.5f, 0.9f, 1.0f };
	static const int   hrtfVals[]    = { -1, 0, 1 };

	qboolean needVidRestart = qfalse;
	qboolean needSndRestart = qfalse;

	if ( notification != QM_ACTIVATED )
		return;

	if ( s_oh_current.msaa     != s_oh_initial.msaa ||
	     s_oh_current.fbo      != s_oh_initial.fbo ||
	     s_oh_current.bloom    != s_oh_initial.bloom ||
	     s_oh_current.ssao     != s_oh_initial.ssao ||
	     s_oh_current.hdr      != s_oh_initial.hdr ||
	     s_oh_current.ssaa     != s_oh_initial.ssaa ||
	     s_oh_current.present  != s_oh_initial.present ||
	     s_oh_current.astc     != s_oh_initial.astc ||
	     s_oh_current.lodbias  != s_oh_initial.lodbias ||
	     s_oh_current.autotune != s_oh_initial.autotune )
	{
		needVidRestart = qtrue;
	}

	if ( s_oh_current.hrtf != s_oh_initial.hrtf )
	{
		needSndRestart = qtrue;
	}

	ui.Cvar_SetValue( "r_ext_multisample", (float)msaaVals[ s_oh_current.msaa ] );
	ui.Cvar_SetValue( "r_fbo", (float)s_oh_current.fbo );

	if ( s_oh_current.bloom == 0 )
	{
		ui.Cvar_SetValue( "r_bloom", 0 );
	}
	else if ( s_oh_current.bloom == 1 )
	{
		ui.Cvar_SetValue( "r_bloom", 1 );
		ui.Cvar_SetValue( "r_bloom_downscale", 4 );
	}
	else
	{
		ui.Cvar_SetValue( "r_bloom", 1 );
		ui.Cvar_SetValue( "r_bloom_downscale", 2 );
	}

	ui.Cvar_SetValue( "r_ssao", (float)s_oh_current.ssao );
	ui.Cvar_SetValue( "r_hdr", (float)s_oh_current.hdr );
	ui.Cvar_SetValue( "r_tonemap", (float)s_oh_current.tonemap );
	ui.Cvar_SetValue( "r_sharpen", sharpVals[ s_oh_current.sharpen ] );
	ui.Cvar_SetValue( "r_ext_supersample", (float)s_oh_current.ssaa );
	ui.Cvar_SetValue( "r_dlightScale", dlightVals[ s_oh_current.dlight ] );
	ui.Cvar_SetValue( "r_swapInterval", (float)s_oh_current.present );
	ui.Cvar_SetValue( "r_ext_compressed_textures", (float)s_oh_current.astc );
	ui.Cvar_SetValue( "r_mipLodBias", lodVals[ s_oh_current.lodbias ] );
	ui.Cvar_SetValue( "cg_fovAspectAdjust", (float)s_oh_current.fov );
	ui.Cvar_SetValue( "r_gpuAutoTune", (float)s_oh_current.autotune );
	ui.Cvar_SetValue( "s_alHrtf", (float)hrtfVals[ s_oh_current.hrtf ] );

	s_oh_initial = s_oh_current;
	OpenHazard_UpdateApplyState();

	if ( needSndRestart )
	{
		ui.Cmd_ExecuteText( EXEC_APPEND, "snd_restart 1\n" );
	}
	if ( needVidRestart )
	{
		ui.Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
	}
}

static void OpenHazard_MenuDraw( void )
{
	UI_MenuFrame( &s_oh_menu );

	UI_DrawProportionalString( 74,  66, "47-01",  UI_RIGHT | UI_TINYFONT, colorTable[CT_BLACK] );
	UI_DrawProportionalString( 74,  84, "74656",  UI_RIGHT | UI_TINYFONT, colorTable[CT_BLACK] );
	UI_DrawProportionalString( 74, 188, "101-VK", UI_RIGHT | UI_TINYFONT, colorTable[CT_BLACK] );
	UI_DrawProportionalString( 74, 395, "47988",  UI_RIGHT | UI_TINYFONT, colorTable[CT_BLACK] );

	UI_Setup_MenuButtons();

	// Decorative LCARS column below the 2 left-side sub-tabs
	ui.R_SetColor( colorTable[CT_DKPURPLE2] );
	UI_DrawHandlePic( 30, 256, 47, 133, uis.whiteShader );
	UI_DrawProportionalString( 74, 260, "250624", UI_RIGHT | UI_TINYFONT, colorTable[CT_BLACK] );

	// LCARS bracket around OpenHazard settings (matches Video menu frame)
	ui.R_SetColor( colorTable[CT_LTPURPLE1] );
	UI_DrawHandlePic( 158, 163,  16,  16, uis.graphicBracket1CornerLU );
	UI_DrawHandlePic( 158, 179,   8, 233, uis.whiteShader );
	UI_DrawHandlePic( 158, 412,  16, -16, uis.graphicBracket1CornerLU );

	UI_DrawHandlePic( 174, 163, 320,   8, uis.whiteShader );
	UI_DrawHandlePic( 494, 163, 128, 128, oh_cache.swooshTop );

	UI_DrawHandlePic( 501, 188, 110,  54, uis.whiteShader );
	UI_DrawHandlePic( 501, 348, 110,  55, uis.whiteShader );

	UI_DrawHandlePic( 494, 406, 128, 128, oh_cache.swooshBottom );
	UI_DrawHandlePic( 174, 420, 320,   8, uis.whiteShader );

	UI_DrawProportionalString( 606, 193, s_oh_page == 0 ? "VULKAN FX" : "SYS / AUDIO", UI_RIGHT | UI_TINYFONT, colorTable[CT_BLACK] );
	UI_DrawProportionalString( 606, 390, s_oh_page == 0 ? "SSAO / HDR" : "ASTC / HRTF", UI_RIGHT | UI_TINYFONT, colorTable[CT_BLACK] );

	Menu_Draw( &s_oh_menu );
}

static sfxHandle_t OpenHazard_MenuKey( int key )
{
	return Menu_DefaultKey( &s_oh_menu, key );
}

static void OpenHazard_InitSpin( menulist_s *ctrl, int x, int y, int width, int id, int textEnum, const char **items )
{
	ctrl->generic.type     = MTYPE_SPINCONTROL;
	ctrl->generic.flags    = QMF_HIGHLIGHT_IF_FOCUS;
	ctrl->generic.x        = x;
	ctrl->generic.y        = y;
	ctrl->generic.id       = id;
	ctrl->generic.callback = OpenHazard_ControlCallback;
	ctrl->textEnum         = textEnum;
	ctrl->textcolor        = CT_BLACK;
	ctrl->textcolor2       = CT_WHITE;
	ctrl->color            = CT_DKPURPLE1;
	ctrl->color2           = CT_LTPURPLE1;
	ctrl->textX            = 5;
	ctrl->textY            = 2;
	ctrl->width            = width;
	ctrl->listnames        = NULL;
	ctrl->itemnames        = items;
}

void UI_OpenHazardMenu_Cache( void )
{
	oh_cache.swooshTop    = ui.R_RegisterShaderNoMip( "menu/common/swoosh_top.tga" );
	oh_cache.swooshBottom = ui.R_RegisterShaderNoMip( "menu/common/swoosh_bottom.tga" );
	UI_OpenHazard_InitLabels();
}

static void OpenHazard_RebuildMenu( qboolean reloadCvars )
{
	const qboolean de = OpenHazard_IsGerman();
	int x = 170;
	int y;
	int inc;
	int width = 148;

	UI_OpenHazardMenu_Cache();

	memset( &s_oh_menu, 0, sizeof( s_oh_menu ) );

	s_oh_menu.nitems               = 0;
	s_oh_menu.wrapAround           = qtrue;
	s_oh_menu.draw                 = OpenHazard_MenuDraw;
	s_oh_menu.key                  = OpenHazard_MenuKey;
	s_oh_menu.fullscreen           = qtrue;
	s_oh_menu.descX                = MENU_DESC_X;
	s_oh_menu.descY                = MENU_DESC_Y;
	s_oh_menu.titleX               = MENU_TITLE_X;
	s_oh_menu.titleY               = MENU_TITLE_Y;
	s_oh_menu.titleI               = MNT_CONTROLSMENU_TITLE;
	s_oh_menu.footNoteEnum         = MNT_OH_FOOTNOTE;

	// Left-column LCARS sub-tabs (matches Controls SetupMenu_SideButtons geometry)
	s_oh_tab_gfx.generic.type     = MTYPE_BITMAP;
	s_oh_tab_gfx.generic.flags    = ( s_oh_page == 0 ) ? QMF_GRAYED : QMF_HIGHLIGHT_IF_FOCUS;
	s_oh_tab_gfx.generic.x        = 30;
	s_oh_tab_gfx.generic.y        = 204;
	s_oh_tab_gfx.generic.name     = "menu/common/square.tga";
	s_oh_tab_gfx.generic.id       = ID_OH_TAB_GFX;
	s_oh_tab_gfx.generic.callback = OpenHazard_TabCallback;
	s_oh_tab_gfx.width            = MENU_BUTTON_MED_WIDTH - 10;
	s_oh_tab_gfx.height           = MENU_BUTTON_MED_HEIGHT;
	s_oh_tab_gfx.color            = CT_DKPURPLE1;
	s_oh_tab_gfx.color2           = CT_LTPURPLE1;
	s_oh_tab_gfx.textX            = 5;
	s_oh_tab_gfx.textY            = 1;
	s_oh_tab_gfx.textEnum         = MBT_OH_TAB_GFX;
	s_oh_tab_gfx.textcolor        = ( s_oh_page == 0 ) ? CT_LTGOLD1 : CT_BLACK;
	s_oh_tab_gfx.textcolor2       = ( s_oh_page == 0 ) ? CT_LTGOLD1 : CT_WHITE;

	s_oh_tab_sys.generic.type     = MTYPE_BITMAP;
	s_oh_tab_sys.generic.flags    = ( s_oh_page == 1 ) ? QMF_GRAYED : QMF_HIGHLIGHT_IF_FOCUS;
	s_oh_tab_sys.generic.x        = 30;
	s_oh_tab_sys.generic.y        = 204 + 6 + MENU_BUTTON_MED_HEIGHT;
	s_oh_tab_sys.generic.name     = "menu/common/square.tga";
	s_oh_tab_sys.generic.id       = ID_OH_TAB_SYS;
	s_oh_tab_sys.generic.callback = OpenHazard_TabCallback;
	s_oh_tab_sys.width            = MENU_BUTTON_MED_WIDTH - 10;
	s_oh_tab_sys.height           = MENU_BUTTON_MED_HEIGHT;
	s_oh_tab_sys.color            = CT_DKPURPLE1;
	s_oh_tab_sys.color2           = CT_LTPURPLE1;
	s_oh_tab_sys.textX            = 5;
	s_oh_tab_sys.textY            = 1;
	s_oh_tab_sys.textEnum         = MBT_OH_TAB_SYS;
	s_oh_tab_sys.textcolor        = ( s_oh_page == 1 ) ? CT_LTGOLD1 : CT_BLACK;
	s_oh_tab_sys.textcolor2       = ( s_oh_page == 1 ) ? CT_LTGOLD1 : CT_WHITE;

	s_oh_apply_action.generic.type     = MTYPE_ACTION;
	s_oh_apply_action.generic.flags    = QMF_HIGHLIGHT_IF_FOCUS | QMF_GRAYED;
	s_oh_apply_action.generic.x        = 501;
	s_oh_apply_action.generic.y        = 245;
	s_oh_apply_action.generic.id       = ID_OH_APPLY;
	s_oh_apply_action.generic.callback = OpenHazard_ApplyChanges;
	s_oh_apply_action.textEnum         = MBT_VIDEOAPPLYCHANGES;
	s_oh_apply_action.textcolor        = CT_BLACK;
	s_oh_apply_action.textcolor2       = CT_WHITE;
	s_oh_apply_action.textcolor3       = CT_LTGREY;
	s_oh_apply_action.color            = CT_DKPURPLE1;
	s_oh_apply_action.color2           = CT_LTPURPLE1;
	s_oh_apply_action.color3           = CT_DKGREY;
	s_oh_apply_action.textX            = 5;
	s_oh_apply_action.textY            = 80;
	s_oh_apply_action.width            = 110;
	s_oh_apply_action.height           = 100;

	SetupMenu_TopButtons( &s_oh_menu, MENU_OPENHAZARD, &s_oh_apply_action );
	Menu_AddItem( &s_oh_menu, (void *)&s_oh_tab_gfx );
	Menu_AddItem( &s_oh_menu, (void *)&s_oh_tab_sys );

	if ( s_oh_page == 0 )
	{
		y = 176;
		inc = 25;
		OpenHazard_InitSpin( &s_oh_msaa_list,    x, y, width, ID_OH_MSAA,    MBT_OH_MSAA,       de ? s_oh_msaa_de    : s_oh_msaa_en );    y += inc;
		OpenHazard_InitSpin( &s_oh_fbo_list,     x, y, width, ID_OH_FBO,     MBT_OH_RENDERPATH, de ? s_oh_fbo_de     : s_oh_fbo_en );     y += inc;
		OpenHazard_InitSpin( &s_oh_bloom_list,   x, y, width, ID_OH_BLOOM,   MBT_OH_BLOOM,      de ? s_oh_bloom_de   : s_oh_bloom_en );   y += inc;
		OpenHazard_InitSpin( &s_oh_ssao_list,    x, y, width, ID_OH_SSAO,    MBT_OH_SSAO,       de ? s_oh_ssao_de    : s_oh_ssao_en );    y += inc;
		OpenHazard_InitSpin( &s_oh_hdr_list,     x, y, width, ID_OH_HDR,     MBT_OH_HDR,        de ? s_oh_hdr_de     : s_oh_hdr_en );     y += inc;
		OpenHazard_InitSpin( &s_oh_tonemap_list, x, y, width, ID_OH_TONEMAP, MBT_OH_TONEMAP,    de ? s_oh_tonemap_de : s_oh_tonemap_en ); y += inc;
		OpenHazard_InitSpin( &s_oh_sharpen_list, x, y, width, ID_OH_SHARPEN, MBT_OH_SHARPEN,    de ? s_oh_sharpen_de : s_oh_sharpen_en ); y += inc;
		OpenHazard_InitSpin( &s_oh_ssaa_list,    x, y, width, ID_OH_SSAA,    MBT_OH_SSAA,       de ? s_oh_ssaa_de    : s_oh_ssaa_en );    y += inc;
		OpenHazard_InitSpin( &s_oh_dlight_list,  x, y, width, ID_OH_DLIGHT,  MBT_OH_DLIGHT,     de ? s_oh_dlight_de  : s_oh_dlight_en );

		Menu_AddItem( &s_oh_menu, (void *)&s_oh_msaa_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_fbo_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_bloom_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_ssao_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_hdr_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_tonemap_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_sharpen_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_ssaa_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_dlight_list );
	}
	else
	{
		y = 186;
		inc = 30;
		OpenHazard_InitSpin( &s_oh_present_list,  x, y, width, ID_OH_PRESENT,  MBT_OH_PRESENT,  de ? s_oh_present_de : s_oh_present_en ); y += inc;
		OpenHazard_InitSpin( &s_oh_astc_list,     x, y, width, ID_OH_ASTC,     MBT_OH_ASTC,     de ? s_oh_astc_de    : s_oh_astc_en );    y += inc;
		OpenHazard_InitSpin( &s_oh_lodbias_list,  x, y, width, ID_OH_LODBIAS,  MBT_OH_LODBIAS,  de ? s_oh_lod_de     : s_oh_lod_en );     y += inc;
		OpenHazard_InitSpin( &s_oh_fov_list,      x, y, width, ID_OH_FOV,      MBT_OH_FOV,      de ? s_oh_fov_de     : s_oh_fov_en );     y += inc;
		OpenHazard_InitSpin( &s_oh_autotune_list, x, y, width, ID_OH_AUTOTUNE, MBT_OH_AUTOTUNE, de ? s_oh_auto_de    : s_oh_auto_en );    y += inc;
		OpenHazard_InitSpin( &s_oh_hrtf_list,     x, y, width, ID_OH_HRTF,     MBT_OH_HRTF,     de ? s_oh_hrtf_de    : s_oh_hrtf_en );

		Menu_AddItem( &s_oh_menu, (void *)&s_oh_present_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_astc_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_lodbias_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_fov_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_autotune_list );
		Menu_AddItem( &s_oh_menu, (void *)&s_oh_hrtf_list );
	}

	Menu_AddItem( &s_oh_menu, (void *)&s_oh_apply_action );

	if ( reloadCvars )
	{
		OpenHazard_ReadCvars( &s_oh_initial );
		s_oh_current = s_oh_initial;
	}
	OpenHazard_SyncControlsFromCurrent();
	OpenHazard_UpdateApplyState();
}

void UI_OpenHazardMenu( void )
{
	s_oh_page = 0;
	OpenHazard_RebuildMenu( qtrue );
	UI_PushMenu( &s_oh_menu );
}
