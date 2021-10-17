#version 460

layout(push_constant) uniform PushConstants {
	vec2 display_size;
	float glyph_width;
	float glyph_height;
	uint cell_width;
	uint cell_height;
	uint glyph_atlas_size;
} pc;

layout(binding = 0) uniform usampler2D glyph_atlas;

layout(location = 0) in vec2 in_uv;
layout(location = 1) flat in uvec2 in_glyph_offset;

layout(location = 0) out vec4 out_color;

// Three masks for R, G, B subpixel bit counts
// R = 1111000011110000, G = 0011110000111100, B = 0000111100001111
const uvec3 masks = uvec3(
	0xF0F0,
	0x3C3C,
	0x0F0F
);

vec3 get_subpixel_contributions(uint value) {
	float r_bits = float(bitCount(value & masks[0]));
	float g_bits = float(bitCount(value & masks[1]));
	float b_bits = float(bitCount(value & masks[2]));

	return vec3(r_bits, g_bits, b_bits) / vec3(8.0);
}

void main() {
	vec2 abs_pixel_origin = in_uv * vec2(pc.glyph_width, pc.glyph_height);
	vec2 pixel_origin = in_uv * vec2(pc.glyph_width, pc.glyph_height) + in_glyph_offset * vec2(pc.cell_width, pc.cell_height);

	vec3 subpixel_left = vec3(0.0);
	vec3 subpixel_right = vec3(0.0);

	uint middle_value = texture(glyph_atlas, (pixel_origin + vec2(0.0, 0.0)) / pc.glyph_atlas_size).x;
	vec3 subpixel_middle = get_subpixel_contributions(middle_value);

	if(abs_pixel_origin.x > 1.0) {
        uint left_value = texture(glyph_atlas, (pixel_origin + vec2(-1.0, 0.0)) / pc.glyph_atlas_size).x;
        subpixel_left = get_subpixel_contributions(left_value);
	}

	if(abs_pixel_origin.x < (pc.glyph_width - 1.0)) {
        uint right_value = texture(glyph_atlas, (pixel_origin + vec2(1.0, 0.0)) / pc.glyph_atlas_size).x;
        subpixel_right = get_subpixel_contributions(right_value);
	}

	float R = dot(subpixel_left.yz, vec2(0.11111, 0.22222)) +
			  dot(subpixel_middle.xyz, vec3(0.33333, 0.22222, 0.11111));

	float G = subpixel_left.z * 0.11111 + 
	          dot(subpixel_middle.xyz, vec3(0.22222, 0.33333, 0.22222)) +
			  subpixel_right.x * 0.11111;

	float B = dot(subpixel_middle.xyz, vec3(0.11111, 0.22222, 0.33333)) +
			  dot(subpixel_right.xy, vec2(0.22222, 0.11111));

	vec3 coverage = vec3(R, G, B);

	vec3 text_color = vec3(0.83137, 0.83137, 0.83137);
	vec3 bg_color = vec3(0.117647, 0.117647, 0.117647);

	float gamma = 1.43;
	vec3 raw_result = mix(pow(bg_color, vec3(gamma)), pow(text_color, vec3(gamma)), vec3(coverage));
	vec3 gamma_corrected_result = pow(raw_result, vec3(1.0 / gamma));
	out_color = vec4(gamma_corrected_result, 1.0);
}

