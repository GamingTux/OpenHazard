#include "ui_local.h"

//===================================================================
//
// OpenHazard Settings Menu (Vulkan / HD / Widescreen / OpenAL HRTF)
//
// Accessible from the 6th Setup top-bar slot (directly under DEFAULTS /
// STANDARD) across all Singleplayer Setup screens.
//
//===================================================================

static struct
{
	qhandle_t	swooshTop;
	qhandle_t	swooshBottom;
} oh_cache;

static menuframework_s	s_oh_menu;
static menuaction_s		s_oh_apply_action;

static menulist_s		s_oh_msaa_list;
static menulist_s		s_oh_fbo_list;
static menulist_s		s_oh_bloom_list;
static menulist_s		s_oh_present_list;
static menulist_s		s_oh_astc_list;
static menulist_s		s_oh_lodbias_list;
static menulist_s		s_oh_dlight_list;
static menulist_s		s_oh_fov_list;
static menulist_s		s_oh_autotune_list;
static menulist_s		s_oh_hrtf_list;

#define ID_OH_MSAA			201
#define ID_OH_FBO			202
#define ID_OH_BLOOM			203
#define ID_OH_PRESENT		204
#define ID_OH_ASTC			205
#define ID_OH_LODBIAS		206
#define ID_OH_DLIGHT		207
#define ID_OH_FOV			208
#define ID_OH_AUTOTUNE		209
#define ID_OH_HRTF			210
#define ID_OH_APPLY			211

typedef struct
{
	int	msaa;
	int	fbo;
	int	bloom;
	int	present;
	int	astc;
	int	lodbias;
	int	dlight;
	int	fov;
	int	autotune;
	int	hrtf;
} ohInitialValues_t;

static ohInitialValues_t	s_oh_initial;

static const char *s_oh_msaa_en[]     = { "OFF", "2X", "4X", "8X", 0 };
static const char *s_oh_msaa_de[]     = { "AUS", "2X", "4X", "8X", 0 };

static const char *s_oh_fbo_en[]      = { "DIRECT (FAST)", "FBO (BLOOM)", 0 };
static const char *s_oh_fbo_de[]      = { "DIRECT (MAX FPS)", "FBO (BLOOM)", 0 };

static const char *s_oh_bloom_en[]    = { "OFF", "1/4-RES (FAST)", "1/2-RES (HIGH)", 0 };
static const char *s_oh_bloom_de[]    = { "AUS", "1/4-RES (SCHNELL)", "1/2-RES (HOCH)", 0 };

static const char *s_oh_present_en[]  = { "OFF (IMMEDIATE)", "VSYNC (FIFO)", "MAILBOX", 0 };
static const char *s_oh_present_de[]  = { "AUS (MAX FPS)", "VSYNC (FIFO)", "MAILBOX", 0 };

static const char *s_oh_astc_en[]     = { "OFF (RGBA8)", "ON (KTX1 ASTC)", 0 };
static const char *s_oh_astc_de[]     = { "AUS (RGBA8)", "AN (KTX1 ASTC)", 0 };

static const char *s_oh_lod_en[]      = { "NEUTRAL (0.0)", "SHARP (-0.35)", "ULTRA (-0.70)", 0 };
static const char *s_oh_lod_de[]      = { "NEUTRAL (0.0)", "SCHARF (-0.35)", "ULTRA (-0.70)", 0 };

static const char *s_oh_dlight_en[]   = { "CLASSIC (0.5)", "ENHANCED (0.9)", "MAX (1.0)", 0 };
static const char *s_oh_dlight_de[]   = { "KLASSISCH (0.5)", "STARK (0.9)", "MAX (1.0)", 0 };

static const char *s_oh_fov_en[]      = { "4:3 (CLASSIC)", "HOR+ (16:9)", 0 };
static const char *s_oh_fov_de[]      = { "4:3 (KLASSISCH)", "HOR+ (16:9)", 0 };

static const char *s_oh_auto_en[]     = { "MANUAL", "AUTO (PI / SBC)", 0 };
static const char *s_oh_auto_de[]     = { "MANUELL", "AUTO (PI / SBC)", 0 };

static const char *s_oh_hrtf_en[]     = { "AUTO", "OFF (STEREO)", "ON (3D AUDIO)", 0 };
static const char *s_oh_hrtf_de[]     = { "AUTO", "AUS (STEREO)", "AN (3D-AUDIO)", 0 };

