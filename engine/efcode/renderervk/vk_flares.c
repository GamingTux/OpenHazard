/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_flares.c

#include "tr_local.h"

/*
=============================================================================

LIGHT FLARES

A light flare is an effect that takes place inside the eye when bright light
sources are visible.  The size of the flare relative to the screen is nearly
constant, irrespective of distance, but the intensity should be proportional to the
projected area of the light source.

A surface that has been flagged as having a light flare will calculate the depth
buffer value that its midpoint should have when the surface is added.

After all opaque surfaces have been rendered, the depth buffer is read back for
each flare in view.  If the point has not been obscured by a closer surface, the
flare should be drawn.

Surfaces that have a repeated texture should never be flagged as flaring, because
there will only be a single flare added at the midpoint of the polygon.

To prevent abrupt popping, the intensity of the flare is interpolated up and
down as it changes visibility.  This involves scene to scene state, unlike almost
all other aspects of the renderer, and is complicated by the fact that a single
frame may have multiple scenes.

RB_RenderFlares() will be called once per view (twice in a mirrored scene, potentially
up to five or more times in a frame with 3D status bar icons).

=============================================================================
*/


// flare states maintain visibility over multiple frames for fading
// layers: view, mirror, menu
typedef struct flare_s {
	struct		flare_s	*next;		// for active chain

	int			addedFrame;
	uint32_t	testCount;

	portalView_t portalView;
	int			frameSceneNum;
	void		*surface;
	int			fogNum;

	int			fadeTime;

	qboolean	visible;			// state of last test
	float		drawIntensity;		// may be non 0 even if !visible due to fading

	int			windowX, windowY;
	float		eyeZ;
	float		drawZ;

	vec3_t		origin;
	vec3_t		color;
} flare_t;

static flare_t	r_flareStructs[ MAX_FLARES ];
static flare_t	*r_activeFlares, *r_inactiveFlares;


/*
==================
R_ClearFlares
==================
*/
void R_ClearFlares( void ) {
	int		i;

	if ( !vk.fragmentStores )
		return;

	Com_Memset( r_flareStructs, 0, sizeof( r_flareStructs ) );
	r_activeFlares = NULL;
	r_inactiveFlares = NULL;

	for ( i = 0 ; i < MAX_FLARES ; i++ ) {
		r_flareStructs[i].next = r_inactiveFlares;
		r_inactiveFlares = &r_flareStructs[i];
	}
}


static flare_t *R_SearchFlare( void *surface )
{
	flare_t *f;

	// see if a flare with a matching surface, scene, and view exists
	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->surface == surface && f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView ) {
			return f;
		}
	}

	return NULL;
}


/*
==================
RB_AddFlare

This is called at surface tesselation time
==================
*/
void RB_AddFlare( void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal ) {
	int				i;
	flare_t			*f;
	vec3_t			local;
	float			d = 1;
	vec4_t			eye, clip, normalized, window;

	backEnd.pc.c_flareAdds++;

	if ( normal && (normal[0] || normal[1] || normal[2] ) )	{
		VectorSubtract( backEnd.viewParms.or.origin, point, local );
		VectorNormalizeFast( local );
		d = DotProduct( local, normal );
		// If the viewer is behind the flare don't add it.
		if ( d < 0 ) {
			return;
		}
	}

	// if the point is off the screen, don't bother adding it
	// calculate screen coordinates and depth
	R_TransformModelToClip( point, backEnd.or.modelMatrix, backEnd.viewParms.projectionMatrix, eye, clip );

	// check to see if the point is completely off screen
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( clip[i] >= clip[3] || clip[i] <= -clip[3] ) {
			return;
		}
	}

	R_TransformClipToWindow( clip, &backEnd.viewParms, normalized, window );

	if ( window[0] < 0 || window[0] >= backEnd.viewParms.viewportWidth || window[1] < 0 || window[1] >= backEnd.viewParms.viewportHeight ) {
		return;	// shouldn't happen, since we check the clip[] above, except for FP rounding
	}

	f = R_SearchFlare( surface );

	// allocate a new one
	if ( !f ) {
		if ( !r_inactiveFlares ) {
			// the list is completely full
			return;
		}
		f = r_inactiveFlares;
		r_inactiveFlares = r_inactiveFlares->next;
		f->next = r_activeFlares;
		r_activeFlares = f;

		f->surface = surface;
		f->frameSceneNum = backEnd.viewParms.frameSceneNum;
		f->portalView = backEnd.viewParms.portalView;
		f->visible = qfalse;
		f->fadeTime = backEnd.refdef.time - 2000;
		f->testCount = 0;
	} else {
		++f->testCount;
	}

	f->addedFrame = backEnd.viewParms.frameCount;
	f->fogNum = fogNum;

	VectorCopy( point, f->origin );
	VectorCopy( color, f->color );

	// fade the intensity of the flare down as the
	// light surface turns away from the viewer
	VectorScale( f->color, d, f->color );

	// save info needed to test
	f->windowX = backEnd.viewParms.viewportX + window[0];
	f->windowY = backEnd.viewParms.viewportY + window[1];

	f->eyeZ = eye[2];

