#version 450

layout(set = 0, binding = 0) uniform sampler2D ssaoTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

// Depth-aware 5x5 cross-bilateral filter: removes Vogel-disk noise while keeping
// architectural & character edges razor-sharp.
void main() {
	vec2 invSize = 1.0 / vec2(textureSize(ssaoTex, 0));
	vec2 center = texture(ssaoTex, frag_tex_coord).rg;
	float aoSum = center.r * 4.0;
	float wSum = 4.0;
	float d0 = center.g;
	float depthScale = 1.0 / max(d0 * 0.035, 3.0);

	for (int y = -2; y <= 2; ++y) {
		for (int x = -2; x <= 2; ++x) {
			if (x == 0 && y == 0) {
				continue;
			}
			vec2 s = texture(ssaoTex, frag_tex_coord + vec2(float(x), float(y)) * invSize).rg;
			float spatialW = exp(-float(x * x + y * y) * 0.22);
			float depthW = exp(-abs(s.g - d0) * depthScale);
			float w = spatialW * depthW;
			aoSum += s.r * w;
			wSum += w;
		}
	}

	out_color = vec4(aoSum / wSum, d0, 0.0, 1.0);
}
