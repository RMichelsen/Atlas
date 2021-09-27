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

// The four texels are in order:
// xyzw, x = top_left, y = top_right, z = bot_left, w = bot_right
vec3 get_subpixel_contributions(vec2 weight, uvec4 texels) {
	if(texels == uvec4(0)) {
		return vec3(0.0);
	}

	uint64_t rows[] = {
		(bitfieldExtract(texels.x, 0, 4) << 4)  | bitfieldExtract(texels.y, 0, 4),
		(bitfieldExtract(texels.x, 4, 4) << 4)  | bitfieldExtract(texels.y, 4, 4),
		(bitfieldExtract(texels.x, 8, 4) << 4)  | bitfieldExtract(texels.y, 8, 4),
		(bitfieldExtract(texels.x, 12, 4) << 4) | bitfieldExtract(texels.y, 12, 4),
		(bitfieldExtract(texels.z, 0, 4) << 4)  | bitfieldExtract(texels.w, 0, 4),
		(bitfieldExtract(texels.z, 4, 4) << 4)  | bitfieldExtract(texels.w, 4, 4),
		(bitfieldExtract(texels.z, 8, 4) << 4)  | bitfieldExtract(texels.w, 8, 4),
		(bitfieldExtract(texels.z, 12, 4) << 4) | bitfieldExtract(texels.w, 12, 4)
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

	vec3 subpixel_left = get_subpixel_contributions(weight, left_side_texel_values.wzxy);
	vec3 subpixel_middle = get_subpixel_contributions(weight,
		uvec4(left_side_texel_values.z, right_side_texel_values.w,
			  left_side_texel_values.y, right_side_texel_values.x)
	);
	vec3 subpixel_right = get_subpixel_contributions(weight, right_side_texel_values.wzxy);

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

	float gamma = 1.8;
	vec3 raw_result = mix(pow(bg_color, vec3(gamma)), pow(text_color, vec3(gamma)), vec3(coverage));
	vec3 gamma_corrected_result = pow(raw_result, vec3(1.0 / gamma));
	out_color = vec4(gamma_corrected_result, 1.0);
}