static qboolean OpenHazard_IsGerman( void )
{
	char lang[32];
	ui.Cvar_VariableStringBuffer( "g_language", lang, sizeof( lang ) );
	return ( Q_stricmp( lang, "DEUTSCH" ) == 0 || Q_stricmp( lang, "GERMAN" ) == 0 ) ? qtrue : qfalse;
}

/*
=================
UI_OpenHazard_InitLabels

Assigns C-side labels & bottom-bar descriptions for appended MBT_OH_* /
MNT_OH_FOOTNOTE enum slots (never inserted mid-table so retail .dat files
stay 100% aligned).
=================
*/
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
		"OPENHAZARD VULKAN-, HD-, WIDESCREEN- UND 3D-AUDIO-OPTIONEN",
		"OPENHAZARD VULKAN, HD, WIDESCREEN AND 3D AUDIO OPTIONS" );

	menu_button_text[MBT_OH_MSAA][0] = (char *)"MSAA";
	menu_button_text[MBT_OH_MSAA][1] = OH_Str( de,
		"KANTENGLAETTUNG (4X DIRECT-TO-SWAPCHAIN ON-CHIP AUF PI 5)",
		"MULTI-SAMPLE ANTI-ALIASING (WORKS DIRECT-TO-SWAPCHAIN ON R_FBO 0)" );

	menu_button_text[MBT_OH_RENDERPATH][0] = OH_Str( de, "RENDER-PFAD", "RENDER PATH" );
	menu_button_text[MBT_OH_RENDERPATH][1] = OH_Str( de,
		"DIRECT = MAXIMALE FPS OHNE FBO-KOPIE, FBO = FUER BLOOM / HDR",
		"DIRECT = MAXIMUM FPS WITHOUT FBO COPY, FBO = REQUIRED FOR BLOOM / HDR" );

	menu_button_text[MBT_OH_BLOOM][0] = (char *)"BLOOM (GLOW)";
	menu_button_text[MBT_OH_BLOOM][1] = OH_Str( de,
		"LEUCHTEFFEKT FUER PHASER & LCARS (1/4-RES SCHNELL AUF PI 5)",
		"GLOW POST-PROCESSING FOR PHASERS & LCARS (1/4-RES RECOMMENDED ON PI 5)" );

	menu_button_text[MBT_OH_PRESENT][0] = (char *)"SYNC / PRESENT";
	menu_button_text[MBT_OH_PRESENT][1] = OH_Str( de,
		"MAILBOX = KEIN TEARING & MINIMALER LAG, AUS = UNBEGRENZTE FPS",
		"MAILBOX = TEAR-FREE LOW LATENCY, OFF = UNCAPPED FPS, VSYNC = 60 HZ" );

	menu_button_text[MBT_OH_ASTC][0] = OH_Str( de, "ASTC-TEXTUREN", "ASTC TEXTURES" );
	menu_button_text[MBT_OH_ASTC][1] = OH_Str( de,
		"HARDWARE-ASTC KTX1 HD-PACKS (~70% WENIGER VRAM AUF PI/ARM64)",
		"HARDWARE-ASTC KTX1 HD PACKS (~70% LESS VRAM ON PI / ARM64 / APPLE)" );

	menu_button_text[MBT_OH_LODBIAS][0] = OH_Str( de, "TEXTUR-LOD", "TEXTURE LOD" );
	menu_button_text[MBT_OH_LODBIAS][1] = OH_Str( de,
		"HARDWARE-MIPMAP-SCHAERFE BEI ANISOTROPISCHER FILTERUNG (0 MS)",
		"HARDWARE SAMPLER MIPMAP SHARPNESS WITH ANISOTROPIC FILTERING (0 MS)" );

	menu_button_text[MBT_OH_DLIGHT][0] = OH_Str( de, "DYNAM. LICHT", "DYNAMIC LIGHTS" );
	menu_button_text[MBT_OH_DLIGHT][1] = OH_Str( de,
		"RADIUS DER PER-PIXEL-LICHTER FUER PHASER, BORG & EXPLOSIONEN",
		"PER-PIXEL DYNAMIC LIGHT RADIUS FOR PHASERS, BORG ALCOVES & EXPLOSIONS" );

	menu_button_text[MBT_OH_FOV][0] = (char *)"WIDESCREEN FOV";
	menu_button_text[MBT_OH_FOV][1] = OH_Str( de,
		"HOR+ BEHAELT 4:3-BILDHOEHE BEI 16:9/21:9 (KEIN TUNNELBLICK)",
		"HOR+ PRESERVES RETAIL VERTICAL FOV ON 16:9 / 21:9 DISPLAYS" );

	menu_button_text[MBT_OH_AUTOTUNE][0] = (char *)"GPU AUTO-TUNE";
	menu_button_text[MBT_OH_AUTOTUNE][1] = OH_Str( de,
		"AUTOMATISCHE OPTIMIERUNG FUER RASPBERRY PI 4/5 & SBC-GPUS",
		"AUTOMATIC MSAA / AF / BLOOM TUNING FOR RASPBERRY PI 4/5 & SBC GPUS" );

	menu_button_text[MBT_OH_HRTF][0] = (char *)"OPENAL 3D HRTF";
	menu_button_text[MBT_OH_HRTF][1] = OH_Str( de,
		"BINAURALER 3D-RAUMKLANG FUER KOPFHOERER (AUTO / AUS / AN)",
		"OPENAL SOFT BINAURAL 3D HEADPHONE SPATIALIZATION (AUTO / OFF / ON)" );
}