#ifdef USE_REVERSED_DEPTH
	f->drawZ = (clip[2]+0.20) / clip[3];
#else
	f->drawZ = (clip[2]-0.20) / clip[3];
#endif

}


/*
==================
RB_AddDlightFlares
==================
*/
void RB_AddDlightFlares( void ) {
	dlight_t		*l;
	int				i, j, k;
	fog_t			*fog = NULL;

	if ( !r_flares->integer ) {
		return;
	}
	if ( !r_dlightFlares || !r_dlightFlares->integer || ( r_ohClassic && r_ohClassic->integer ) ) {
		return;
	}

	l = backEnd.refdef.dlights;

	if ( tr.world )
		fog = tr.world->fogs;

	for ( i = 0 ; i < backEnd.refdef.num_dlights; i++, l++ ) {
		vec3_t delta, scaledColor;
		float dist, camFade, radScale;

		if ( l->radius < 20.0f ) {
			continue;
		}

		// Fade out very close dynamic lights (e.g. player's own muzzle flash) so they never obscure the crosshair
		VectorSubtract( l->origin, backEnd.viewParms.or.origin, delta );
		dist = VectorLength( delta );
		if ( dist < 32.0f ) {
			continue;
		}
		camFade = ( dist < 72.0f ) ? ( ( dist - 32.0f ) * ( 1.0f / 40.0f ) ) : 1.0f;

		radScale = l->radius * ( 1.0f / 260.0f );
		if ( radScale < 0.25f ) radScale = 0.25f;
		else if ( radScale > 1.0f ) radScale = 1.0f;

		scaledColor[0] = l->color[0] * ( 0.45f * radScale * camFade );
		scaledColor[1] = l->color[1] * ( 0.45f * radScale * camFade );
		scaledColor[2] = l->color[2] * ( 0.45f * radScale * camFade );
		if ( scaledColor[0] > 0.72f ) scaledColor[0] = 0.72f;
		if ( scaledColor[1] > 0.72f ) scaledColor[1] = 0.72f;
		if ( scaledColor[2] > 0.72f ) scaledColor[2] = 0.72f;
		if ( scaledColor[0] + scaledColor[1] + scaledColor[2] < 0.04f ) {
			continue;
		}

		if ( fog )
		{
			// find which fog volume the light is in
			for ( j = 1 ; j < tr.world->numfogs ; j++ ) {
				fog = &tr.world->fogs[j];
				for ( k = 0 ; k < 3 ; k++ ) {
					if ( l->origin[k] < fog->bounds[0][k] || l->origin[k] > fog->bounds[1][k] ) {
						break;
					}
				}
				if ( k == 3 ) {
					break;
				}
			}
			if ( j == tr.world->numfogs ) {
				j = 0;
			}
		}
		else
			j = 0;

		RB_AddFlare( (void *)l, j, l->origin, scaledColor, NULL );
	}
}

/*
===============================================================================

FLARE BACK END

===============================================================================
*/


static float *vk_ortho( float x1, float x2,
						float y2, float y1,
						float z1, float z2 ) {

	static float m[16] = { 0 };

	m[0] = 2.0f / (x2 - x1);
	m[5] = 2.0f / (y2 - y1);
	m[10] = 1.0f / (z1 - z2);
	m[12] = -(x2 + x1) / (x2 - x1);
	m[13] = -(y2 + y1) / (y2 - y1);
	m[14] = z1 / (z1 - z2);
	m[15] = 1.0f;

	return m;
}


