#version 450

#ifdef USE_MSAA
layout(set = 0, binding = 0) uniform sampler2DMS depthTex;
#else
layout(set = 0, binding = 0) uniform sampler2D depthTex;
#endif

layout(push_constant) uniform SsaoParams {
	float zNear;
	float zFar;
	float tanHalfFovX;
	float tanHalfFovY;
} pc;

layout(constant_id = 13) const float ssaoIntensity = 0.75;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

float fetchRawDepth(ivec2 ic, ivec2 texSize) {
	ivec2 c = clamp(ic, ivec2(0), texSize - ivec2(1));
	return texelFetch(depthTex, c, 0).r;
}

vec2 uvFromTexel(ivec2 ic, vec2 invSize) {
	return (vec2(ic) + 0.5) * invSize;
}

// Reversed-Z Vulkan perspective linearization matching tr_main.c:
// P[10] = zNear / (zFar - zNear), P[14] = zFar * zNear / (zFar - zNear)
float linearizeDepth(float z) {
	float denom = pc.zNear + z * (pc.zFar - pc.zNear);
	return (pc.zFar * pc.zNear) / max(denom, 1e-6);
}

vec3 viewPosFromUV(vec2 uv, float d) {
	vec2 ndc = uv * 2.0 - 1.0;
	return vec3(ndc.x * pc.tanHalfFovX * d, ndc.y * pc.tanHalfFovY * d, d);
}

void main() {
#ifdef USE_MSAA
	ivec2 texSize = textureSize(depthTex);
#else
	ivec2 texSize = textureSize(depthTex, 0);
#endif
	vec2 invSize = 1.0 / vec2(texSize);

	// Snap to exact integer depth-texel coordinates before stepping to neighbors.
	// Using float UV +/- invSize with texelFetch(ivec2(uv * texSize)) suffers IEEE-754
	// boundary truncation (167/540 broken scanlines at 1080p) where icD == ic0, collapsing
	// dPdy and tilting the reconstructed normal toward the camera (horizontal floor bands).
	ivec2 ic0 = clamp(ivec2(floor(frag_tex_coord * vec2(texSize))), ivec2(0), texSize - ivec2(1));
	vec2 uv0 = uvFromTexel(ic0, invSize);

	float z0 = fetchRawDepth(ic0, texSize);
	if (z0 <= 1e-5) {
		// Skybox / far plane: no ambient occlusion
		out_color = vec4(1.0, pc.zFar, 0.0, 1.0);
		return;
	}

	float d0 = linearizeDepth(z0);
	if (d0 < 3.0) {
		// Keep ultra-near viewmodel pixels clean
		out_color = vec4(1.0, d0, 0.0, 1.0);
		return;
	}

	vec3 p0 = viewPosFromUV(uv0, d0);

	// Edge-preserving view-space normal reconstruction using exact +/-1 integer texel steps
	ivec2 icL = clamp(ic0 + ivec2(-1, 0), ivec2(0), texSize - ivec2(1));
	ivec2 icR = clamp(ic0 + ivec2( 1, 0), ivec2(0), texSize - ivec2(1));
	ivec2 icU = clamp(ic0 + ivec2( 0,-1), ivec2(0), texSize - ivec2(1));
	ivec2 icD = clamp(ic0 + ivec2( 0, 1), ivec2(0), texSize - ivec2(1));

	float dL = linearizeDepth(fetchRawDepth(icL, texSize));
	float dR = linearizeDepth(fetchRawDepth(icR, texSize));
	float dU = linearizeDepth(fetchRawDepth(icU, texSize));
	float dD = linearizeDepth(fetchRawDepth(icD, texSize));

	vec3 dPdx = (abs(dR - d0) < abs(dL - d0))
		? (viewPosFromUV(uvFromTexel(icR, invSize), dR) - p0)
		: (p0 - viewPosFromUV(uvFromTexel(icL, invSize), dL));
	vec3 dPdy = (abs(dD - d0) < abs(dU - d0))
		? (viewPosFromUV(uvFromTexel(icD, invSize), dD) - p0)
		: (p0 - viewPosFromUV(uvFromTexel(icU, invSize), dU));

	vec3 n = normalize(cross(dPdy, dPdx));
	// Ensure normal faces the camera (-Z in our +Z-forward eye-depth convention)
	if (n.z > 0.0) {
		n = -n;
	}

	// World-space radius in Quake units (~22 units = natural architectural corners/crevices)
	const float worldRadius = 22.0;
	vec2 uvRadius = vec2(worldRadius / (d0 * pc.tanHalfFovX * 2.0),
	                     worldRadius / (d0 * pc.tanHalfFovY * 2.0));
	uvRadius = clamp(uvRadius, invSize * 2.0, vec2(0.055));

	// Per-pixel Interleaved Gradient Noise rotation (no noise texture required)
	float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
	float baseAngle = noise * 6.2831853;

	const int SAMPLES = 12;
	const float GOLDEN_ANGLE = 2.39996323;
	float bias = min(0.08 + d0 * 0.0006, 0.28);

	float occ = 0.0;
	for (int i = 0; i < SAMPLES; ++i) {
		float fi = (float(i) + 0.5) / float(SAMPLES);
		float r = sqrt(fi);
		float theta = float(i) * GOLDEN_ANGLE + baseAngle;
		vec2 offset = vec2(cos(theta), sin(theta)) * (r * uvRadius);
		ivec2 sic = clamp(ivec2(floor((uv0 + offset) * vec2(texSize))), ivec2(0), texSize - ivec2(1));
		if (sic == ic0) {
			continue;
		}

		float sz = fetchRawDepth(sic, texSize);
		if (sz <= 1e-5) {
			continue;
		}
		float sd = linearizeDepth(sz);
		vec3 sp = viewPosFromUV(uvFromTexel(sic, invSize), sd);
		vec3 v = sp - p0;
		float dist2 = dot(v, v);
		if (dist2 < 1e-4) {
			continue;
		}
		float invDist = inversesqrt(dist2);
		float dist = dist2 * invDist;
		float ndotv = dot(n, v * invDist);

		// Range falloff prevents dark halos across large foreground/background jumps
		float rangeW = smoothstep(worldRadius * 2.2, worldRadius * 0.25, dist);
		occ += max(0.0, ndotv - bias) * rangeW;
	}

	float ao = 1.0 - clamp((occ / float(SAMPLES)) * 2.2 * ssaoIntensity, 0.0, 0.85);
	// Fade out smoothly at extreme distances so distant foggy rooms stay clean
	float distFade = smoothstep(2200.0, 1200.0, d0);
	ao = mix(1.0, ao, distFade);

	out_color = vec4(ao, d0, 0.0, 1.0);
}
