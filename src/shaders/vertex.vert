#version 460

const vec2 positions[] = {
	vec2(0.0, 0.0),
	vec2(0.0, 1.0),
	vec2(1.0, 1.0),
	vec2(0.0, 0.0),
	vec2(1.0, 1.0),
	vec2(1.0, 0.0)
};

const vec2 uv_coords[] = {
	vec2(0.0, 0.0),
	vec2(0.0, 1.0),
	vec2(1.0, 1.0),
	vec2(0.0, 0.0),
	vec2(1.0, 1.0),
	vec2(1.0, 0.0)
};

layout(push_constant) uniform PushConstants {
	vec2 display_size;
	float glyph_width;
	float glyph_height;
	uint cell_width;
	uint cell_height;
	uint glyph_atlas_size;
} pc;

layout(location = 0) in uvec2 in_vertex;
layout(location = 0) out vec2 out_uv;
layout(location = 1) flat out uvec2 out_glyph_offset;

void main() {
	vec2 pos = positions[bitfieldExtract(in_vertex.x, 0, 3)] * vec2(pc.glyph_width / 3.0f, pc.glyph_height);
	vec2 offset = vec2(bitfieldExtract(in_vertex.y, 0, 16), bitfieldExtract(in_vertex.y, 16, 16)) * vec2(pc.glyph_width / 3.0f, pc.glyph_height);
	gl_Position = vec4((pos + offset) * vec2(2.0 / pc.display_size.x, 2.0 / pc.display_size.y) - vec2(1.0), 0.0, 1.0);  

	out_uv = uv_coords[bitfieldExtract(in_vertex.x, 3, 3)]; 
	out_glyph_offset = uvec2(bitfieldExtract(in_vertex.x, 6, 13), bitfieldExtract(in_vertex.x, 19, 13)); 
}