/*
==================
RB_TestFlare
==================
*/
static void RB_TestFlare( flare_t *f ) {
	qboolean		visible;
	float			fade;
	float			*m;
	uint32_t		offset;
	int				i;

	backEnd.pc.c_flareTests++;

/*
	We don't have equivalent of glReadPixels() in vulkan
	and explicit depth buffer reading may be very slow and require surface conversion.

	So we will use storage buffer and exploit early depth tests by
	rendering test dot in orthographic projection at projected flare coordinates
	window-x, window-y and world-z: if test dot is not covered by
	any world geometry - it will invoke fragment shader which will
	fill storage buffer at desired location, then we discard fragment.
	In next frame we read storage buffer: if there is a non-zero value
	then our flare WAS visible (as we're working with 1-frame delay),
	multisampled image will cause multiple fragment shader invocations.
*/

	// we neeed only single uint32_t but take care of alignment
	offset = (f - r_flareStructs) * vk.storage_alignment;

	if ( f->testCount ) {
		uint32_t *cnt = (uint32_t*)(vk.storage.buffer_ptr + offset);
		if ( *cnt )
			visible = qtrue;
		else
			visible = qfalse;

		f->testCount = 1;
	} else {
		visible = qfalse;
	}

	// reset test result in storage buffer
	// *((uint32_t*)(vk.storage.buffer_ptr + offset)) = 0x00;

	m = vk_ortho( backEnd.viewParms.viewportX, backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
		backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, 0, 1 );
	vk_update_mvp( m );

	tess.xyz[0][0] = f->windowX;
	tess.xyz[0][1] = f->windowY;
	tess.xyz[0][2] = -f->drawZ;
	tess.numVertexes = 1;

#ifdef USE_VBO
	tess.vboIndex = 0;
#endif
	// invalidate descriptors
	for ( i = 0; i < VK_DESC_COUNT; i++ ) {
		vk_reset_descriptor( i );
	}
	// render test dot
	vk_bind_pipeline( vk.dot_pipeline );
	vk_bind_geometry( TESS_XYZ );
	vk_draw_dot( offset );

	//Com_Memcpy( vk_world.modelview_transform, modelMatrix_original, sizeof( modelMatrix_original ) );
	//vk_update_mvp( NULL );

	if ( visible ) {
		if ( !f->visible ) {
			f->visible = qtrue;
			f->fadeTime = backEnd.refdef.time - 1;
		}
		fade = ( ( backEnd.refdef.time - f->fadeTime ) /1000.0f ) * r_flareFade->value;
	} else {
		if ( f->visible ) {
			f->visible = qfalse;
			f->fadeTime = backEnd.refdef.time - 1;
		}
		fade = 1.0f - ( ( backEnd.refdef.time - f->fadeTime ) / 1000.0f ) * r_flareFade->value;
	}

	if ( fade < 0 ) {
		fade = 0;
	} else if ( fade > 1 ) {
		fade = 1;
	}

	f->drawIntensity = fade;
}


/*
==================
RB_RenderFlare
==================
*/
static void RB_RenderFlare( flare_t *f ) {
	float			size;
	vec3_t			color;
	float distance, intensity, factor;
	byte fogFactors[3] = {255, 255, 255};
	color4ub_t		c;

	//if ( f->drawIntensity == 0.0 )
	//	return;

	backEnd.pc.c_flareRenders++;

	// We don't want too big values anyways when dividing by distance.
	if ( f->eyeZ > -1.0f )
		distance = 1.0f;
	else
		distance = -f->eyeZ;

	// calculate the flare size..
	size = backEnd.viewParms.viewportWidth * ( r_flareSize->value/640.0f + 8 / distance );

/*
 * This is an alternative to intensity scaling. It changes the size of the flare on screen instead
 * with growing distance. See in the description at the top why this is not the way to go.
	// size will change ~ 1/r.
	size = backEnd.viewParms.viewportWidth * (r_flareSize->value / (distance * -2.0f));
*/

/*
 * As flare sizes stay nearly constant with increasing distance we must decrease the intensity
 * to achieve a reasonable visual result. The intensity is ~ (size^2 / distance^2) which can be
 * got by considering the ratio of
 * (flaresurface on screen) : (Surface of sphere defined by flare origin and distance from flare)
 * An important requirement is:
 * intensity <= 1 for all distances.
 *
 * The formula used here to compute the intensity is as follows:
 * intensity = flareCoeff * size^2 / (distance + size*sqrt(flareCoeff))^2
 * As you can see, the intensity will have a max. of 1 when the distance is 0.
 * The coefficient flareCoeff will determine the falloff speed with increasing distance.
 */

	factor = distance + size * sqrt( r_flareCoeff->value );

	intensity = r_flareCoeff->value * size * size / ( factor * factor );

	VectorScale( f->color, f->drawIntensity * intensity, color );

	// Calculations for fogging
	if ( tr.world && f->fogNum > 0 && f->fogNum < tr.world->numfogs )
	{
		tess.numVertexes = 1;
		VectorCopy( f->origin, tess.xyz[0] );
		tess.fogNum = f->fogNum;

		RB_CalcModulateColorsByFog( fogFactors );

		// We don't need to render the flare if colors are 0 anyways.
		if ( !(fogFactors[0] || fogFactors[1] || fogFactors[2]) )
			return;
	}

	RB_BeginSurface( tr.flareShader, f->fogNum );

	c.rgba[0] = color[0] * fogFactors[0];
	c.rgba[1] = color[1] * fogFactors[1];
	c.rgba[2] = color[2] * fogFactors[2];
	c.rgba[3] = 255;

	RB_AddQuadStamp2( f->windowX - size, f->windowY - size, size * 2, size * 2, 0, 0, 1, 1, c );

	RB_EndSurface();
}


