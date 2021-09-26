#version 460
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float64 : enable
#extension GL_EXT_debug_printf : enable

layout(push_constant) uniform PushConstants {
	vec2 display_size;
	float glyph_width;
	float glyph_height;
	uint glyph_atlas_width;
	uint glyph_atlas_height;
} pc;

layout(binding = 0) uniform usampler2D glyph_atlas;

layout(location = 0) in vec2 in_uv;
layout(location = 1) flat in uvec2 in_glyph_offset;

layout(location = 0) out vec4 out_color;

vec3 get_subpixel_contributions(vec2 pixel_origin) {
	vec2 texel_origin = floor(pixel_origin);
	vec2 weight = pixel_origin - texel_origin;

	vec2 glyph_atlas_size = textureSize(glyph_atlas, 0);
	vec2 texel_center = (texel_origin + vec2(1.0f)) / glyph_atlas_size;
	uvec4 texel_values = textureGather(glyph_atlas, texel_center);

	uint top_right = texel_values.z;
	uint top_left = texel_values.w;
	uint bot_left = texel_values.x;
	uint bot_right = texel_values.y;

	uint64_t row1 = (bitfieldExtract(top_left, 0, 4) << 4)  | bitfieldExtract(top_right, 0, 4);
	uint64_t row2 = (bitfieldExtract(top_left, 4, 4) << 4)  | bitfieldExtract(top_right, 4, 4);
	uint64_t row3 = (bitfieldExtract(top_left, 8, 4) << 4)  | bitfieldExtract(top_right, 8, 4);
	uint64_t row4 = (bitfieldExtract(top_left, 12, 4) << 4) | bitfieldExtract(top_right, 12, 4);
	uint64_t row5 = (bitfieldExtract(bot_left, 0, 4) << 4)  | bitfieldExtract(bot_right, 0, 4);
	uint64_t row6 = (bitfieldExtract(bot_left, 4, 4) << 4)  | bitfieldExtract(bot_right, 4, 4);
	uint64_t row7 = (bitfieldExtract(bot_left, 8, 4) << 4)  | bitfieldExtract(bot_right, 8, 4);
	uint64_t row8 = (bitfieldExtract(bot_left, 12, 4) << 4) | bitfieldExtract(bot_right, 12, 4);
	uint64_t rows[] = {
		row1, row2, row3, row4, row5, row6, row7, row8
	};

	uint64_t r_mask = 0xC0 >> int(round(weight.x * 4.0f));
	uint64_t g_mask = 0x60 >> int(round(weight.x * 4.0f));
	uint64_t b_mask = 0x30 >> int(round(weight.x * 4.0f));

	int start_row = int(floor(weight.y * 4.0f));
	int end_row = start_row + 4;
	int64_t r_bits = 0;
	int64_t g_bits = 0;
	int64_t b_bits = 0;
	for(int i = start_row; i < end_row; ++i) {
		r_bits += bitCount(r_mask & rows[i]);
		g_bits += bitCount(g_mask & rows[i]);
		b_bits += bitCount(b_mask & rows[i]);
	}

	return vec3(double(r_bits) / 8.0f, double(g_bits) / 8.0f, double(b_bits) / 8.0f);
}

void main() {
	vec2 pixel_origin = in_uv * vec2(pc.glyph_width, pc.glyph_height) + in_glyph_offset * vec2(pc.glyph_atlas_width, pc.glyph_atlas_height);

	vec2 texel_origin = floor(pixel_origin);
	vec2 weight = pixel_origin - texel_origin;

	vec3 subpixel_left = get_subpixel_contributions(pixel_origin + vec2(-1.0f, 0.0f));
	vec3 subpixel_middle = get_subpixel_contributions(pixel_origin);
	vec3 subpixel_right = get_subpixel_contributions(pixel_origin + vec2(1.0f, 0.0f));

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

	vec3 text_color = vec3(0.83137f, 0.83137f, 0.83137f);
	vec3 bg_color = vec3(0.117647f, 0.117647f, 0.117647f);

	float gamma = 1.8f;
	vec3 raw_result = mix(pow(bg_color, vec3(gamma)), pow(text_color, vec3(gamma)), vec3(coverage));
	vec3 gamma_corrected_result = pow(raw_result, vec3(1.0f / gamma));
	out_color = vec4(gamma_corrected_result, 1.0f);
}

