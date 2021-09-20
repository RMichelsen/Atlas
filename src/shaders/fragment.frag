#version 460
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float64 : enable
#extension GL_EXT_debug_printf : enable

layout(push_constant) uniform PushConstants {
	vec2 display_size;
	float glyph_width;
	float glyph_height;
	float glyph_width_to_height_ratio;
	float font_size;
} pc;

layout(binding = 0) uniform usampler2D glyph_atlas;

layout(location = 0) in vec2 in_uv;
layout(location = 1) flat in uvec2 in_glyph_offset;

layout(location = 0) out vec4 out_color;

void main() {
	vec2 pixel_origin = in_uv * vec2(pc.glyph_width, pc.glyph_height) + in_glyph_offset * vec2(pc.glyph_width, pc.glyph_height);
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

	uint64_t x_mask = 0xF0 >> int(round(weight.x * 4.0f));

	int start_row = int(floor(weight.y * 4.0f));
	int end_row = start_row + 4;
	int64_t bits = 0;
	for(int i = start_row; i < end_row; ++i) {
		bits += bitCount(x_mask & rows[i]);
	}

	double coverage = double(bits) / 16.0;
	vec3 text_color = vec3(0.83137f, 0.83137f, 0.83137f);
	vec3 bg_color = vec3(0.117647f, 0.117647f, 0.117647f);

	float gamma = 1.8f;
	vec3 raw_result = mix(pow(bg_color, vec3(gamma)), pow(text_color, vec3(gamma)), vec3(coverage));
	vec3 gamma_corrected_result = pow(raw_result, vec3(1.0f / gamma));
	out_color = vec4(gamma_corrected_result, 1.0f);
}