/*
==================
RB_RenderFlares

Because flares are simulating an occular effect, they should be drawn after
everything (all views) in the entire frame has been drawn.

Because of the way portals use the depth buffer to mark off areas, the
needed information would be lost after each view, so we are forced to draw
flares after each view.

The resulting artifact is that flares in mirrors or portals don't dim properly
when occluded by something in the main view, and portal flares that should
extend past the portal edge will be overwritten.
==================
*/
/*
==================
RB_RenderBloomHalos

Zero-FBO Fake-Bloom + dynamic light coronas rendered in ONE SINGLE batched world-space
hardware depth-tested draw call using the procedural 2-lobe Gaussian glow shader:
- Static BSP light fixtures (tr.world->bloomHalos): PVS-culled, normal-faded, distance-faded
  (auto-disabled when true FBO r_bloom 1 is active so it never double-blooms).
- Dynamic lights (backEnd.refdef.dlights): phasers, explosions, consoles, Borg energy.
Zero vk.dot_pipeline SSBO stalls, zero 1-frame lag, true Z-occlusion against walls/doors.
==================
*/
static void RB_RenderBloomHalos( void ) {
	const dlight_t *l;
	int i, drawn = 0;
	qboolean doStatic, doDynamic;
	float fakeIntensity;

	if ( r_ohClassic && r_ohClassic->integer ) {
		return;
	}
	if ( !tr.bloomHaloShader ) {
		return;
	}
	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP || backEnd.isHyperspace ) {
		return;
	}

	fakeIntensity = r_fakeBloomIntensity ? Com_Clamp( 0.0f, 1.5f, r_fakeBloomIntensity->value ) : 0.55f;
	doStatic = ( r_fakeBloom && r_fakeBloom->integer && fakeIntensity > 0.01f && !vk.bloomActive &&
		tr.world && tr.world->bloomHalos && tr.world->numBloomHalos > 0 ) ? qtrue : qfalse;
	doDynamic = ( r_flares->integer && r_dlightFlares && r_dlightFlares->integer &&
		backEnd.refdef.num_dlights > 0 ) ? qtrue : qfalse;

	if ( !doStatic && !doDynamic ) {
		return;
	}

	backEnd.currentEntity = &tr.worldEntity;
	backEnd.or = backEnd.viewParms.world;

	// Restore world-space perspective MVP (RB_ShadowFinish may have bound ortho)
	Com_Memcpy( vk_world.modelview_transform, backEnd.viewParms.world.modelMatrix, sizeof( vk_world.modelview_transform ) );
	vk_update_mvp( NULL );

	// 1. Static map light fixture soft Gaussian bloom halos (PVS + frustum + normal culled)
	if ( doStatic ) {
		const byte *pvs = ( tr.world->vis && tr.viewCluster >= 0 )
			? ( tr.world->vis + tr.viewCluster * tr.world->clusterBytes ) : NULL;
		const worldBloomHalo_t *halos = tr.world->bloomHalos;
		int numHalos = tr.world->numBloomHalos;

		for ( i = 0; i < numHalos; i++ ) {
			const worldBloomHalo_t *h = &halos[i];
			vec3_t toCam, center, left, up;
			float dist, invDist, fwdDot, facing, distFade, scale, nudge, size;
			color4ub_t c;
			int r, g, b;

			if ( pvs && h->cluster >= 0 && !( pvs[ h->cluster >> 3 ] & ( 1 << ( h->cluster & 7 ) ) ) ) {
				continue;
			}

			VectorSubtract( backEnd.viewParms.or.origin, h->origin, toCam );
			dist = VectorLength( toCam );
			if ( dist < 18.0f || dist > 1350.0f ) {
				continue;
			}
			invDist = 1.0f / dist;

			// View-cone check (toCam points from light -> eye, so negative dot with forward means in front)
			fwdDot = -DotProduct( toCam, backEnd.viewParms.or.axis[0] ) * invDist;
			if ( fwdDot < 0.25f ) {
				continue;
			}

			// Smoothly fade lights facing away from the viewer (allow shallow hallway ceiling angles to glow!)
			if ( h->normal[0] || h->normal[1] || h->normal[2] ) {
				float ndot = DotProduct( h->normal, toCam ) * invDist;
				if ( ndot <= -0.25f ) {
					continue;
				}
				facing = Com_Clamp( 0.0f, 1.0f, ndot * 1.25f + 0.35f );
			} else {
				facing = 1.0f;
			}

			// Smooth near + far distance attenuation
			distFade = 1.0f;
			if ( dist < 56.0f ) {
				distFade = ( dist - 18.0f ) * ( 1.0f / 38.0f );
			} else if ( dist > 750.0f ) {
				distFade = ( 1350.0f - dist ) * ( 1.0f / 600.0f );
			}

			scale = 0.78f * fakeIntensity * h->intensity * facing * distFade;
			r = (int)( h->color[0] * scale * 255.0f );
			g = (int)( h->color[1] * scale * 255.0f );
			b = (int)( h->color[2] * scale * 255.0f );
			if ( r > 215 ) r = 215;
			if ( g > 215 ) g = 215;
			if ( b > 215 ) b = 215;
			if ( r + g + b < 10 ) {
				continue;
			}

			if ( !drawn ) {
				RB_BeginSurface( tr.bloomHaloShader, 0 );
				drawn = 1;
			}

			// Clamp angular billboard size so overhead lamps never overdraw the entire 1080p screen
			size = MIN( h->radius, dist * 0.32f );

			// Gentle pull toward camera wraps the soft glow over the bezel while keeping true hardware Z-occlusion
			nudge = MIN( size * 0.38f, dist * 0.22f );
			center[0] = h->origin[0] + toCam[0] * ( invDist * nudge );
			center[1] = h->origin[1] + toCam[1] * ( invDist * nudge );
			center[2] = h->origin[2] + toCam[2] * ( invDist * nudge );

			VectorScale( backEnd.viewParms.or.axis[1], size, left );
			VectorScale( backEnd.viewParms.or.axis[2], size, up );

			c.rgba[0] = (byte)r;
			c.rgba[1] = (byte)g;
			c.rgba[2] = (byte)b;
			c.rgba[3] = 255;

			RB_AddQuadStamp( center, left, up, c );
		}
	}

	// 2. Dynamic light soft Gaussian bloom halos (phasers, explosions, Borg energy, consoles)
	if ( doDynamic ) {
		l = backEnd.refdef.dlights;
		for ( i = 0; i < backEnd.refdef.num_dlights; i++, l++ ) {
			vec3_t toCam, center, left, up;
			float dist, invDist, camFade, radScale, size, scale, nudge;
			color4ub_t c;
			int r, g, b;

			if ( l->radius < 24.0f ) {
				continue;
			}

			VectorSubtract( backEnd.viewParms.or.origin, l->origin, toCam );
			dist = VectorLength( toCam );
			if ( dist < 36.0f ) {
				continue; // never blind the crosshair on player's own muzzle flash
			}
			if ( DotProduct( toCam, backEnd.viewParms.or.axis[0] ) >= 0.0f ) {
				continue;
			}

			camFade = ( dist < 84.0f ) ? ( ( dist - 36.0f ) * ( 1.0f / 48.0f ) ) : 1.0f;
			radScale = l->radius * ( 1.0f / 260.0f );
			if ( radScale < 0.28f ) radScale = 0.28f;
			else if ( radScale > 1.0f ) radScale = 1.0f;

			scale = 0.40f * radScale * camFade;
			r = (int)( l->color[0] * scale * 255.0f );
			g = (int)( l->color[1] * scale * 255.0f );
			b = (int)( l->color[2] * scale * 255.0f );
			if ( r > 185 ) r = 185;
			if ( g > 185 ) g = 185;
			if ( b > 185 ) b = 185;
			if ( r + g + b < 10 ) {
				continue;
			}

			if ( !drawn ) {
				RB_BeginSurface( tr.bloomHaloShader, 0 );
				drawn = 1;
			}

			invDist = 1.0f / dist;
			nudge = ( dist * 0.22f < 14.0f ) ? ( dist * 0.22f ) : 14.0f;
			center[0] = l->origin[0] + toCam[0] * ( invDist * nudge );
			center[1] = l->origin[1] + toCam[1] * ( invDist * nudge );
			center[2] = l->origin[2] + toCam[2] * ( invDist * nudge );

			size = l->radius * 0.16f;
			if ( size < 14.0f ) size = 14.0f;
			else if ( size > 42.0f ) size = 42.0f;

			VectorScale( backEnd.viewParms.or.axis[1], size, left );
			VectorScale( backEnd.viewParms.or.axis[2], size, up );

			c.rgba[0] = (byte)r;
			c.rgba[1] = (byte)g;
			c.rgba[2] = (byte)b;
			c.rgba[3] = 255;

			RB_AddQuadStamp( center, left, up, c );
		}
	}

	if ( drawn ) {
		RB_EndSurface();
	}
}


