#version 450

// On-chip tile-memory post-processing subpass for Direct-to-Swapchain mode (r_fbo 0).
// Reads the resolved scene color via subpassLoad() (zero full-frame DRAM round-trip on TBDR GPUs)
// and outputs directly to the swapchain attachment.

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputColor;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const float gamma = 1.0;
layout(constant_id = 1) const float obScale = 1.0;
layout(constant_id = 2) const float greyscale = 0.0;
layout(constant_id = 7) const int ditherMode = 0; // 0 - disabled, 1 - ordered 8x8 Bayer
layout(constant_id = 8) const int depth_r = 255;
layout(constant_id = 9) const int depth_g = 255;
layout(constant_id = 10) const int depth_b = 255;
layout(constant_id = 11) const int tonemapMode = 0;       // 0 - disabled, 1 - filmic highlight roll-off
layout(constant_id = 14) const float vignetteStrength = 0.0; // 0.0 - disabled, 0.0..0.5 - subtle edge vignette

const vec3 sRGB = vec3( 0.2126, 0.7152, 0.0722 );

// Zero-array Interleaved Gradient Noise (Jimenez 2014): 3 ALU ops, zero register/scratch spill on TBDR GPUs.
float threshold() {
	return fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
}

vec3 dither(vec3 color) {
	vec3 depth = vec3(float(depth_r), float(depth_g), float(depth_b));
	vec3 cDenormalized = color * depth;
	vec3 cLow = floor(cDenormalized);
	vec3 cFractional = cDenormalized - cLow;
	vec3 cDithered = cLow + step(threshold(), cFractional);
	return cDithered / depth;
}

// Fast zero-pow() Filmic highlight roll-off + gentle micro-contrast tuned for Elite Force:
// uses x^2 / sqrt(x) gamma approximation to keep QPU utilization high on VideoCore VII.
vec3 filmicTonemapFast(vec3 srgb) {
	vec3 c = max(srgb, vec3(0.0));
	vec3 color = c * c;

	// Subtle filmic S-curve contrast without crushing deep Borg shadows
	vec3 cl = clamp(color, 0.0, 1.0);
	vec3 contrast = cl * cl * (3.0 - 2.0 * cl);
	color = mix(color, contrast, 0.16);

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

	return sqrt(max(color, vec3(0.0)));
}

void main() {
	vec3 base = subpassLoad(inputColor).rgb;

	if ( greyscale == 1.0 ) {
		base = vec3(dot(base, sRGB));
	} else if ( greyscale != 0.0 ) {
		vec3 luma = vec3(dot(base, sRGB));
		base = mix(base, luma, greyscale);
	}

	if ( gamma != 1.0 ) {
		base = pow(max(base, vec3(0.0)), vec3(gamma)) * obScale;
	} else if ( obScale != 1.0 ) {
		base = base * obScale;
	}

	if ( tonemapMode == 1 ) {
		base = filmicTonemapFast(base);
	}

	if ( vignetteStrength > 0.0 ) {
		vec2 vuv = frag_tex_coord * 2.0 - 1.0;
		float vig = 1.0 - vignetteStrength * smoothstep(0.45, 1.55, dot(vuv, vuv));
		base *= vig;
	}

	if ( ditherMode == 1 ) {
		base = dither(clamp(base, 0.0, 1.0));
	}

	out_color = vec4(base, 1.0);
}
