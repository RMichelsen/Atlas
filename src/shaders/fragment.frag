#version 460
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float64 : enable
#extension GL_EXT_control_flow_attributes : enable
#extension GL_EXT_debug_printf : enable

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

vec3 get_subpixel_contributions(vec2 weight, uint top_left, uint top_right, uint bot_left, uint bot_right) {
	if(top_left == 0 && top_right == 0 && bot_left == 0 && bot_right == 0) {
		return vec3(0.0);
	}

	uint64_t rows[] = {
		(bitfieldExtract(top_left, 0, 4) << 4)  | bitfieldExtract(top_right, 0, 4),
		(bitfieldExtract(top_left, 4, 4) << 4)  | bitfieldExtract(top_right, 4, 4),
		(bitfieldExtract(top_left, 8, 4) << 4)  | bitfieldExtract(top_right, 8, 4),
		(bitfieldExtract(top_left, 12, 4) << 4) | bitfieldExtract(top_right, 12, 4),
		(bitfieldExtract(bot_left, 0, 4) << 4)  | bitfieldExtract(bot_right, 0, 4),
		(bitfieldExtract(bot_left, 4, 4) << 4)  | bitfieldExtract(bot_right, 4, 4),
		(bitfieldExtract(bot_left, 8, 4) << 4)  | bitfieldExtract(bot_right, 8, 4),
		(bitfieldExtract(bot_left, 12, 4) << 4) | bitfieldExtract(bot_right, 12, 4)
	};

	// Three masks for R, G, B subpixel bit counts
	// R = 11000000, B = 01100000, B = 00110000
	int start_col = int(round(weight.x * 4.0));
	u64vec3 masks = u64vec3(
		0xC0 >> start_col,
		0x60 >> start_col,
		0x30 >> start_col
	);

	int start_row = int(floor(weight.y * 4.0));
	int end_row = start_row + 4;
	i64vec3 bits = i64vec3(0);
	for(int i = start_row; i < end_row; ++i) {
		bits += bitCount(masks & rows[i]);
	}

	return vec3(vec3(bits) / vec3(8.0));
}

void main() {
	vec2 pixel_origin = in_uv * vec2(pc.glyph_width, pc.glyph_height) + in_glyph_offset * vec2(pc.cell_width, pc.cell_height);
	vec2 texel_origin = floor(pixel_origin);
	vec2 weight = pixel_origin - texel_origin;

	vec2 texel_center = texel_origin + vec2(1.0);
	uvec4 left_side_texel_values = textureGather(glyph_atlas, (texel_center - vec2(1.0, 0.0)) / pc.glyph_atlas_size);
	uvec4 right_side_texel_values = textureGather(glyph_atlas, (texel_center + vec2(1.0, 0.0)) / pc.glyph_atlas_size);

	uint left_top_right = left_side_texel_values.z;
	uint left_top_left = left_side_texel_values.w;
	uint left_bot_left = left_side_texel_values.x;
	uint left_bot_right = left_side_texel_values.y;

	uint right_top_right = right_side_texel_values.z;
	uint right_top_left = right_side_texel_values.w;
	uint right_bot_left = right_side_texel_values.x;
	uint right_bot_right = right_side_texel_values.y;

	vec3 subpixel_left = get_subpixel_contributions(weight, left_top_left, left_top_right, left_bot_left, left_bot_right);
	vec3 subpixel_middle = get_subpixel_contributions(weight, left_top_right, right_top_left, left_bot_right, right_bot_left);
	vec3 subpixel_right = get_subpixel_contributions(weight, right_top_left, right_top_right, right_bot_left, right_bot_right);

	float R = subpixel_left.y * 0.11111 +
		subpixel_left.z * 0.22222 +
		subpixel_middle.x * 0.33333 +
		subpixel_middle.y * 0.22222 +
		subpixel_middle.z * 0.11111;

	float G = subpixel_left.z * 0.11111 +
		subpixel_middle.x * 0.22222 +
		subpixel_middle.y * 0.33333 +
		subpixel_middle.z * 0.22222 +
		subpixel_right.x * 0.11111;

	float B = subpixel_middle.x * 0.11111 +
		subpixel_middle.y * 0.22222 +
		subpixel_middle.z * 0.33333 +
		subpixel_right.x * 0.22222 +
		subpixel_right.y * 0.11111;

	vec3 coverage = vec3(R, G, B);

	vec3 text_color = vec3(0.83137, 0.83137, 0.83137);
	vec3 bg_color = vec3(0.117647, 0.117647, 0.117647);

	float gamma = 1.8;
	vec3 raw_result = mix(pow(bg_color, vec3(gamma)), pow(text_color, vec3(gamma)), vec3(coverage));
	vec3 gamma_corrected_result = pow(raw_result, vec3(1.0 / gamma));
	out_color = vec4(gamma_corrected_result, 1.0);
}

