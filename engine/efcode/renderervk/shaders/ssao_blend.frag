#version 450

layout(set = 0, binding = 0) uniform sampler2D ssaoTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

// Multiplicative composite onto vk.color_image (src=ZERO, dst=SRC_COLOR -> dst *= ao).
void main() {
	float ao = texture(ssaoTex, frag_tex_coord).r;
	out_color = vec4(ao, ao, ao, 1.0);
}
