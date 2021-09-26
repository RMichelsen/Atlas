#version 460

const vec2 uv_coords[] = {
	vec2(0.0f, 0.0f),
	vec2(0.0f, 1.0f),
	vec2(1.0f, 1.0f),
	vec2(0.0f, 0.0f),
	vec2(1.0f, 1.0f),
	vec2(1.0f, 0.0f)
};

layout(push_constant) uniform PushConstants {
	vec2 display_size;
	float glyph_width;
	float glyph_height;
	uint glyph_atlas_width;
	uint glyph_atlas_height;
} pc;

layout(location = 0) in uint in_vertex;
layout(location = 0) out vec2 out_uv;
layout(location = 1) flat out uvec2 out_glyph_offset;

void main() {
	uint pos = bitfieldExtract(in_vertex, 0, 3);
	vec2 position = vec2(
		(pos == 2 || pos > 3) ? pc.glyph_width : 0.0f,
		(pos == 1 || pos == 2 || pos == 4) ? pc.glyph_height : 0.0f
	);
	uvec2 cell_offset = uvec2(bitfieldExtract(in_vertex, 19, 8), bitfieldExtract(in_vertex, 27, 5));
	vec2 pos_offset = vec2(pc.glyph_width, pc.glyph_atlas_height) * cell_offset;

	gl_Position = vec4((position + pos_offset) * vec2(2.0f / pc.display_size.x, 2.0f / pc.display_size.y) - vec2(1.0f), 0.0f, 1.0f);  
	out_uv = uv_coords[bitfieldExtract(in_vertex, 3, 3)]; 
	out_glyph_offset = uvec2(bitfieldExtract(in_vertex, 6, 8), bitfieldExtract(in_vertex, 14, 5)); 
}