void RB_RenderFlares( void ) {
	flare_t		*f;
	flare_t		**prev;
	qboolean	draw;
	float		*m;

	// Render batched hardware Z-tested zero-FBO Fake-Bloom & dynamic light halos first
	RB_RenderBloomHalos();

	if ( !r_flares->integer ) {
		return;
	}

	// Flare visibility is tested by drawing a point through vk.dot_pipeline,
	// which writes a storage-buffer flag through a fragment shader (there is no
	// depth readback in Vulkan; see RB_TestFlare). Both the pipeline and the
	// storage-buffer layout only exist when the device advertises
	// fragmentStoresAndAtomics -- R_ClearFlares() already uses exactly this test,
	// and vk.dot_pipeline is only created under it (vk.c, "flare visibility test
	// dot"). Without this guard RB_TestFlare() would bind the default pipeline
	// and then bind vk.storage.descriptor against vk.pipeline_layout_storage,
	// a descriptor set that is not compatible with that pipeline's layout: an
	// invalid-usage path on a device where flares can never be visible anyway.
	if ( !vk.fragmentStores ) {
		return;
	}

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP ) {
		return;
	}

	if ( backEnd.isHyperspace ) {
		return;
	}

	// Reset currentEntity to world so that any previously referenced entities
	// don't have influence on the rendering of these flares (i.e. RF_ renderer flags).
	backEnd.currentEntity = &tr.worldEntity;
	backEnd.or = backEnd.viewParms.world;

	// Dynamic light halos are now rendered in a single depth-tested batch by RB_RenderDlightCoronas() above.

	// perform z buffer readback on each flare in this view
	draw = qfalse;
	prev = &r_activeFlares;
	while ( ( f = *prev ) != NULL ) {
		// throw out any flares that weren't added last frame
		if ( backEnd.viewParms.frameCount - f->addedFrame > 0 && f->portalView == backEnd.viewParms.portalView ) {
			*prev = f->next;
			f->next = r_inactiveFlares;
			r_inactiveFlares = f;
			continue;
		}

		// don't draw any here that aren't from this scene / portal
		f->drawIntensity = 0;
		if ( f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView ) {
			RB_TestFlare( f );
			if ( f->testCount == 0 ) {
				// recently added, wait 1 frame for test result
			} else if ( f->drawIntensity ) {
				draw = qtrue;
			} else {
				// this flare has completely faded out, so remove it from the chain
				*prev = f->next;
				f->next = r_inactiveFlares;
				r_inactiveFlares = f;
				continue;
			}
		}

		prev = &f->next;
	}

	if ( !draw ) {
		return;		// none visible
	}

#ifdef USE_REVERSED_DEPTH
	m = vk_ortho( backEnd.viewParms.viewportX, backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
		backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, 1.0, 0.0 );
#else
	m = vk_ortho( backEnd.viewParms.viewportX, backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
		backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, 0.0, 1.0 );
#endif

	vk_update_mvp( m );

	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView && f->drawIntensity ) {
			RB_RenderFlare( f );
		}
	}

	//Com_Memcpy( vk_world.modelview_transform, modelMatrix_original, sizeof( modelMatrix_original ) );
	//vk_update_mvp( NULL );
}
