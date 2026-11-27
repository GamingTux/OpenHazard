#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const float gamma = 1.0;
layout(constant_id = 1) const float obScale = 2.0;
layout(constant_id = 2) const float greyscale = 0.0;
//
layout(constant_id = 7) const int ditherMode = 0; // 0 - disabled, 1 - ordered
layout(constant_id = 8) const int depth_r = 255;
layout(constant_id = 9) const int depth_g = 255;
layout(constant_id = 10) const int depth_b = 255;
layout(constant_id = 11) const int tonemapMode = 0;      // 0 - disabled, 1 - filmic highlight roll-off
layout(constant_id = 12) const float sharpenStrength = 0.0; // 0.0 - disabled, 0.0..1.0 - FidelityFX CAS
layout(constant_id = 14) const float vignetteStrength = 0.0; // 0.0 - disabled, 0.0..0.5 - subtle edge vignette

const vec3 sRGB = { 0.2126, 0.7152, 0.0722 };

// Zero-array Interleaved Gradient Noise (Jimenez 2014): 3 ALU ops instead of a 64-entry LUT
// (avoids 35 ms/frame scratch-RAM array spill on VideoCore VII / mobile TBDR GPUs!).
float threshold() {
	return fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
}

vec3 dither(vec3 color) {
	ivec3 depth = ivec3(depth_r, depth_g, depth_b);
	vec3 cDenormalized = color * depth;
	vec3 cLow = floor(cDenormalized);
	vec3 cFractional = cDenormalized - cLow;
	vec3 cDithered = cLow + step(threshold(), cFractional);
	return cDithered / depth;
}

// 5-tap AMD FidelityFX Contrast Adaptive Sharpening (CAS)
vec3 casSharpen(vec3 e) {
	vec3 b = textureOffset(texture0, frag_tex_coord, ivec2( 0, -1)).rgb;
	vec3 d = textureOffset(texture0, frag_tex_coord, ivec2(-1,  0)).rgb;
	vec3 f = textureOffset(texture0, frag_tex_coord, ivec2( 1,  0)).rgb;
	vec3 h = textureOffset(texture0, frag_tex_coord, ivec2( 0,  1)).rgb;

	vec3 mn = min(min(min(b, d), min(f, h)), e);
	vec3 mx = max(max(max(b, d), max(f, h)), e);

	// Soft adaptive sharpening weight: suppresses ringing on already high-contrast edges
	vec3 amp = sqrt(clamp(min(mn, 1.0 - mx) / max(mx, vec3(1e-4)), 0.0, 1.0));
	float peak = -1.0 / mix(8.0, 5.0, clamp(sharpenStrength, 0.0, 1.0));
	vec3 w = amp * peak;

	vec3 sharpened = (e + (b + d + f + h) * w) / (1.0 + 4.0 * w);
	return clamp(sharpened, 0.0, 1.0);
}

// Filmic highlight roll-off + gentle micro-contrast tuned for Elite Force moody lighting:
// preserves dark Borg shadows 1:1 while preventing bright phasers/bloom from clipping flat white.
vec3 filmicTonemap(vec3 srgb) {
	vec3 color = pow(max(srgb, vec3(0.0)), vec3(2.2));

	// Subtle filmic S-curve contrast without crushing deep shadows
	vec3 contrast = color * color * (3.0 - 2.0 * clamp(color, 0.0, 1.0));
	color = mix(color, contrast, 0.14);

	// Khronos PBR-Neutral style smooth highlight compression + subtle desaturation
	const float startCompression = 0.72;
	const float desaturation = 0.18;
	float peak = max(color.r, max(color.g, color.b));
	if (peak > startCompression) {
		const float d = 1.0 - startCompression;
		float newPeak = 1.0 - (d * d) / (peak + d - startCompression);
		color *= newPeak / peak;
		float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
		color = mix(color, vec3(newPeak), g);
	}

	return pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
}

void main() {
	vec3 base = texture(texture0, frag_tex_coord).rgb;

	if ( sharpenStrength > 0.0 )
	{
		base = casSharpen(base);
	}

	if ( greyscale == 1 )
	{
		base = vec3(dot(base, sRGB));
	}
	else if ( greyscale != 0 )
	{
		vec3 luma = vec3(dot(base, sRGB));
		base = mix(base, luma, greyscale);
	}

	if ( gamma != 1.0 )
	{
		base = pow(max(base, vec3(0.0)), vec3(gamma)) * obScale;
	}
	else
	{
		base = base * obScale;
	}

	if ( tonemapMode == 1 )
	{
		base = filmicTonemap(base);
	}

	if ( vignetteStrength > 0.0 )
	{
		vec2 vuv = frag_tex_coord * 2.0 - 1.0;
		float vig = 1.0 - vignetteStrength * smoothstep(0.45, 1.55, dot(vuv, vuv));
		base *= vig;
	}

	out_color = vec4(base, 1.0);

	if ( ditherMode == 1 ) {
		out_color.rgb = dither(out_color.rgb);
	}
}