static void OpenHazard_ReadValues( ohInitialValues_t *v )
{
	int msaa = (int)ui.Cvar_VariableValue( "r_ext_multisample" );
	int fbo = (int)ui.Cvar_VariableValue( "r_fbo" );
	int bloom = (int)ui.Cvar_VariableValue( "r_bloom" );
	int bdown = (int)ui.Cvar_VariableValue( "r_bloom_downscale" );
	int swap = (int)ui.Cvar_VariableValue( "r_swapInterval" );
	int astc = (int)ui.Cvar_VariableValue( "r_ext_compressed_textures" );
	float lod = ui.Cvar_VariableValue( "r_mipLodBias" );
	float dl = ui.Cvar_VariableValue( "r_dlightScale" );
	int fov = (int)ui.Cvar_VariableValue( "cg_fovAspectAdjust" );
	int autotune = (int)ui.Cvar_VariableValue( "r_gpuAutoTune" );
	int hrtf = (int)ui.Cvar_VariableValue( "s_alHrtf" );

	if ( msaa >= 8 )      v->msaa = 3;
	else if ( msaa >= 4 ) v->msaa = 2;
	else if ( msaa >= 2 ) v->msaa = 1;
	else                  v->msaa = 0;

	v->fbo = fbo ? 1 : 0;

	if ( !bloom || !fbo ) {
		v->bloom = 0;
	} else if ( bdown == 2 ) {
		v->bloom = 2;
	} else {
		v->bloom = 1; // auto (quarter-res on Pi 5) or explicit 4
	}

	if ( swap >= 2 )      v->present = 2;
	else if ( swap == 1 ) v->present = 1;
	else                  v->present = 0;

	v->astc = astc ? 1 : 0;

	if ( lod <= -0.55f )      v->lodbias = 2;
	else if ( lod <= -0.15f ) v->lodbias = 1;
	else                      v->lodbias = 0;

	if ( dl >= 0.95f )        v->dlight = 2;
	else if ( dl >= 0.70f )   v->dlight = 1;
	else                      v->dlight = 0;

	v->fov = fov ? 1 : 0;
	v->autotune = autotune ? 1 : 0;

	if ( hrtf > 0 )           v->hrtf = 2;
	else if ( hrtf == 0 )     v->hrtf = 1;
	else                      v->hrtf = 0;
}

static void OpenHazard_SetControlsFromValues( const ohInitialValues_t *v )
{
	s_oh_msaa_list.curvalue     = v->msaa;
	s_oh_fbo_list.curvalue      = v->fbo;
	s_oh_bloom_list.curvalue    = v->bloom;
	s_oh_present_list.curvalue  = v->present;
	s_oh_astc_list.curvalue     = v->astc;
	s_oh_lodbias_list.curvalue  = v->lodbias;
	s_oh_dlight_list.curvalue   = v->dlight;
	s_oh_fov_list.curvalue      = v->fov;
	s_oh_autotune_list.curvalue = v->autotune;
	s_oh_hrtf_list.curvalue     = v->hrtf;
}

