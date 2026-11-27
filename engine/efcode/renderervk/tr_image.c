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
// tr_image.c
#include "tr_local.h"

static byte			 s_intensitytable[256];
static unsigned char s_gammatable[256];

#ifdef USE_VULKAN
static unsigned char s_gammatable_linear[256];
#endif

GLint	gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
GLint	gl_filter_max = GL_LINEAR;

#define FILE_HASH_SIZE		1024
static	image_t*		hashTable[FILE_HASH_SIZE];

/*
================
return a hash value for the filename
================
*/
#define generateHashValue(fname) Com_GenerateHashValue((fname),FILE_HASH_SIZE)


/*
** R_GammaCorrect
*/
void R_GammaCorrect( byte *buffer, int bufSize ) {
	int i;
#ifdef USE_VULKAN
	if ( vk.capture.image != VK_NULL_HANDLE )
		return;
	if ( !gls.deviceSupportsGamma )
		return;
#endif
	for ( i = 0; i < bufSize; i++ ) {
		buffer[i] = s_gammatable[buffer[i]];
	}
}

typedef struct {
	const char *name;
	GLint minimize, maximize;
} textureMode_t;

static const textureMode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode( const char *string ) {
	const textureMode_t *mode;
	image_t	*img;
	int		i;

	mode = NULL;
	for ( i = 0 ; i < ARRAY_LEN( modes ) ; i++ ) {
		if ( !Q_stricmp( modes[i].name, string ) ) {
			mode = &modes[i];
			break;
		}
	}

	if ( mode == NULL ) {
		ri.Printf( PRINT_ALL, "bad texture filter name '%s'\n", string );
		return;
	}

	gl_filter_min = mode->minimize;
	gl_filter_max = mode->maximize;

#ifdef USE_VULKAN
	if ( gl_filter_min == vk.samplers.filter_min && gl_filter_max == vk.samplers.filter_max ) {
		return;
	}
	vk_wait_idle();
	vk_destroy_samplers();

	vk.samplers.filter_min = gl_filter_min;
	vk.samplers.filter_max = gl_filter_max;
	vk_update_attachment_descriptors();
	for ( i = 0; i < tr.numImages; i++ ) {
		img = tr.images[i];
		if ( img->flags & IMGFLAG_MIPMAP ) {
			vk_update_descriptor_set( img, qtrue );
		}
	}
#else
	// hack to prevent trilinear from being set on voodoo,
	// because their driver freaks...
	if ( glConfig.hardwareType == GLHW_3DFX_2D3D && gl_filter_max == GL_LINEAR &&
		gl_filter_min == GL_LINEAR_MIPMAP_LINEAR ) {
		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
		ri.Printf( PRINT_ALL, "Refusing to set trilinear on a voodoo.\n" );
	}

	// change all the existing mipmap texture objects
	for ( i = 0; i < tr.numImages; i++ ) {
		img = tr.images[ i ];
		if ( img->flags & IMGFLAG_MIPMAP ) {
			GL_Bind( img );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		}
	}
#endif
}


/*
===============
R_SumOfUsedImages
===============
*/
int R_SumOfUsedImages( void ) {
	const image_t *img;
	int i, total = 0;

	for ( i = 0; i < tr.numImages; i++ ) {
		img = tr.images[ i ];
		if ( img->frameUsed == tr.frameCount ) {
			total += img->uploadWidth * img->uploadHeight;
		}
	}

	return total;
}


/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f( void ) {
	const image_t *image;
	int i, estTotalSize = 0;
	char *name, buf[MAX_QPATH*2 + 5];

	ri.Printf( PRINT_ALL, "\n -n- --w-- --h-- type  -size- --name-------\n" );

	for ( i = 0; i < tr.numImages; i++ )
	{
		const char *format = "???? ";
		const char *sizeSuffix;
		int estSize;
		int displaySize;

		image = tr.images[ i ];
		estSize = image->uploadHeight * image->uploadWidth;

		switch ( image->internalFormat )
		{
#ifdef USE_VULKAN
			case VK_FORMAT_B8G8R8A8_UNORM:
				format = "BGRA ";
				estSize *= 4;
				break;
			case VK_FORMAT_R8G8B8A8_UNORM:
				format = "RGBA ";
				estSize *= 4;
				break;
			case VK_FORMAT_R8G8B8_UNORM:
				format = "RGB  ";
				estSize *= 3;
				break;
			case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
				format = "RGBA ";
				estSize *= 2;
				break;
			case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
				format = "RGB  ";
				estSize *= 2;
				break;
			case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
				format = "ASTC4";
				estSize = ( ( image->uploadWidth + 3 ) / 4 ) * ( ( image->uploadHeight + 3 ) / 4 ) * 16;
				break;
			case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
				format = "ASTC5";
				estSize = ( ( image->uploadWidth + 4 ) / 5 ) * ( ( image->uploadHeight + 4 ) / 5 ) * 16;
				break;
			case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
				format = "ASTC6";
				estSize = ( ( image->uploadWidth + 5 ) / 6 ) * ( ( image->uploadHeight + 5 ) / 6 ) * 16;
				break;
			case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
				format = "ASTC8";
				estSize = ( ( image->uploadWidth + 7 ) / 8 ) * ( ( image->uploadHeight + 7 ) / 8 ) * 16;
				break;
#else
			case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
				format = "DXT1 ";
				// 64 bits per 16 pixels, so 4 bits per pixel
				estSize /= 2;
				break;
			case GL_RGB4_S3TC:
				format = "S3TC ";
				// same as DXT1?
				estSize /= 2;
				break;
			case GL_RGBA4:
			case GL_RGBA8:
			case GL_RGBA:
				format = "RGBA ";
				// 4 bytes per pixel
				estSize *= 4;
				break;
			case GL_RGB5:
			case GL_RGB8:
			case GL_RGB:
				format = "RGB  ";
				// 3 bytes per pixel?
				estSize *= 3;
				break;
#endif
		}

		// mipmap adds about 50%
		if (image->flags & IMGFLAG_MIPMAP)
			estSize += estSize / 2;

		sizeSuffix = "b ";
		displaySize = estSize;

		if ( displaySize >= 2048 )
		{
			displaySize = ( displaySize + 1023 ) / 1024;
			sizeSuffix = "kb";
		}

		if ( displaySize >= 2048 )
		{
			displaySize = ( displaySize + 1023 ) / 1024;
			sizeSuffix = "Mb";
		}

		if ( displaySize >= 2048 )
		{
			displaySize = ( displaySize + 1023 ) / 1024;
			sizeSuffix = "Gb";
		}

		if ( Q_stricmp( image->imgName, image->imgName2 ) == 0 ) {
			name = image->imgName;
		} else {
			Com_sprintf( buf, sizeof( buf ), "%s => " S_COLOR_YELLOW "%s",
				image->imgName, image->imgName2 );
			name = buf;
		}

		ri.Printf( PRINT_ALL, " %3i %5i %5i %s %4i%s %s\n", i, image->uploadWidth, image->uploadHeight, format, displaySize, sizeSuffix, name );
		estTotalSize += estSize;
	}

	ri.Printf( PRINT_ALL, " -----------------------\n" );
	ri.Printf( PRINT_ALL, " approx %i kbytes\n", (estTotalSize + 1023) / 1024 );
	ri.Printf( PRINT_ALL, " %i total images\n\n", tr.numImages );
}

//=======================================================================

