#version 460

const uint R_left_masks[] = {
	0xCCCC,
	0x6666,
	0x3333,
	0x1111,
	0x0000
};
const uint R_right_masks[] = {
	0x0000,
	0x0000,
	0x0000,
	0x8888,
	0xCCCC
};
const uint G_left_masks[] = {
	0x6666,
	0x3333,
	0x1111,
	0x0000,
	0x0000
};
const uint G_right_masks[] = {
	0x0000,
	0x0000,
	0x8888,
	0xcccc,
	0x6666
};
const uint B_left_masks[] = {
	0x3333,
	0x1111,
	0x0000,
	0x0000,
	0x0000
};
const uint B_right_masks[] = {
	0x0000,
	0x8888,
	0xCCCC,
	0x6666,
	0x3333
};

layout(push_constant) uniform PushConstants {
	int glyph_width;
	int glyph_height;
	float glyph_width_to_height_ratio;
	float font_size;
} pc;

layout(binding = 0) uniform usampler2D glyph_atlas;

layout(location = 0) in vec2 in_uv;
layout(location = 1) flat in uvec2 in_glyph_offset;
layout(location = 0) out vec4 out_color;

vec3 get_subpixel_contributions(vec2 pixel_pos_left_corner) {
	vec2 texel_pos_left_corner = floor(pixel_pos_left_corner);
	vec2 weight = pixel_pos_left_corner - texel_pos_left_corner;
	
	vec2 glyph_atlas_size = textureSize(glyph_atlas, 0);
	vec2 texel_center = (texel_pos_left_corner + vec2(1.0f, 1.0f)) / glyph_atlas_size;
	uvec4 texel_values = textureGather(glyph_atlas, texel_center);

	int horizontal_index = int(round(weight.x * 4.0f));
	uint R_left_mask = R_left_masks[horizontal_index];
	uint R_right_mask = R_right_masks[horizontal_index];
	uint G_left_mask = G_left_masks[horizontal_index];
	uint G_right_mask = G_right_masks[horizontal_index];
	uint B_left_mask = B_left_masks[horizontal_index];
	uint B_right_mask = B_right_masks[horizontal_index];
	uint top_left = texel_values.x;
	uint top_right = texel_values.y;
	uint bot_right = texel_values.z;
	uint bot_left = texel_values.w;
	uint R_left = ((top_left & R_left_mask) << 16) | (bot_left & R_left_mask);
	uint R_right = ((top_right & R_right_mask) << 16) | (bot_right & R_right_mask);
	uint G_left = ((top_left & G_left_mask) << 16) | (bot_left & G_left_mask);
	uint G_right = ((top_right & G_right_mask) << 16) | (bot_right & G_right_mask);
	uint B_left = ((top_left & B_left_mask) << 16) | (bot_left & B_left_mask);
	uint B_right = ((top_right & B_right_mask) << 16) | (bot_right & B_right_mask);
	int shift_down = int(round(weight.y * 4.0f)) * 4;
	R_left = (R_left >> shift_down) & 0xFFFF;
	R_right = (R_right >> shift_down) & 0xFFFF;
	G_left = (G_left >> shift_down) & 0xFFFF;
	G_right = (G_right >> shift_down) & 0xFFFF;
	B_left = (B_left >> shift_down) & 0xFFFF;
	B_right = (B_right >> shift_down) & 0xFFFF;
	
	float R_alpha = (bitCount(R_left) + bitCount(R_right)) * 0.125f;
	float G_alpha = (bitCount(G_left) + bitCount(G_right)) * 0.125f;
	float B_alpha = (bitCount(B_left) + bitCount(B_right)) * 0.125f;
	
	return vec3(R_alpha, G_alpha, B_alpha);
}

void main() {
	vec2 pixel_pos_left_corner = in_uv * vec2(pc.glyph_width, pc.glyph_height) + in_glyph_offset * vec2(pc.glyph_width, pc.glyph_height);
	
	vec3 text_color = vec3(0.83137f, 0.83137f, 0.83137f);
	vec3 bg_color = vec3(0.117647f, 0.117647f, 0.117647f);
	vec3 subpixel_left = get_subpixel_contributions(pixel_pos_left_corner + vec2(-1.0f, 0.0f));
	vec3 subpixel_middle = get_subpixel_contributions(pixel_pos_left_corner);
	vec3 subpixel_right = get_subpixel_contributions(pixel_pos_left_corner + vec2(1.0f, 0.0f));
	float R_alpha = subpixel_left.y * 0.11111f +
					subpixel_left.z * 0.22222f +
					subpixel_middle.x * 0.33333f +
					subpixel_middle.y * 0.22222f +
					subpixel_middle.z * 0.11111f;
	float G_alpha = subpixel_left.z * 0.11111f +
					subpixel_middle.x * 0.22222f +
					subpixel_middle.y * 0.33333f +
					subpixel_middle.z * 0.22222f +
					subpixel_right.x * 0.11111f;
	float B_alpha = subpixel_middle.x * 0.11111f +
					subpixel_middle.y * 0.22222f +
					subpixel_middle.z * 0.33333f +
					subpixel_right.x * 0.22222f +
					subpixel_right.y * 0.11111f;
	vec3 subpixel_alphas = vec3(R_alpha, G_alpha, B_alpha);
	
	out_color = vec4(mix(bg_color, text_color, subpixel_alphas), 1.0f);
}