static void OpenHazard_UpdateApplyState( void )
{
	qboolean changed =
		( s_oh_msaa_list.curvalue     != s_oh_initial.msaa ) ||
		( s_oh_fbo_list.curvalue      != s_oh_initial.fbo ) ||
		( s_oh_bloom_list.curvalue    != s_oh_initial.bloom ) ||
		( s_oh_present_list.curvalue  != s_oh_initial.present ) ||
		( s_oh_astc_list.curvalue     != s_oh_initial.astc ) ||
		( s_oh_lodbias_list.curvalue  != s_oh_initial.lodbias ) ||
		( s_oh_dlight_list.curvalue   != s_oh_initial.dlight ) ||
		( s_oh_fov_list.curvalue      != s_oh_initial.fov ) ||
		( s_oh_autotune_list.curvalue != s_oh_initial.autotune ) ||
		( s_oh_hrtf_list.curvalue     != s_oh_initial.hrtf );

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

static void OpenHazard_ControlCallback( void *ptr, int notification )
{
	int id;
	if ( notification != QM_ACTIVATED )
		return;

	id = ((menucommon_s *)ptr)->id;

	// Keep Bloom and Render Path consistent: Bloom requires FBO, and switching
	// to Direct mode turns Bloom off cleanly.
	if ( id == ID_OH_BLOOM && s_oh_bloom_list.curvalue > 0 && s_oh_fbo_list.curvalue == 0 )
	{
		s_oh_fbo_list.curvalue = 1;
	}
	else if ( id == ID_OH_FBO && s_oh_fbo_list.curvalue == 0 && s_oh_bloom_list.curvalue > 0 )
	{
		s_oh_bloom_list.curvalue = 0;
	}

	OpenHazard_UpdateApplyState();
}

static void OpenHazard_ApplyChanges( void *unused, int notification )
{
	static const int   msaaVals[]    = { 0, 2, 4, 8 };
	static const float lodVals[]     = { 0.0f, -0.35f, -0.70f };
	static const float dlightVals[]  = { 0.5f, 0.9f, 1.0f };
	static const int   hrtfVals[]    = { -1, 0, 1 };

	qboolean needVidRestart = qfalse;
	qboolean needSndRestart = qfalse;

	if ( notification != QM_ACTIVATED )
		return;

	if ( s_oh_msaa_list.curvalue     != s_oh_initial.msaa ||
	     s_oh_fbo_list.curvalue      != s_oh_initial.fbo ||
	     s_oh_bloom_list.curvalue    != s_oh_initial.bloom ||
	     s_oh_present_list.curvalue  != s_oh_initial.present ||
	     s_oh_astc_list.curvalue     != s_oh_initial.astc ||
	     s_oh_lodbias_list.curvalue  != s_oh_initial.lodbias ||
	     s_oh_autotune_list.curvalue != s_oh_initial.autotune )
	{
		needVidRestart = qtrue;
	}

	if ( s_oh_hrtf_list.curvalue != s_oh_initial.hrtf )
	{
		needSndRestart = qtrue;
	}

	ui.Cvar_SetValue( "r_ext_multisample", (float)msaaVals[ s_oh_msaa_list.curvalue ] );
	ui.Cvar_SetValue( "r_fbo", (float)s_oh_fbo_list.curvalue );

	if ( s_oh_bloom_list.curvalue == 0 )
	{
		ui.Cvar_SetValue( "r_bloom", 0 );
	}
	else if ( s_oh_bloom_list.curvalue == 1 )
	{
		ui.Cvar_SetValue( "r_bloom", 1 );
		ui.Cvar_SetValue( "r_bloom_downscale", 4 );
	}
	else
	{
		ui.Cvar_SetValue( "r_bloom", 1 );
		ui.Cvar_SetValue( "r_bloom_downscale", 2 );
	}

	ui.Cvar_SetValue( "r_swapInterval", (float)s_oh_present_list.curvalue );
	ui.Cvar_SetValue( "r_ext_compressed_textures", (float)s_oh_astc_list.curvalue );
	ui.Cvar_SetValue( "r_mipLodBias", lodVals[ s_oh_lodbias_list.curvalue ] );
	ui.Cvar_SetValue( "r_dlightScale", dlightVals[ s_oh_dlight_list.curvalue ] );
	ui.Cvar_SetValue( "cg_fovAspectAdjust", (float)s_oh_fov_list.curvalue );
	ui.Cvar_SetValue( "r_gpuAutoTune", (float)s_oh_autotune_list.curvalue );
	ui.Cvar_SetValue( "s_alHrtf", (float)hrtfVals[ s_oh_hrtf_list.curvalue ] );

	s_oh_initial.msaa     = s_oh_msaa_list.curvalue;
	s_oh_initial.fbo      = s_oh_fbo_list.curvalue;
	s_oh_initial.bloom    = s_oh_bloom_list.curvalue;
	s_oh_initial.present  = s_oh_present_list.curvalue;
	s_oh_initial.astc     = s_oh_astc_list.curvalue;
	s_oh_initial.lodbias  = s_oh_lodbias_list.curvalue;
	s_oh_initial.dlight   = s_oh_dlight_list.curvalue;
	s_oh_initial.fov      = s_oh_fov_list.curvalue;
	s_oh_initial.autotune = s_oh_autotune_list.curvalue;
	s_oh_initial.hrtf     = s_oh_hrtf_list.curvalue;

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

	// Left decorative LCARS column bar
	ui.R_SetColor( colorTable[CT_DKPURPLE2] );
	UI_DrawHandlePic( 30, 203, 47, 186, uis.whiteShader );
	UI_DrawProportionalString( 74, 206, "250624", UI_RIGHT | UI_TINYFONT, colorTable[CT_BLACK] );

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

	UI_DrawProportionalString( 606, 193, "VULKAN 1.3", UI_RIGHT | UI_TINYFONT, colorTable[CT_BLACK] );
	UI_DrawProportionalString( 606, 390, "ASTC / MSAA", UI_RIGHT | UI_TINYFONT, colorTable[CT_BLACK] );

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

static void OpenHazard_MenuInit( void )
{
	const qboolean de = OpenHazard_IsGerman();
	int x = 170;
	int y = 176;
	int inc = 23;
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

	OpenHazard_InitSpin( &s_oh_msaa_list,     x, y, width, ID_OH_MSAA,     MBT_OH_MSAA,       de ? s_oh_msaa_de     : s_oh_msaa_en );     y += inc;
	OpenHazard_InitSpin( &s_oh_fbo_list,      x, y, width, ID_OH_FBO,      MBT_OH_RENDERPATH, de ? s_oh_fbo_de      : s_oh_fbo_en );      y += inc;
	OpenHazard_InitSpin( &s_oh_bloom_list,    x, y, width, ID_OH_BLOOM,    MBT_OH_BLOOM,      de ? s_oh_bloom_de    : s_oh_bloom_en );    y += inc;
	OpenHazard_InitSpin( &s_oh_present_list,  x, y, width, ID_OH_PRESENT,  MBT_OH_PRESENT,    de ? s_oh_present_de  : s_oh_present_en );  y += inc;
	OpenHazard_InitSpin( &s_oh_astc_list,     x, y, width, ID_OH_ASTC,     MBT_OH_ASTC,       de ? s_oh_astc_de     : s_oh_astc_en );     y += inc;
	OpenHazard_InitSpin( &s_oh_lodbias_list,  x, y, width, ID_OH_LODBIAS,  MBT_OH_LODBIAS,    de ? s_oh_lod_de      : s_oh_lod_en );      y += inc;
	OpenHazard_InitSpin( &s_oh_dlight_list,   x, y, width, ID_OH_DLIGHT,   MBT_OH_DLIGHT,     de ? s_oh_dlight_de   : s_oh_dlight_en );   y += inc;
	OpenHazard_InitSpin( &s_oh_fov_list,      x, y, width, ID_OH_FOV,      MBT_OH_FOV,        de ? s_oh_fov_de      : s_oh_fov_en );      y += inc;
	OpenHazard_InitSpin( &s_oh_autotune_list, x, y, width, ID_OH_AUTOTUNE, MBT_OH_AUTOTUNE,   de ? s_oh_auto_de     : s_oh_auto_en );     y += inc;
	OpenHazard_InitSpin( &s_oh_hrtf_list,     x, y, width, ID_OH_HRTF,     MBT_OH_HRTF,       de ? s_oh_hrtf_de     : s_oh_hrtf_en );

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

	Menu_AddItem( &s_oh_menu, (void *)&s_oh_msaa_list );
	Menu_AddItem( &s_oh_menu, (void *)&s_oh_fbo_list );
	Menu_AddItem( &s_oh_menu, (void *)&s_oh_bloom_list );
	Menu_AddItem( &s_oh_menu, (void *)&s_oh_present_list );
	Menu_AddItem( &s_oh_menu, (void *)&s_oh_astc_list );
	Menu_AddItem( &s_oh_menu, (void *)&s_oh_lodbias_list );
	Menu_AddItem( &s_oh_menu, (void *)&s_oh_dlight_list );
	Menu_AddItem( &s_oh_menu, (void *)&s_oh_fov_list );
	Menu_AddItem( &s_oh_menu, (void *)&s_oh_autotune_list );
	Menu_AddItem( &s_oh_menu, (void *)&s_oh_hrtf_list );
	Menu_AddItem( &s_oh_menu, (void *)&s_oh_apply_action );

	OpenHazard_ReadValues( &s_oh_initial );
	OpenHazard_SetControlsFromValues( &s_oh_initial );
	OpenHazard_UpdateApplyState();
}

void UI_OpenHazardMenu( void )
{
	OpenHazard_MenuInit();
	UI_PushMenu( &s_oh_menu );
}