/*
================
ResampleTexture

Used to resample images in a more general than quartering fashion.

This will only be filtered properly if the resampled size
is greater than half the original size.

If a larger shrinking is needed, use the mipmap function 
before or after.
================
*/
static void ResampleTexture( unsigned *in, int inwidth, int inheight, unsigned *out,  
							int outwidth, int outheight ) {
	int		i, j;
	unsigned	*inrow, *inrow2;
	unsigned	frac, fracstep;
	unsigned	p1[MAX_TEXTURE_SIZE];
	unsigned	p2[MAX_TEXTURE_SIZE];
	byte		*pix1, *pix2, *pix3, *pix4;

	if ( outwidth > ARRAY_LEN( p1 ) )
		ri.Error( ERR_DROP, "ResampleTexture: max width" );
								
	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep>>2;
	for ( i=0 ; i<outwidth ; i++ ) {
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}
	frac = 3*(fracstep>>2);
	for ( i=0 ; i<outwidth ; i++ ) {
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	for (i=0 ; i<outheight ; i++, out += outwidth) {
		inrow = in + inwidth*(int)((i+0.25)*inheight/outheight);
		inrow2 = in + inwidth*(int)((i+0.75)*inheight/outheight);
		for (j=0 ; j<outwidth ; j++) {
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			((byte *)(out+j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			((byte *)(out+j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			((byte *)(out+j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			((byte *)(out+j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}
}


/*
================
R_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
static void R_LightScaleTexture( byte *in, int inwidth, int inheight, qboolean only_gamma )
{
	if ( in == NULL )
		return;

	if ( only_gamma )
	{
#ifdef USE_VULKAN
		if ( !glConfig.deviceSupportsGamma && !vk.fboActive )
#else
		if ( !glConfig.deviceSupportsGamma )
#endif
		{
			int		i, c;
			byte	*p;

			p = (byte *)in;

			c = inwidth*inheight;
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_gammatable[p[0]];
				p[1] = s_gammatable[p[1]];
				p[2] = s_gammatable[p[2]];
			}
		}
	}
	else
	{
		int		i, c;
		byte	*p;

		p = (byte *)in;

		c = inwidth*inheight;

#ifdef USE_VULKAN
		if ( glConfig.deviceSupportsGamma || vk.fboActive )
#else
		if ( glConfig.deviceSupportsGamma )
#endif
		{
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_intensitytable[p[0]];
				p[1] = s_intensitytable[p[1]];
				p[2] = s_intensitytable[p[2]];
			}
		}
		else
		{
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_gammatable[s_intensitytable[p[0]]];
				p[1] = s_gammatable[s_intensitytable[p[1]]];
				p[2] = s_gammatable[s_intensitytable[p[2]]];
			}
		}
	}
}


/*
================
R_MipMap2

Operates in place, quartering the size of the texture
Proper linear filter
================
*/
static void R_MipMap2( unsigned * const out, unsigned * const in, int inWidth, int inHeight ) {
	int			i, j, k;
	byte		*outpix;
	int			inWidthMask, inHeightMask;
	int			total;
	int			outWidth, outHeight;
	unsigned	*temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;

	if ( out == in )
		temp = ri.Hunk_AllocateTempMemory( outWidth * outHeight * 4 );
	else
		temp = out;

	inWidthMask = inWidth - 1;
	inHeightMask = inHeight - 1;

	for ( i = 0 ; i < outHeight ; i++ ) {
		for ( j = 0 ; j < outWidth ; j++ ) {
			outpix = (byte *) ( temp + i * outWidth + j );
			for ( k = 0 ; k < 4 ; k++ ) {
				total = 
					1 * ((byte *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					1 * ((byte *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					2 * ((byte *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					4 * ((byte *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					4 * ((byte *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					2 * ((byte *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					4 * ((byte *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					4 * ((byte *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					1 * ((byte *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					1 * ((byte *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k];
				outpix[k] = total / 36;
			}
		}
	}

	if ( out == in ) {
		Com_Memcpy( out, temp, outWidth * outHeight * 4 );
		ri.Hunk_FreeTempMemory( temp );
	}
}


/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
*/
static void R_MipMap( byte *out, byte *in, int width, int height ) {
	int		i, j;
	int		row;

	if ( in == NULL )
		return;

	if ( !r_simpleMipMaps->integer ) {
		R_MipMap2( (unsigned *)out, (unsigned *)in, width, height );
		return;
	}

	if ( width == 1 && height == 1 ) {
		return;
	}

	row = width * 4;
	width >>= 1;
	height >>= 1;

	if ( width == 0 || height == 0 ) {
		width += height;	// get largest
		for (i=0 ; i<width ; i++, out+=4, in+=8 ) {
			out[0] = ( in[0] + in[4] )>>1;
			out[1] = ( in[1] + in[5] )>>1;
			out[2] = ( in[2] + in[6] )>>1;
			out[3] = ( in[3] + in[7] )>>1;
		}
		return;
	}

	for (i=0 ; i<height ; i++, in+=row) {
		for (j=0 ; j<width ; j++, out+=4, in+=8) {
			out[0] = (in[0] + in[4] + in[row+0] + in[row+4])>>2;
			out[1] = (in[1] + in[5] + in[row+1] + in[row+5])>>2;
			out[2] = (in[2] + in[6] + in[row+2] + in[row+6])>>2;
			out[3] = (in[3] + in[7] + in[row+3] + in[row+7])>>2;
		}
	}
}


/*
==================
R_BlendOverTexture

Apply a color blend over a set of pixels
==================
*/
static void R_BlendOverTexture( byte *data, int pixelCount, int mipLevel ) {

	static const byte blendColors[][4] = {
		{255,0,0,128},
		{255,255,0,128},
		{0,255,0,128},
		{0,255,255,128},
		{0,0,255,128},
		{255,0,255,128}
	};

	const byte *blend;
	int		i;
	int		inverseAlpha;
	int		premult[3];

	if ( data == NULL )
		return;

	if ( mipLevel <= 0 )
		return;

	blend = blendColors[ ( mipLevel - 1 ) % ARRAY_LEN( blendColors ) ];

	inverseAlpha = 255 - blend[3];
	premult[0] = blend[0] * blend[3];
	premult[1] = blend[1] * blend[3];
	premult[2] = blend[2] * blend[3];

	for ( i = 0 ; i < pixelCount ; i++, data+=4 ) {
		data[0] = ( data[0] * inverseAlpha + premult[0] ) >> 9;
		data[1] = ( data[1] * inverseAlpha + premult[1] ) >> 9;
		data[2] = ( data[2] * inverseAlpha + premult[2] ) >> 9;
	}
}


static qboolean RawImage_HasAlpha( const byte *scan, const int numPixels )
{
	int i;

	if ( !scan )
		return qtrue;

	for ( i = 0; i < numPixels; i++ )
	{
		if ( scan[i*4 + 3] != 255 )
		{
			return qtrue;
		}
	}

	return qfalse;
}

#ifdef USE_VULKAN

typedef struct {
	byte *buffer;
	int buffer_size;
	int mip_levels;
	int base_level_width;
	int base_level_height;
} Image_Upload_Data;

// Bytes a base WxH image occupies once uploaded: the full mip chain (RGBA) when
// mipmapped, otherwise just the base level. Used to measure the low-end Mali
// texture clamp's saving (see vk.reg_img_* / vk_registration_report).
static uint32_t R_MipChainBytes( int w, int h, qboolean mipmap ) {
	uint32_t total = 0;
	for ( ;; ) {
		total += (uint32_t)w * (uint32_t)h * 4;
		if ( !mipmap || ( w == 1 && h == 1 ) )
			break;
		w = ( w > 1 ) ? ( w >> 1 ) : 1;
		h = ( h > 1 ) ? ( h >> 1 ) : 1;
	}
	return total;
}

static void generate_image_upload_data( image_t *image, byte *data, Image_Upload_Data *upload_data ) {

	qboolean mipmap = (image->flags & IMGFLAG_MIPMAP) ? qtrue : qfalse;
	qboolean picmip = (image->flags & IMGFLAG_PICMIP) ? qtrue : qfalse;
	byte* resampled_buffer = NULL;
	int scaled_width, scaled_height;
	int full_width, full_height; // base dims WITHOUT the low-end Mali clamp (for the saving measurement)
	int width = image->width;
	int height = image->height;
	unsigned* scaled_buffer;
	int mip_level_size;
	int miplevel;

	Com_Memset( upload_data, 0, sizeof( *upload_data ) );

	if ( image->flags & IMGFLAG_NOSCALE ) {
		//
		// keep original dimensions
		//
		scaled_width = width;
		scaled_height = height;
	} else {
		//
		// convert to exact power of 2 sizes
		//
		for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
			;
		for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
			;

		if ( r_roundImagesDown->integer && scaled_width > width )
			scaled_width >>= 1;
		if ( r_roundImagesDown->integer && scaled_height > height )
			scaled_height >>= 1;
	}

	//
	// clamp to the current upper OpenGL limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	//
	while ( scaled_width > glConfig.maxTextureSize
		|| scaled_height > glConfig.maxTextureSize ) {
		scaled_width >>= 1;
		scaled_height >>= 1;
	}

	// Base dims before the low-end Mali clamp -- kept to measure how much the clamp saves.
	full_width = scaled_width;
	full_height = scaled_height;

	// Low-end single-queue tiler (Mali/PowerVR) mission-load mitigation: cap the base
	// size of bulk (picmip) textures so no single upload is oversized. UI/HUD/font
	// textures lack IMGFLAG_PICMIP and stay full-res. See vk.maxTextureClamp (vk.h).
	if ( picmip && vk.maxTextureClamp > 0 ) {
		while ( scaled_width > vk.maxTextureClamp
			|| scaled_height > vk.maxTextureClamp ) {
			scaled_width >>= 1;
			scaled_height >>= 1;
		}
	}

	upload_data->buffer = (byte*) ri.Hunk_AllocateTempMemory( 2 * 4 * scaled_width * scaled_height );
	if ( data == NULL ) {
		Com_Memset( upload_data->buffer, 0, 2 * 4 * scaled_width * scaled_height );
	}

	if ( ( scaled_width != width || scaled_height != height ) && data ) {
		resampled_buffer = (byte*) ri.Hunk_AllocateTempMemory( scaled_width * scaled_height * 4 );
		ResampleTexture ((unsigned*)data, width, height, (unsigned*)resampled_buffer, scaled_width, scaled_height);
		data = resampled_buffer;
	}

	width = scaled_width;
	height = scaled_height;

	if ( data == NULL ) {
		data = upload_data->buffer;
	} else {
		if ( image->flags & IMGFLAG_COLORSHIFT ) {
			byte *p = data;
			int i, n = width * height;
			for ( i = 0; i < n; i++, p+=4 ) {
				R_ColorShiftLightingBytes( p, p, qfalse );
			}
		}
	}

	//
	// perform optional picmip operation
	//
	if ( picmip && ( tr.mapLoading || r_nomip->integer == 0 ) ) {
		// vk.extraPicmip: extra halving of bulk textures on low-end Mali/PowerVR to cut
		// the cumulative mission-load upload storm (0 on capable GPUs -- unchanged).
		scaled_width >>= r_picmip->integer + vk.extraPicmip;
		scaled_height >>= r_picmip->integer + vk.extraPicmip;
		//x >>= r_picmip->integer;
		//y >>= r_picmip->integer;

		// unclamped reference gets only the standard picmip, so full - actual == our saving
		full_width >>= r_picmip->integer;
		full_height >>= r_picmip->integer;
	}

	//
	// clamp to minimum size
	//
	if (scaled_width < 1) {
		scaled_width = 1;
	}
	if (scaled_height < 1) {
		scaled_height = 1;
	}
	if (full_width < 1) {
		full_width = 1;
	}
	if (full_height < 1) {
		full_height = 1;
	}

	// Per-registration upload accounting (measures the low-end Mali clamp's saving).
	{
		uint32_t actual_bytes = R_MipChainBytes( scaled_width, scaled_height, mipmap );
		uint32_t full_bytes   = R_MipChainBytes( full_width, full_height, mipmap );
		vk.reg_img_count++;
		vk.reg_img_bytes += actual_bytes;
		vk.reg_img_bytes_full += full_bytes;
		if ( actual_bytes < full_bytes )
			vk.reg_img_reduced++;
		if ( actual_bytes > vk.reg_img_biggest ) {
			vk.reg_img_biggest = actual_bytes;
			Q_strncpyz( vk.reg_img_biggest_name, image->imgName, sizeof( vk.reg_img_biggest_name ) );
		}
	}

	upload_data->base_level_width = scaled_width;
	upload_data->base_level_height = scaled_height;

	if ( scaled_width == width && scaled_height == height && !mipmap ) {
		upload_data->mip_levels = 1;
		upload_data->buffer_size = scaled_width * scaled_height * 4;

		if ( data != NULL ) {
			Com_Memcpy( upload_data->buffer, data, upload_data->buffer_size );
		}

		if ( resampled_buffer != NULL ) {
			ri.Hunk_FreeTempMemory( resampled_buffer );
		}

		return;	//return upload_data;
	}

	// Use the normal mip-mapping to go down from [width, height] to [scaled_width, scaled_height] dimensions.
	while (width > scaled_width || height > scaled_height) {
		R_MipMap(data, data, width, height);

		width >>= 1;
		if (width < 1) width = 1;

		height >>= 1;
		if (height < 1) height = 1; 
	}

	// At this point width == scaled_width and height == scaled_height.

	scaled_buffer = (unsigned int*) ri.Hunk_AllocateTempMemory( sizeof( unsigned ) * scaled_width * scaled_height );
	Com_Memcpy(scaled_buffer, data, scaled_width * scaled_height * 4);

	if ( !(image->flags & IMGFLAG_NOLIGHTSCALE ) ) {
		R_LightScaleTexture( (byte*)scaled_buffer, scaled_width, scaled_height, !mipmap );
	}

	miplevel = 0;
	mip_level_size = scaled_width * scaled_height * 4;

	Com_Memcpy(upload_data->buffer, scaled_buffer, mip_level_size);
	upload_data->buffer_size = mip_level_size;
	
	if ( mipmap ) {
		while (scaled_width > 1 && scaled_height > 1) {
			R_MipMap((byte *)scaled_buffer, (byte *)scaled_buffer, scaled_width, scaled_height);

			scaled_width >>= 1;
			if (scaled_width < 1) scaled_width = 1;

			scaled_height >>= 1;
			if (scaled_height < 1) scaled_height = 1;

			miplevel++;
			mip_level_size = scaled_width * scaled_height * 4;

			if ( r_colorMipLevels->integer ) {
				R_BlendOverTexture( (byte *)scaled_buffer, scaled_width * scaled_height, miplevel );
			}

			Com_Memcpy(&upload_data->buffer[upload_data->buffer_size], scaled_buffer, mip_level_size);
			upload_data->buffer_size += mip_level_size;
		}
	}

	upload_data->mip_levels = miplevel + 1;

	ri.Hunk_FreeTempMemory( scaled_buffer );

	if ( resampled_buffer != NULL )
		ri.Hunk_FreeTempMemory( resampled_buffer );
}


static void upload_vk_image( image_t *image, byte *pic ) {

	Image_Upload_Data upload_data;
	int w, h;

	generate_image_upload_data( image, pic, &upload_data );

	w = upload_data.base_level_width;
	h = upload_data.base_level_height;

	if ( r_texturebits->integer > 16 || r_texturebits->integer == 0 || ( image->flags & IMGFLAG_LIGHTMAP ) ) {
		image->internalFormat = VK_FORMAT_R8G8B8A8_UNORM;
		//image->internalFormat = VK_FORMAT_B8G8R8A8_UNORM;
	} else {
		qboolean has_alpha = RawImage_HasAlpha( upload_data.buffer, w * h );
		image->internalFormat = has_alpha ? VK_FORMAT_B4G4R4A4_UNORM_PACK16 : VK_FORMAT_A1R5G5B5_UNORM_PACK16;
	}

	image->uploadWidth = w;
	image->uploadHeight = h;

	vk_create_image( image, w, h, upload_data.mip_levels );
	vk_upload_image_data( image, 0, 0, w, h, upload_data.mip_levels, upload_data.buffer, upload_data.buffer_size, qfalse );

	ri.Hunk_FreeTempMemory( upload_data.buffer );
}

#else // !USE_VULKAN

static GLint RawImage_GetInternalFormat( const byte *scan, int numPixels, qboolean lightMap, qboolean allowCompression )
{
	GLint internalFormat;

	if ( lightMap )
		return GL_RGB;

	if ( RawImage_HasAlpha( scan, numPixels ) )
	{
		if ( r_texturebits->integer == 16 )
		{
			internalFormat = GL_RGBA4;
		}
		else if ( r_texturebits->integer == 32 )
		{
			internalFormat = GL_RGBA8;
		}
		else
		{
			internalFormat = GL_RGBA;
		}
	}
	else
	{
		if ( allowCompression && glConfig.textureCompression == TC_S3TC_ARB )
		{
			internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		}
		else if ( allowCompression && glConfig.textureCompression == TC_S3TC )
		{
			internalFormat = GL_RGB4_S3TC;
		}
		else if ( r_texturebits->integer == 16 )
		{
			internalFormat = GL_RGB5;
		}
		else if ( r_texturebits->integer == 32 )
		{
			internalFormat = GL_RGB8;
		}
		else
		{
			internalFormat = GL_RGB;
		}
	}

	return internalFormat;
}


static void LoadTexture( int miplevel, int x, int y, int width, int height, const byte *data, qboolean subImage, image_t *image )
{
	if ( subImage )
		qglTexSubImage2D( GL_TEXTURE_2D, miplevel, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data );
	else
		qglTexImage2D( GL_TEXTURE_2D, miplevel, image->internalFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
}


/*
===============
Upload32
===============
*/
static void Upload32( byte *data, int x, int y, int width, int height, image_t *image, qboolean subImage )
{
	qboolean allowCompression = !(image->flags & IMGFLAG_NO_COMPRESSION);
	qboolean lightMap = image->flags & IMGFLAG_LIGHTMAP;
	qboolean mipmap = image->flags & IMGFLAG_MIPMAP;
	qboolean picmip = image->flags & IMGFLAG_PICMIP;
	byte		*resampledBuffer = NULL;
	int			scaled_width, scaled_height;

	if ( image->flags & IMGFLAG_NOSCALE ) {
		//
		// keep original dimensions
		//
		scaled_width = width;
		scaled_height = height;
	} else {
		//
		// convert to exact power of 2 sizes
		//
		for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
			;
		for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
			;

		if ( r_roundImagesDown->integer && scaled_width > width )
			scaled_width >>= 1;
		if ( r_roundImagesDown->integer && scaled_height > height )
			scaled_height >>= 1;
	}

	//
	// clamp to the current texture size limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	//
	while ( scaled_width > glConfig.maxTextureSize
		|| scaled_height > glConfig.maxTextureSize ) {
		scaled_width >>= 1;
		scaled_height >>= 1;
		x >>= 1;
		y >>= 1;
	}

	if ( scaled_width != width || scaled_height != height ) {
		if ( data ) {
			resampledBuffer = ri.Hunk_AllocateTempMemory( scaled_width * scaled_height * 4 );
			ResampleTexture( (unsigned*)data, width, height, (unsigned*)resampledBuffer, scaled_width, scaled_height );
			data = resampledBuffer;
		}
		width = scaled_width;
		height = scaled_height;
	}

	if ( image->flags & IMGFLAG_COLORSHIFT ) {
		byte *p = data;
		int i, n = width * height;
		for ( i = 0; i < n; i++, p+=4 ) {
			R_ColorShiftLightingBytes( p, p, qfalse );
		}
	}

	//
	// perform optional picmip operation
	//
	if ( picmip && ( tr.mapLoading || r_nomip->integer == 0 ) ) {
		scaled_width >>= r_picmip->integer;
		scaled_height >>= r_picmip->integer;
		x >>= r_picmip->integer;
		y >>= r_picmip->integer;
	}

	//
	// clamp to minimum size
	//
	if (scaled_width < 1) {
		scaled_width = 1;
	}
	if (scaled_height < 1) {
		scaled_height = 1;
	}

	if ( !subImage ) {
		// verify if the alpha channel is being used or not
		if ( image->internalFormat == 0 ) {
			image->internalFormat = RawImage_GetInternalFormat( data, width*height, lightMap, allowCompression );
		}
		image->uploadWidth = scaled_width;
		image->uploadHeight = scaled_height;
	}

	// copy or resample data as appropriate for first MIP level
	if ( ( scaled_width == width ) && ( scaled_height == height ) )
	{
		if ( !mipmap )
		{
			LoadTexture( 0, x, y, scaled_width, scaled_height, data, subImage, image );
			goto done;
		}
	}
	else
	{
		// use the normal mip-mapping function to go down from here
		while ( width > scaled_width || height > scaled_height ) {
			R_MipMap( data, data, width, height );
			width = MAX( 1, width >> 1 );
			height = MAX( 1, height >> 1 );
		}
	}

	if ( !(image->flags & IMGFLAG_NOLIGHTSCALE) )
		R_LightScaleTexture( data, scaled_width, scaled_height, !mipmap );

	LoadTexture( 0, x, y, scaled_width, scaled_height, data, subImage, image );

	if ( mipmap )
	{
		int	miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			R_MipMap( data, data, scaled_width, scaled_height );
			scaled_width = MAX( 1, scaled_width >> 1 );
			scaled_height = MAX( 1, scaled_height >> 1 );
			x >>= 1;
			y >>= 1;
			miplevel++;

			if ( r_colorMipLevels->integer ) {
				R_BlendOverTexture( data, scaled_width * scaled_height, miplevel );
			}

			LoadTexture( miplevel, x, y, scaled_width, scaled_height, data, subImage, image );
		}
	}
done:
	if ( resampledBuffer != NULL )
		ri.Hunk_FreeTempMemory( resampledBuffer );

	GL_CheckErrors();
}


/*
================
R_UploadSubImage
================
*/
void R_UploadSubImage( byte *data, int x, int y, int width, int height, image_t *image )
{
	if ( image )
	{
		GL_Bind( image );
		Upload32( data, x, y, width, height, image, qtrue ); // subImage = qtrue
	}
}
#endif // !USE_VULKAN


/*
================
R_CreateImage

This is the only way any image_t are created
Picture data may be modified in-place during mipmap processing
================
*/
image_t *R_CreateImage( const char *name, const char *name2, byte *pic, int width, int height, imgFlags_t flags ) {
	image_t		*image;
	long		hash;
#ifndef USE_VULKAN
	GLint		glWrapClampMode;
	GLuint		currTexture;
	int			currTMU;
#endif
	int			namelen, namelen2;
	const char	*slash;

	// uploading a texture allocates descriptor sets and submits a staging copy on
	// vk.queue; in SMP mode the render thread must be idle so that access is exclusive.
	R_SyncRenderThread();

	namelen = (int)strlen( name ) + 1;
	if ( namelen > MAX_QPATH ) {
		ri.Error( ERR_DROP, "R_CreateImage: \"%s\" is too long", name );
	}

	if ( name2 && Q_stricmp( name, name2 ) != 0 ) {
		// leave only file name
		name2 = ( slash = strrchr( name2, '/' ) ) != NULL ? slash + 1 : name2;
		namelen2 = (int)strlen( name2 ) + 1;
	} else {
		namelen2 = 0;
	}

	if ( tr.numImages == MAX_DRAWIMAGES ) {
		ri.Error( ERR_DROP, "R_CreateImage: MAX_DRAWIMAGES hit" );
	}

	image = ri.Hunk_Alloc( sizeof( *image ) + namelen + namelen2, h_low );
	image->imgName = (char *)( image + 1 );
	strcpy( image->imgName, name );
	if ( namelen2 ) {
		image->imgName2 = image->imgName + namelen;
		strcpy( image->imgName2, name2 );
	} else {
		image->imgName2 = image->imgName; 
	}

	hash = generateHashValue( name );
	image->next = hashTable[ hash ];
	hashTable[ hash ] = image;

	tr.images[ tr.numImages++ ] = image;

	image->flags = flags;
	image->width = width;
	image->height = height;

	if ( namelen > 6 && Q_stristr( image->imgName, "maps/" ) == image->imgName && Q_stristr( image->imgName + 6, "/lm_" ) != NULL ) {
		// external lightmap atlases stored in maps/<mapname>/lm_XXXX textures
		// image->flags = IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_NOSCALE | IMGFLAG_COLORSHIFT;
		image->flags |= IMGFLAG_NO_COMPRESSION | IMGFLAG_NOSCALE;
	}

#ifdef USE_VULKAN
	if ( flags & IMGFLAG_CLAMPTOBORDER )
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	else if ( flags & IMGFLAG_CLAMPTOEDGE )
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	else
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	image->handle = VK_NULL_HANDLE;
	image->view = VK_NULL_HANDLE;
	image->descriptor = VK_NULL_HANDLE;
	image->normalMap = NULL;
	image->hasBloomColor = qfalse;
	if ( pic && width > 0 && height > 0 && !( flags & ( IMGFLAG_LIGHTMAP | IMGFLAG_NOLIGHTSCALE ) ) ) {
		int total = width * height;
		int step = ( total > 4096 ) ? ( total / 4096 ) : 1;
		float br = 0.0f, bg = 0.0f, bb = 0.0f, bcnt = 0.0f;
		float ar = 0.0f, ag = 0.0f, ab = 0.0f, acnt = 0.0f;
		int idx;
		for ( idx = 0; idx < total; idx += step ) {
			const byte *px = pic + idx * 4;
			float r = px[0] * ( 1.0f / 255.0f );
			float g = px[1] * ( 1.0f / 255.0f );
			float b = px[2] * ( 1.0f / 255.0f );
			float luma = r * 0.299f + g * 0.587f + b * 0.114f;
			ar += r; ag += g; ab += b; acnt += 1.0f;
			if ( luma >= 0.42f ) {
				br += r; bg += g; bb += b; bcnt += 1.0f;
			}
		}
		if ( bcnt >= 4.0f ) {
			image->bloomColor[0] = br / bcnt;
			image->bloomColor[1] = bg / bcnt;
			image->bloomColor[2] = bb / bcnt;
			image->hasBloomColor = qtrue;
		} else if ( acnt > 0.0f ) {
			image->bloomColor[0] = ar / acnt;
			image->bloomColor[1] = ag / acnt;
			image->bloomColor[2] = ab / acnt;
			image->hasBloomColor = qtrue;
		}
	}

	upload_vk_image( image, pic );
#else
	if ( flags & IMGFLAG_RGB )
		image->internalFormat = GL_RGB;
	else
		image->internalFormat = 0; // autodetect

	if ( flags & IMGFLAG_CLAMPTOBORDER )
		glWrapClampMode = GL_CLAMP_TO_BORDER;
	else if ( flags & IMGFLAG_CLAMPTOEDGE )
		glWrapClampMode = gl_clamp_mode;
	else
		glWrapClampMode = GL_REPEAT;

	// save current state
	currTMU = glState.currenttmu;
	currTexture = glState.currenttextures[ glState.currenttmu ];

	qglGenTextures( 1, &image->texnum );

	// lightmaps are always allocated on TMU 1
	if ( qglActiveTextureARB && (flags & IMGFLAG_LIGHTMAP) ) {
		image->TMU = 1;
	} else {
		image->TMU = 0;
	}

	if ( qglActiveTextureARB ) {
		GL_SelectTexture( image->TMU );
	}

	GL_Bind( image );
	Upload32( pic, 0, 0, image->width, image->height, image, qfalse ); // subImage = qfalse

	if ( image->flags & IMGFLAG_MIPMAP )
	{
		if ( textureFilterAnisotropic ) {
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, (GLint) maxAnisotropy );
		}

		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
	}
	else
	{
		if ( textureFilterAnisotropic )
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );

		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, glWrapClampMode );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, glWrapClampMode );

	// restore original state
	GL_SelectTexture( currTMU );
	glState.currenttextures[ glState.currenttmu ] = currTexture;
	qglBindTexture( GL_TEXTURE_2D, currTexture );

#endif
	return image;
}

//===================================================================

typedef struct
{
	const char *ext;
	void (*ImageLoader)( const char *, unsigned char **, int *, int * );
} imageExtToLoaderMap_t;

// Note that the ordering indicates the order of preference used
// when there are multiple images of different formats available
static const imageExtToLoaderMap_t imageLoaders[] =
{
	{ "png",  R_LoadPNG },
	{ "tga",  R_LoadTGA },
	{ "jpg",  R_LoadJPG },
	{ "jpeg", R_LoadJPG },
	{ "pcx",  R_LoadPCX },
	{ "bmp",  R_LoadBMP }
};

static const int numImageLoaders = ARRAY_LEN( imageLoaders );

/*
=================
R_LoadImage

Loads any of the supported image types into a canonical
32 bit format.
=================
*/
static const char *R_LoadImage( const char *name, byte **pic, int *width, int *height )
{
	static char localName[ MAX_QPATH ];
	const char *altName, *ext;
	//qboolean orgNameFailed = qfalse;
	int orgLoader = -1;
	int i;

	*pic = NULL;
	*width = 0;
	*height = 0;

	Q_strncpyz( localName, name, sizeof( localName ) );

	ext = COM_GetExtension( localName );
	if ( *ext )
	{
		// Look for the correct loader and use it
		for ( i = 0; i < numImageLoaders; i++ )
		{
			if ( !Q_stricmp( ext, imageLoaders[ i ].ext ) )
			{
				// Load
				imageLoaders[ i ].ImageLoader( localName, pic, width, height );
				break;
			}
		}

		// A loader was found
		if ( i < numImageLoaders )
		{
			if ( *pic == NULL )
			{
				// Loader failed, most likely because the file isn't there;
				// try again without the extension
				//orgNameFailed = qtrue;
				orgLoader = i;
				COM_StripExtension( name, localName, MAX_QPATH );
			}
			else
			{
				// Something loaded
				return localName;
			}
		}
	}

	// Try and find a suitable match using all
	// the image formats supported
	for ( i = 0; i < numImageLoaders; i++ )
	{
		if ( i == orgLoader )
			continue;

		altName = va( "%s.%s", localName, imageLoaders[ i ].ext );

		// Load
		imageLoaders[ i ].ImageLoader( altName, pic, width, height );

		if ( *pic )
		{
#if 0
			if ( orgNameFailed )
			{
				ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW "WARNING: %s not present, using %s instead\n",
						name, altName );
			}
#endif
			Q_strncpyz( localName, altName, sizeof( localName ) );
			break;
		}
	}

	return localName;
}


#ifdef USE_VULKAN
static uint32_t KTX_ReadU32LE( const byte *p )
{
	return (uint32_t)p[0]
		| ( (uint32_t)p[1] << 8 )
		| ( (uint32_t)p[2] << 16 )
		| ( (uint32_t)p[3] << 24 );
}

/*
===============
R_TryLoadKTX1ASTC

Attempts to load a hardware-compressed KTX1 ASTC texture (<name_without_ext>.ktx)
directly into Vulkan VRAM with precomputed mipmaps. Returns NULL on any miss or
unsupported GPU so R_FindImageFile transparently falls back to .tga/.jpg.
===============
*/
static image_t *R_TryLoadKTX1ASTC( const char *name, imgFlags_t flags )
{
	static const byte ktx1Magic[12] = { 0xAB, 'K', 'T', 'X', ' ', '1', '1', 0xBB, '\r', '\n', 0x1A, '\n' };
	char stripped[ MAX_QPATH ];
	char ktxPath[ MAX_QPATH ];
	union { byte *b; void *v; } fbuf;
	int flen;
	uint32_t endianness, glType, glFormat, glInternalFormat;
	uint32_t pixelWidth, pixelHeight, pixelDepth, arrayElems, numFaces, numMips, kvBytes;
	VkFormat vkFmt;
	int bx, by;
	int baseMip, uploadMips, upW, upH;
	size_t pos;
	uint32_t mipOffsets[16];
	uint32_t mipSizes[16];
	int totalSize = 0;
	byte *packed;
	int m, mw, mh;
	image_t *image;
	long hash;
	int namelen, namelen2;
	const char *slash, *shortName;

	if ( !vk.astcLdr || !r_ext_compressed_textures->integer )
		return NULL;
	if ( flags & ( IMGFLAG_NO_COMPRESSION | IMGFLAG_LIGHTMAP ) )
		return NULL;
	if ( !( flags & IMGFLAG_NOLIGHTSCALE ) && tr.mapLoading && r_mapGreyScale->value > 0 )
		return NULL;
	if ( !( flags & IMGFLAG_NOLIGHTSCALE ) &&
	     ( r_intensity->value != 1.0f || ( !glConfig.deviceSupportsGamma && !vk.fboActive && r_gamma->value != 1.0f ) ) )
		return NULL;
	if ( name[0] == '*' )
		return NULL;

	COM_StripExtension( name, stripped, sizeof( stripped ) );
	if ( !stripped[0] )
		return NULL;
	Com_sprintf( ktxPath, sizeof( ktxPath ), "%s.ktx", stripped );

	flen = ri.FS_ReadFile( ktxPath, &fbuf.v );
	if ( flen < 64 || !fbuf.b )
		return NULL;

	if ( memcmp( fbuf.b, ktx1Magic, 12 ) != 0 ) {
		ri.FS_FreeFile( fbuf.v );
		return NULL;
	}

	endianness       = KTX_ReadU32LE( fbuf.b + 12 );
	glType           = KTX_ReadU32LE( fbuf.b + 16 );
	glFormat         = KTX_ReadU32LE( fbuf.b + 24 );
	glInternalFormat = KTX_ReadU32LE( fbuf.b + 28 );
	pixelWidth       = KTX_ReadU32LE( fbuf.b + 36 );
	pixelHeight      = KTX_ReadU32LE( fbuf.b + 40 );
	pixelDepth       = KTX_ReadU32LE( fbuf.b + 44 );
	arrayElems       = KTX_ReadU32LE( fbuf.b + 48 );
	numFaces         = KTX_ReadU32LE( fbuf.b + 52 );
	numMips          = KTX_ReadU32LE( fbuf.b + 56 );
	kvBytes          = KTX_ReadU32LE( fbuf.b + 60 );

	if ( endianness != 0x04030201u || glType != 0 || glFormat != 0 ||
	     pixelDepth > 1 || arrayElems != 0 || numFaces != 1 ||
	     pixelWidth == 0 || pixelHeight == 0 ||
	     (int)pixelWidth > glConfig.maxTextureSize || (int)pixelHeight > glConfig.maxTextureSize ) {
		ri.FS_FreeFile( fbuf.v );
		return NULL;
	}

	switch ( glInternalFormat ) {
		case 0x93B0u: // GL_COMPRESSED_RGBA_ASTC_4x4_KHR
		case 0x93D0u: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR
			vkFmt = VK_FORMAT_ASTC_4x4_UNORM_BLOCK; bx = 4; by = 4; break;
		case 0x93B2u: // GL_COMPRESSED_RGBA_ASTC_5x5_KHR
		case 0x93D2u:
			vkFmt = VK_FORMAT_ASTC_5x5_UNORM_BLOCK; bx = 5; by = 5; break;
		case 0x93B4u: // GL_COMPRESSED_RGBA_ASTC_6x6_KHR
		case 0x93D4u:
			vkFmt = VK_FORMAT_ASTC_6x6_UNORM_BLOCK; bx = 6; by = 6; break;
		case 0x93B7u: // GL_COMPRESSED_RGBA_ASTC_8x8_KHR
		case 0x93D7u:
			vkFmt = VK_FORMAT_ASTC_8x8_UNORM_BLOCK; bx = 8; by = 8; break;
		default:
			ri.FS_FreeFile( fbuf.v );
			return NULL;
	}

	if ( !vk_astc_format_supported( vkFmt ) ) {
		ri.FS_FreeFile( fbuf.v );
		return NULL;
	}

	if ( numMips == 0 ) numMips = 1;
	if ( numMips > 16 ) numMips = 16;

	pos = 64 + (size_t)kvBytes;
	if ( pos >= (size_t)flen ) {
		ri.FS_FreeFile( fbuf.v );
		return NULL;
	}

	baseMip = 0;
	if ( ( flags & IMGFLAG_PICMIP ) && r_picmip->integer > 0 && numMips > 1 ) {
		baseMip = r_picmip->integer;
		if ( baseMip >= (int)numMips )
			baseMip = (int)numMips - 1;
	}
	uploadMips = ( flags & IMGFLAG_MIPMAP ) ? ( (int)numMips - baseMip ) : 1;
	if ( uploadMips < 1 ) uploadMips = 1;
	if ( uploadMips > 16 ) uploadMips = 16;

	// First pass: validate mip sizes and compute contiguous packed buffer size
	mw = (int)pixelWidth;
	mh = (int)pixelHeight;
	upW = mw;
	upH = mh;
	{
		size_t scan = pos;
		for ( m = 0; m < baseMip + uploadMips; m++ ) {
			uint32_t imgSize, expected, padded;
			if ( scan + 4 > (size_t)flen ) {
				ri.FS_FreeFile( fbuf.v );
				return NULL;
			}
			imgSize = KTX_ReadU32LE( fbuf.b + scan );
			scan += 4;
			expected = (uint32_t)( ( ( mw + bx - 1 ) / bx ) * ( ( mh + by - 1 ) / by ) * 16 );
			padded = ( imgSize + 3u ) & ~3u;
			if ( imgSize != expected || scan + imgSize > (size_t)flen ) {
				ri.FS_FreeFile( fbuf.v );
				return NULL;
			}
			if ( m == baseMip ) {
				upW = mw;
				upH = mh;
			}
			if ( m >= baseMip ) {
				int idx = m - baseMip;
				mipOffsets[idx] = (uint32_t)totalSize;
				mipSizes[idx] = imgSize;
				totalSize += (int)imgSize; // ASTC blocks are always 16-byte multiples
			}
			scan += padded;
			mw = ( mw > 1 ) ? ( mw >> 1 ) : 1;
			mh = ( mh > 1 ) ? ( mh >> 1 ) : 1;
		}
	}

	namelen = (int)strlen( name ) + 1;
	if ( namelen > MAX_QPATH || tr.numImages == MAX_DRAWIMAGES ) {
		ri.FS_FreeFile( fbuf.v );
		return NULL;
	}

	// Second pass: copy selected mip levels contiguously into temp hunk buffer
	packed = (byte *)ri.Hunk_AllocateTempMemory( totalSize );
	{
		size_t scan = pos;
		for ( m = 0; m < baseMip + uploadMips; m++ ) {
			uint32_t imgSize = KTX_ReadU32LE( fbuf.b + scan );
			uint32_t padded = ( imgSize + 3u ) & ~3u;
			scan += 4;
			if ( m >= baseMip ) {
				Com_Memcpy( packed + mipOffsets[m - baseMip], fbuf.b + scan, imgSize );
			}
			scan += padded;
		}
	}

	R_SyncRenderThread();

	shortName = ( slash = strrchr( ktxPath, '/' ) ) != NULL ? slash + 1 : ktxPath;
	namelen2 = (int)strlen( shortName ) + 1;

	image = ri.Hunk_Alloc( sizeof( *image ) + namelen + namelen2, h_low );
	image->imgName = (char *)( image + 1 );
	strcpy( image->imgName, name );
	image->imgName2 = image->imgName + namelen;
	strcpy( image->imgName2, shortName );

	hash = generateHashValue( name );
	image->next = hashTable[ hash ];
	hashTable[ hash ] = image;
	tr.images[ tr.numImages++ ] = image;

	image->flags = flags;
	image->width = (int)pixelWidth;
	image->height = (int)pixelHeight;
	image->uploadWidth = upW;
	image->uploadHeight = upH;
	image->internalFormat = vkFmt;

	if ( flags & IMGFLAG_CLAMPTOBORDER )
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	else if ( flags & IMGFLAG_CLAMPTOEDGE )
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	else
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	image->handle = VK_NULL_HANDLE;
	image->view = VK_NULL_HANDLE;
	image->descriptor = VK_NULL_HANDLE;
	image->normalMap = NULL;
	image->hasBloomColor = qfalse;
	if ( uploadMips > 0 && mipSizes[uploadMips - 1] >= 16u ) {
		// Decode 1x1 ASTC void-extent mip block directly (zero decoder overhead!) for zero-FBO fake bloom tint
		const byte *blk = packed + mipOffsets[uploadMips - 1];
		uint32_t w0 = (uint32_t)blk[0] | ( (uint32_t)blk[1] << 8 );
		if ( ( w0 & 0x1FFu ) == 0x1FCu ) {
			uint32_t cr = (uint32_t)blk[8]  | ( (uint32_t)blk[9]  << 8 );
			uint32_t cg = (uint32_t)blk[10] | ( (uint32_t)blk[11] << 8 );
			uint32_t cb = (uint32_t)blk[12] | ( (uint32_t)blk[13] << 8 );
			image->bloomColor[0] = (float)cr * ( 1.0f / 65535.0f );
			image->bloomColor[1] = (float)cg * ( 1.0f / 65535.0f );
			image->bloomColor[2] = (float)cb * ( 1.0f / 65535.0f );
			image->hasBloomColor = qtrue;
		}
	}

	vk_create_image( image, upW, upH, uploadMips );
	vk_upload_compressed_image( image, upW, upH, uploadMips, packed, mipOffsets, mipSizes, totalSize );

	ri.Hunk_FreeTempMemory( packed );
	ri.FS_FreeFile( fbuf.v );
	return image;
}
#endif // USE_VULKAN


#ifdef USE_VULKAN
/*
===============
R_TryAttachNormalMap

Automatically attaches an optional tangent-space <stem>_nh companion map
(R=Normal.x, G=Normal.y, B=Micro-AO, A=Height; KTX1 ASTC 4x4 or TGA) to a
loaded diffuse world/model image. Uses IMGFLAG_NOLIGHTSCALE so linear normal
data is never gamma/intensity distorted.
===============
*/
static void R_TryAttachNormalMap( image_t *image )
{
	static qboolean s_loadingNormal = qfalse;
	char stripped[ MAX_QPATH ];
	char nmPath[ MAX_QPATH ];
	size_t len;
	imgFlags_t nmFlags;

	if ( s_loadingNormal || !image || !image->imgName || image->normalMap )
		return;
	if ( !r_normalMapping || !r_normalMapping->integer )
		return;
	if ( r_ohClassic && r_ohClassic->integer )
		return;
	if ( vk.maxBoundDescriptorSets < VK_DESC_COUNT )
		return;
	if ( image->imgName[0] == '*' || ( image->flags & IMGFLAG_LIGHTMAP ) )
		return;

	// Only world textures and 3D models use tangent-space normal maps
	if ( Q_stricmpn( image->imgName, "textures/", 9 ) != 0 &&
	     Q_stricmpn( image->imgName, "models/", 7 ) != 0 ) {
		return;
	}
	if ( Q_stricmpn( image->imgName, "textures/detail/", 16 ) == 0 ||
	     Q_stricmpn( image->imgName, "textures/common/", 16 ) == 0 ||
	     Q_stricmpn( image->imgName, "textures/skies/", 15 ) == 0 ||
	     Q_stricmpn( image->imgName, "textures/decals/", 16 ) == 0 ||
	     Q_stricmpn( image->imgName, "textures/sfx/", 13 ) == 0 ||
	     Q_stricmpn( image->imgName, "textures/effects/", 17 ) == 0 ) {
		return;
	}

	COM_StripExtension( image->imgName, stripped, sizeof( stripped ) );
	len = strlen( stripped );
	if ( len < 2 || len + 8 >= sizeof( nmPath ) )
		return;
	if ( len >= 3 && Q_stricmp( stripped + len - 3, "_nh" ) == 0 )
		return;

	Com_sprintf( nmPath, sizeof( nmPath ), "%s_nh.tga", stripped );
	nmFlags = ( image->flags & ( IMGFLAG_MIPMAP | IMGFLAG_PICMIP | IMGFLAG_CLAMPTOEDGE | IMGFLAG_CLAMPTOBORDER ) )
	        | IMGFLAG_NOLIGHTSCALE;

	s_loadingNormal = qtrue;
	image->normalMap = R_FindImageFile( nmPath, nmFlags );
	s_loadingNormal = qfalse;
}
#endif


/*
===============
R_FindImageFile

Finds or loads the given image.
Returns NULL if it fails, not a default image.
==============
*/
image_t	*R_FindImageFile( const char *name, imgFlags_t flags )
{
	image_t	*image;
	const char *localName;
	char	strippedName[ MAX_QPATH ];
	int		width, height;
	byte	*pic;
	int		hash;

	if ( !name ) {
		return NULL;
	}

	hash = generateHashValue( name );

	//
	// see if the image is already loaded
	//
	for ( image = hashTable[ hash ]; image; image = image->next ) {
		if ( !Q_stricmp( name, image->imgName ) ) {
			// the white image can be used with any set of parms, but other mismatches are errors
			if ( strcmp( name, "*white" ) ) {
				if ( image->flags != flags ) {
					ri.Printf( PRINT_DEVELOPER, "WARNING: reused image %s with mixed flags (%i vs %i)\n", name, image->flags, flags );
				}
			}
			return image;
		}
	}

	if ( strrchr( name, '.' ) > name ) {
		// try with stripped extension
		COM_StripExtension( name, strippedName, sizeof( strippedName ) );
		for ( image = hashTable[ hash ]; image; image = image->next ) {
			if ( !Q_stricmp( strippedName, image->imgName ) ) {
				//if ( strcmp( strippedName, "*white" ) ) {
					if ( image->flags != flags ) {
						ri.Printf( PRINT_DEVELOPER, "WARNING: reused image %s with mixed flags (%i vs %i)\n", strippedName, image->flags, flags );
					}
				//}
				return image;
			}
		}
	}

#ifdef USE_VULKAN
	// Prefer hardware-compressed KTX1 ASTC texture when supported by the GPU (Pi 4/5, ARM64, Apple Silicon).
	image = R_TryLoadKTX1ASTC( name, flags );
	if ( image ) {
		R_TryAttachNormalMap( image );
		return image;
	}
#endif

	//
	// load the pic from disk
	//
	localName = R_LoadImage( name, &pic, &width, &height );
	if ( pic == NULL ) {
		return NULL;
	}

	if ( !( flags & IMGFLAG_NOLIGHTSCALE ) && tr.mapLoading && r_mapGreyScale->value > 0 ) {
		byte *img;
		int i;
		for ( i = 0, img = pic; i < width * height; i++, img += 4 ) {
			if ( r_mapGreyScale->integer ) {
				byte luma = LUMA( img[0], img[1], img[2] );
				img[0] = luma;
				img[1] = luma;
				img[2] = luma;
			} else {
				float luma = LUMA( img[0], img[1], img[2] );
				img[0] = LERP( img[0], luma, r_mapGreyScale->value );
				img[1] = LERP( img[1], luma, r_mapGreyScale->value );
				img[2] = LERP( img[2], luma, r_mapGreyScale->value );
			}
		}
	}

	image = R_CreateImage( name, localName, pic, width, height, flags );
	ri.Free( pic );
#ifdef USE_VULKAN
	R_TryAttachNormalMap( image );
#endif
	return image;
}


/*
================
R_CreateDlightImage
================
*/
#define	DLIGHT_SIZE	16
static void R_CreateDlightImage( void ) {
	int		x,y;
	byte	data[DLIGHT_SIZE][DLIGHT_SIZE][4];
	int		b;

	// make a centered inverse-square falloff blob for dynamic lighting
	for (x=0 ; x<DLIGHT_SIZE ; x++) {
		for (y=0 ; y<DLIGHT_SIZE ; y++) {
			float	d;

			d = ( DLIGHT_SIZE/2 - 0.5f - x ) * ( DLIGHT_SIZE/2 - 0.5f - x ) +
				( DLIGHT_SIZE/2 - 0.5f - y ) * ( DLIGHT_SIZE/2 - 0.5f - y );
			b = 4000 / d;
			if (b > 255) {
				b = 255;
			} else if ( b < 75 ) {
				b = 0;
			}
			data[y][x][0] = 
			data[y][x][1] = 
			data[y][x][2] = b;
			data[y][x][3] = 255;
		}
	}
	tr.dlightImage = R_CreateImage( "*dlight", NULL, (byte*)data, DLIGHT_SIZE, DLIGHT_SIZE, IMGFLAG_CLAMPTOEDGE );
}


/*
=================
R_InitFogTable
=================
*/
void R_InitFogTable( void ) {
	int		i;
	float	d;
	float	exp;

	exp = 0.5;

	for ( i = 0 ; i < FOG_TABLE_SIZE ; i++ ) {
		d = powf( (float)i/(FOG_TABLE_SIZE-1), exp );

		tr.fogTable[i] = d;
	}
}


/*
================
R_FogFactor

Returns a 0.0 to 1.0 fog density value
This is called for each texel of the fog texture on startup
and for each vertex of transparent shaders in fog dynamically
================
*/
float R_FogFactor( float s, float t ) {
	float	d;

	s -= 1.0/512;
	if ( s < 0 ) {
		return 0;
	}
	if ( t < 1.0/32 ) {
		return 0;
	}
	if ( t < 31.0/32 ) {
		s *= (t - 1.0f/32.0f) / (30.0f/32.0f);
	}

	// we need to leave a lot of clamp range
	s *= 8;

	if ( s > 1.0 ) {
		s = 1.0;
	}

	d = tr.fogTable[ (uint32_t)(s * (FOG_TABLE_SIZE-1)) ];

	return d;
}


/*
================
R_CreateFogImage
================
*/
#define	FOG_S	256
#define	FOG_T	32
static void R_CreateFogImage( void ) {
	int		x,y;
	byte	*data;
	float	d;

	data = ri.Hunk_AllocateTempMemory( FOG_S * FOG_T * 4 );

	// S is distance, T is depth
	for (x=0 ; x<FOG_S ; x++) {
		for (y=0 ; y<FOG_T ; y++) {
			d = R_FogFactor( ( x + 0.5f ) / FOG_S, ( y + 0.5f ) / FOG_T );

			data[(y*FOG_S+x)*4+0] = 
			data[(y*FOG_S+x)*4+1] = 
			data[(y*FOG_S+x)*4+2] = 255;
			data[(y*FOG_S+x)*4+3] = 255*d;
		}
	}
	tr.fogImage = R_CreateImage( "*fog", NULL, data, FOG_S, FOG_T, IMGFLAG_CLAMPTOEDGE );
	ri.Hunk_FreeTempMemory( data );
}


static int Hex( char c )
{
	if ( c >= '0' && c <= '9' ) {
		return c - '0';
	}
	if ( c >= 'A' && c <= 'F' ) {
		return 10 + c - 'A';
	}
	if ( c >= 'a' && c <= 'f' ) {
		return 10 + c - 'a';
	}

	return -1;
}


/*
==================
R_BuildDefaultImage

Create solid color texture from following input formats (hex):
#rgb
#rrggbb
==================
*/
#define	DEFAULT_SIZE 16
static qboolean R_BuildDefaultImage( const char *format ) {
	byte data[DEFAULT_SIZE][DEFAULT_SIZE][4];
	byte color[4];
	int i, len, hex[6];
	int x, y;

	if ( *format++ != '#' ) {
		return qfalse;
	}

	len = (int)strlen( format );
	if ( len <= 0 || len > 6 ) {
		return qfalse;
	}

	for ( i = 0; i < len; i++ ) {
		hex[i] = Hex( format[i] );
		if ( hex[i] == -1 ) {
			return qfalse;
		}
	}

	switch ( len ) {
		case 3: // #rgb
			color[0] = hex[0] << 4 | hex[0];
			color[1] = hex[1] << 4 | hex[1];
			color[2] = hex[2] << 4 | hex[2];
			color[3] = 255;
			break;
		case 6: // #rrggbb
			color[0] = hex[0] << 4 | hex[1];
			color[1] = hex[2] << 4 | hex[3];
			color[2] = hex[4] << 4 | hex[5];
			color[3] = 255;
			break;
		default: // unsupported format
			return qfalse;
	}

	for ( y = 0; y < DEFAULT_SIZE; y++ ) {
		for ( x = 0; x < DEFAULT_SIZE; x++ ) {
			data[x][y][0] = color[0];
			data[x][y][1] = color[1];
			data[x][y][2] = color[2];
			data[x][y][3] = color[3];
		}
	}

	tr.defaultImage = R_CreateImage( "*default", NULL, (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, IMGFLAG_MIPMAP );

	return qtrue;
}


/*
==================
R_CreateDefaultImage
==================
*/
static void R_CreateDefaultImage( void ) {
	int		x;
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	if ( r_defaultImage->string[0] )
	{
		// build from format
		if ( R_BuildDefaultImage( r_defaultImage->string ) )
			return;
		// load from external file
		tr.defaultImage = R_FindImageFile( r_defaultImage->string, IMGFLAG_MIPMAP | IMGFLAG_PICMIP );
		if ( tr.defaultImage )
			return;
	}

	// the default image will be a box, to allow you to see the mapping coordinates
	Com_Memset( data, 32, sizeof( data ) );
	for ( x = 0 ; x < DEFAULT_SIZE ; x++ ) {
		data[0][x][0] =
		data[0][x][1] =
		data[0][x][2] =
		data[0][x][3] = 255;

		data[x][0][0] =
		data[x][0][1] =
		data[x][0][2] =
		data[x][0][3] = 255;

		data[DEFAULT_SIZE-1][x][0] =
		data[DEFAULT_SIZE-1][x][1] =
		data[DEFAULT_SIZE-1][x][2] =
		data[DEFAULT_SIZE-1][x][3] = 255;

		data[x][DEFAULT_SIZE-1][0] =
		data[x][DEFAULT_SIZE-1][1] =
		data[x][DEFAULT_SIZE-1][2] =
		data[x][DEFAULT_SIZE-1][3] = 255;
	}

	tr.defaultImage = R_CreateImage( "*default", NULL, (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, IMGFLAG_MIPMAP );
}


/*
==================
R_CreateBuiltinImages
==================
*/
static void R_CreateBuiltinImages( void ) {
	int		x,y;
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	R_CreateDefaultImage();

	Com_Memset( data, 0, sizeof( data ) );
	tr.blackImage = R_CreateImage( "*black", NULL, (byte *)data, 8, 8, IMGFLAG_NONE );

	// we use a solid white image instead of disabling texturing
	Com_Memset( data, 255, sizeof( data ) );
	tr.whiteImage = R_CreateImage( "*white", NULL, (byte *)data, 8, 8, IMGFLAG_NONE );

	// with overbright bits active, we need an image which is some fraction of full color,
	// for default lightmaps, etc
	for (x=0 ; x<DEFAULT_SIZE ; x++) {
		for (y=0 ; y<DEFAULT_SIZE ; y++) {
			data[y][x][0] = 
			data[y][x][1] = 
			data[y][x][2] = tr.identityLightByte;
			data[y][x][3] = 255;
		}
	}

	tr.identityLightImage = R_CreateImage( "*identityLight", NULL, (byte *)data, 8, 8, IMGFLAG_NONE );

	//for ( x = 0; x < ARRAY_LEN( tr.scratchImage ); x++ ) {
		// scratchimage is usually used for cinematic drawing
		//tr.scratchImage[x] = R_CreateImage( "*scratch", (byte*)data, DEFAULT_SIZE, DEFAULT_SIZE,
		//	IMGFLAG_PICMIP | IMGFLAG_CLAMPTOEDGE | IMGFLAG_RGB );
	//}

	R_CreateDlightImage();
	R_CreateFogImage();

	// Procedural 64x64 2-lobe Gaussian glow texture for zero-FBO Fake-Bloom (no harsh lens-flare rings/crosses)
	{
		byte halo[64][64][4];
		int hx, hy;
		for ( hy = 0; hy < 64; hy++ ) {
			float dy = ( (float)hy + 0.5f ) * ( 2.0f / 64.0f ) - 1.0f;
			for ( hx = 0; hx < 64; hx++ ) {
				float dx = ( (float)hx + 0.5f ) * ( 2.0f / 64.0f ) - 1.0f;
				float r2 = dx * dx + dy * dy;
				float v = 0.0f;
				if ( r2 < 1.0f ) {
					float edge = 1.0f - r2;
					edge *= edge;
					v = ( 0.35f * expf( -r2 * 10.0f ) + 0.65f * expf( -r2 * 2.6f ) ) * edge;
				}
				int b = (int)( v * 255.0f + 0.5f );
				if ( b < 0 ) b = 0;
				else if ( b > 255 ) b = 255;
				halo[hy][hx][0] = (byte)b;
				halo[hy][hx][1] = (byte)b;
				halo[hy][hx][2] = (byte)b;
				halo[hy][hx][3] = (byte)b;
			}
		}
		tr.bloomHaloImage = R_CreateImage( "*bloomHalo", NULL, (byte *)halo, 64, 64, IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOLIGHTSCALE );
	}
}


/*
===============
R_SetColorMappings
===============
*/
void R_SetColorMappings( void ) {
	int		i, j;
	float	g;
	int		inf;
	int		shift;
	qboolean applyGamma;

	if ( !tr.inited ) {
		// it may be called from window handling functions where gamma flags is now yet known/set
		return;
	}

	// setup the overbright lighting
	// negative value will force gamma in windowed mode
	tr.overbrightBits = abs( r_overBrightBits->integer );

	// never overbright in windowed mode
#ifdef USE_VULKAN
	if ( !glConfig.isFullscreen && r_overBrightBits->integer >= 0 && !vk.fboActive && !vk.directPostActive ) {
#else
	if ( !glConfig.isFullscreen && r_overBrightBits->integer >= 0 ) {
#endif
		tr.overbrightBits = 0;
		applyGamma = qfalse;
	} else {
#ifdef USE_VULKAN
		if ( !glConfig.deviceSupportsGamma && !vk.fboActive && !vk.directPostActive ) {
#else
		if ( !glConfig.deviceSupportsGamma ) {
#endif
			tr.overbrightBits = 0; // need hardware gamma for overbright
			applyGamma = qfalse;
		} else {
			applyGamma = qtrue;
		}
	}

	// allow 2 overbright bits in 24 bit, but only 1 in 16 bit
	if ( glConfig.colorBits > 16 ) {
		if ( tr.overbrightBits > 2 ) {
			tr.overbrightBits = 2;
		}
	} else {
		if ( tr.overbrightBits > 1 ) {
			tr.overbrightBits = 1;
		}
	}
	if ( tr.overbrightBits < 0 ) {
		tr.overbrightBits = 0;
	}

	tr.identityLight = 1.0f / ( 1 << tr.overbrightBits );
	tr.identityLightByte = 255 * tr.identityLight;

	g = r_gamma->value;

	shift = tr.overbrightBits;

	for ( i = 0; i < ARRAY_LEN( s_gammatable ); i++ ) {
		if ( g == 1.0f ) {
			inf = i;
		} else {
			inf = 255 * powf( i/255.0f, 1.0f / g ) + 0.5f;
		}
		inf <<= shift;
		if (inf < 0) {
			inf = 0;
		}
		if (inf > 255) {
			inf = 255;
		}
		s_gammatable[i] = inf;
	}

	for ( i = 0; i < ARRAY_LEN( s_intensitytable ); i++ ) {
		j = i * r_intensity->value;
		if ( j > 255 ) {
			j = 255;
		}
		s_intensitytable[i] = j;
	}

#ifdef USE_VULKAN
	if ( gls.deviceSupportsGamma ) {
		if ( vk.fboActive || vk.directPostActive )
			ri.GLimp_SetGamma( s_gammatable_linear, s_gammatable_linear, s_gammatable_linear );
		else {
			if ( applyGamma ) {
				ri.GLimp_SetGamma( s_gammatable, s_gammatable, s_gammatable );
			}
		}
	}
#else
	if ( gls.deviceSupportsGamma ) {
		if ( applyGamma ) {
			ri.GLimp_SetGamma( s_gammatable, s_gammatable, s_gammatable );
		}
	}
#endif
}


/*
===============
R_InitImages
===============
*/
void R_InitImages( void ) {

#ifdef USE_VULKAN
	// initialize linear gamma table before setting color mappings for the first time
	int i;

	for ( i = 0; i < 256; i++ )
		s_gammatable_linear[i] = (unsigned char)i;
#endif

	Com_Memset( hashTable, 0, sizeof( hashTable ) );

	// build brightness translation tables
	R_SetColorMappings();

	// create default texture and white texture
	R_CreateBuiltinImages();

#ifdef USE_VULKAN
	vk_update_post_process_pipelines();
#endif
}


/*
===============
R_DeleteTextures
===============
*/
void R_DeleteTextures( void ) {
	int i;

	if ( tr.numImages == 0 ) {
		return;
	}

#ifdef USE_VULKAN
	vk_wait_idle();

	for ( i = 0; i < tr.numImages; i++ ) {
		image_t *img = tr.images[ i ];
		vk_destroy_image_resources( &img->handle, &img->view );

		// img->descriptor will be released with pool reset
	}
#else
	for ( i = 0; i < tr.numImages; i++ ) {
		image_t *img = tr.images[ i ];
		qglDeleteTextures( 1, &img->texnum );
	}

	if ( qglActiveTextureARB ) {
		for ( i = glConfig.numTextureUnits - 1; i >= 0; i-- ) {
			qglActiveTextureARB( GL_TEXTURE0_ARB + i );
			qglBindTexture( GL_TEXTURE_2D, 0 );
		}
	} else {
		qglBindTexture( GL_TEXTURE_2D, 0 );
	}
#endif

	Com_Memset( tr.images, 0, sizeof( tr.images ) );
	Com_Memset( tr.scratchImage, 0, sizeof( tr.scratchImage ) );
	tr.numImages = 0;

	Com_Memset( glState.currenttextures, 0, sizeof( glState.currenttextures ) );
}


/*
============================================================================

SKINS

============================================================================
*/

/*
==================
CommaParse

This is unfortunate, but the skin files aren't
compatible with our normal parsing rules.
==================
*/
static char *CommaParse( const char **data_p ) {
	int c, len;
	const char *data;
	static char com_token[ MAX_TOKEN_CHARS ];

	data = *data_p;
	com_token[0] = '\0';

	// make sure incoming data is valid
	if ( !data ) {
		*data_p = NULL;
		return com_token;
	}

	len = 0;

	while ( 1 ) {
		// skip whitespace
		while ( (c = *data) <= ' ' ) {
			if ( c == '\0' ) {
				break;
			}
			data++;
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			data += 2;
			while ( *data && *data != '\n' ) {
				data++;
			}
		}
		// skip /* */ comments
		else if ( c == '/' && data[1] == '*' ) 
		{
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) ) 
			{
				data++;
			}
			if ( *data ) 
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	if ( c == '\0' ) {
		return "";
	}

	// handle quoted strings
	if ( c == '\"' )
	{
		data++;
		while (1)
		{
			c = *data;
			if ( c == '\"' || c == '\0' )
			{
				if ( c == '\"' )
					data++;
				com_token[ len ] = '\0';
				*data_p = data;
				return com_token;
			}
			data++;
			if ( len < MAX_TOKEN_CHARS-1 )
			{
				com_token[ len ] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if ( len < MAX_TOKEN_CHARS-1 )
		{
			com_token[ len ] = c;
			len++;
		}
		data++;
		c = *data;
	} while ( c > ' ' && c != ',' );

	com_token[ len ] = '\0';

	*data_p = data;
	return com_token;
}


/*
===============
RE_RegisterSkin
===============
*/
static qhandle_t R_RegisterSkinSingle( const char *name ) {
	skinSurface_t parseSurfaces[MAX_SKIN_SURFACES];
	qhandle_t	hSkin;
	skin_t		*skin;
	skinSurface_t	*surf;
	union {
		char *c;
		void *v;
	} text;
	const char	*text_p;
	const char	*token;
	char		surfName[MAX_QPATH];
	int			totalSurfaces;

	if ( !name || !name[0] ) {
		ri.Printf( PRINT_DEVELOPER, "Empty name passed to RE_RegisterSkin\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_DEVELOPER, "Skin name exceeds MAX_QPATH\n" );
		return 0;
	}


	// see if the skin is already loaded
	for ( hSkin = 1; hSkin < tr.numSkins ; hSkin++ ) {
		skin = tr.skins[hSkin];
		if ( !Q_stricmp( skin->name, name ) ) {
			if( skin->numSurfaces == 0 ) {
				return 0;		// default skin
			}
			return hSkin;
		}
	}

	// allocate a new skin
	if ( tr.numSkins == MAX_SKINS ) {
		ri.Printf( PRINT_WARNING, "WARNING: RE_RegisterSkin( '%s' ) MAX_SKINS hit\n", name );
		return 0;
	}
	tr.numSkins++;
	skin = ri.Hunk_Alloc( sizeof( skin_t ), h_low );
	tr.skins[hSkin] = skin;
	Q_strncpyz( skin->name, name, sizeof( skin->name ) );
	skin->numSurfaces = 0;

	// If not a .skin file, load as a single shader
	if ( strcmp( name + strlen( name ) - 5, ".skin" ) ) {
		skin->numSurfaces = 1;
		skin->surfaces = ri.Hunk_Alloc( sizeof( skinSurface_t ), h_low );
		skin->surfaces[0].shader = R_FindShader( name, LIGHTMAP_NONE, qtrue );
		return hSkin;
	}

	// load and parse the skin file
	ri.FS_ReadFile( name, &text.v );
	if ( !text.c ) {
		return 0;
	}

	totalSurfaces = 0;
	text_p = text.c;
	while ( text_p && *text_p ) {
		// get surface name
		token = CommaParse( &text_p );
		Q_strncpyz( surfName, token, sizeof( surfName ) );

		if ( !token[0] ) {
			break;
		}
		// lowercase the surface name so skin compares are faster
		Q_strlwr( surfName );

		if ( *text_p == ',' ) {
			text_p++;
		}

		if ( strstr( token, "tag_" ) ) {
			continue;
		}

		// parse the shader name
		token = CommaParse( &text_p );

		if ( skin->numSurfaces < MAX_SKIN_SURFACES ) {
			surf = &parseSurfaces[skin->numSurfaces];
			Q_strncpyz( surf->name, surfName, sizeof( surf->name ) );
			surf->shader = R_FindShader( token, LIGHTMAP_NONE, qtrue );
			skin->numSurfaces++;
		}

		totalSurfaces++;
	}

	ri.FS_FreeFile( text.v );

	if ( totalSurfaces > MAX_SKIN_SURFACES ) {
		ri.Printf( PRINT_WARNING, "WARNING: Ignoring excess surfaces (found %d, max is %d) in skin '%s'!\n",
					totalSurfaces, MAX_SKIN_SURFACES, name );
	}

	// never let a skin have 0 shaders
	if ( skin->numSurfaces == 0 ) {
		return 0;		// use default skin
	}

	// copy surfaces to skin
	skin->surfaces = ri.Hunk_Alloc( skin->numSurfaces * sizeof( skinSurface_t ), h_low );
	memcpy( skin->surfaces, parseSurfaces, skin->numSurfaces * sizeof( skinSurface_t ) );

	return hSkin;
}

/*
===============
RE_RegisterSkin

EF facial-viseme extension convention (Raven). A head skin "models/players/X/head_Y.skin" ships with
mouth-shape variants "head_Y-1.skin" .. "head_Y-8.skin". The retail renderer registers those variants
CONTIGUOUSLY right after the base skin and returns the NEGATED base handle so the cgame knows facial
extensions exist (cg_main.cpp: ci->headSkin<0 -> ci->extensions=qtrue, headSkin=-headSkin; then
cg_players.cpp CG_PlayerHeadExtension: head->customSkin = ci->headSkin + gi.S_Override[ent], i.e. the
viseme picked by voice volume, plus blink/frown). Stock Quake3 RE_RegisterSkin (now R_RegisterSkinSingle)
lacked this, so ci->extensions stayed qfalse and faces NEVER animated anywhere. The variants are
sequential; we stop at the first gap. Non-head skins simply have no variants and return the positive
handle unchanged.
===============
*/
#define MAX_SKIN_EXTENSIONS 8
qhandle_t RE_RegisterSkin( const char *name ) {
	qhandle_t	base;
	size_t		len;
	int			v;
	qboolean	haveExt = qfalse;
	char		stem[MAX_QPATH], vn[MAX_QPATH];

	base = R_RegisterSkinSingle( name );
	if ( base <= 0 ) {
		return base;
	}
	len = strlen( name );
	if ( len < 5 || Q_stricmp( name + len - 5, ".skin" ) ) {
		return base;			// only .skin files can carry viseme variants
	}

	Q_strncpyz( stem, name, sizeof( stem ) );
	stem[len - 5] = '\0';		// drop ".skin"
	for ( v = 1; v <= MAX_SKIN_EXTENSIONS; v++ ) {
		void *buf = NULL;
		Com_sprintf( vn, sizeof( vn ), "%s-%d.skin", stem, v );
		// existence check first: R_RegisterSkinSingle allocates a slot BEFORE the file read, so calling
		// it on a missing variant would leak a skin slot and break the required contiguity.
		ri.FS_ReadFile( vn, &buf );
		if ( !buf ) {
			break;
		}
		ri.FS_FreeFile( buf );
		R_RegisterSkinSingle( vn );	// contiguous: handle == base + v
		haveExt = qtrue;
	}

	return haveExt ? -base : base;
}


/*
===============
R_InitSkins
===============
*/
void	R_InitSkins( void ) {
	skin_t		*skin;

	tr.numSkins = 1;

	// make the default skin have all default shaders
	skin = tr.skins[0] = ri.Hunk_Alloc( sizeof( skin_t ), h_low );
	Q_strncpyz( skin->name, "<default skin>", sizeof( skin->name )  );
	skin->numSurfaces = 1;
	skin->surfaces = ri.Hunk_Alloc( sizeof( skinSurface_t ), h_low );
	skin->surfaces[0].shader = tr.defaultShader;
}


/*
===============
R_GetSkinByHandle
===============
*/
skin_t	*R_GetSkinByHandle( qhandle_t hSkin ) {
	if ( hSkin < 1 || hSkin >= tr.numSkins ) {
		return tr.skins[0];
	}
	return tr.skins[ hSkin ];
}


/*
===============
R_SkinList_f
===============
*/
void	R_SkinList_f( void ) {
	int			i, j;
	skin_t		*skin;

	ri.Printf (PRINT_ALL, "------------------\n");

	for ( i = 0 ; i < tr.numSkins ; i++ ) {
		skin = tr.skins[i];

		ri.Printf( PRINT_ALL, "%3i:%s (%d surfaces)\n", i, skin->name, skin->numSurfaces );
		for ( j = 0 ; j < skin->numSurfaces ; j++ ) {
			ri.Printf( PRINT_ALL, "       %s = %s\n", 
				skin->surfaces[j].name, skin->surfaces[j].shader->name );
		}
	}
	ri.Printf (PRINT_ALL, "------------------\n");
}
