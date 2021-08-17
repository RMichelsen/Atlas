#pragma once

//#version 460
//
//layout(location = 0) in vec2 in_pos;
//
//void main() {
//	gl_Position = vec4(in_pos, 0.0f, 1.0f);
//}
//glslangValidator.exe --target-env vulkan1.2 -x vertex.vert
constexpr unsigned int VERTEX_SHADER_BYTECODE[] = {
	// 1011.5.0
	0x07230203,0x00010500,0x0008000a,0x0000001b,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0007000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000d,0x00000012,0x00030003,
	0x00000002,0x000001cc,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00060005,0x0000000b,
	0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,0x0000000b,0x00000000,0x505f6c67,
	0x7469736f,0x006e6f69,0x00070006,0x0000000b,0x00000001,0x505f6c67,0x746e696f,0x657a6953,
	0x00000000,0x00070006,0x0000000b,0x00000002,0x435f6c67,0x4470696c,0x61747369,0x0065636e,
	0x00070006,0x0000000b,0x00000003,0x435f6c67,0x446c6c75,0x61747369,0x0065636e,0x00030005,
	0x0000000d,0x00000000,0x00040005,0x00000012,0x705f6e69,0x0000736f,0x00050048,0x0000000b,
	0x00000000,0x0000000b,0x00000000,0x00050048,0x0000000b,0x00000001,0x0000000b,0x00000001,
	0x00050048,0x0000000b,0x00000002,0x0000000b,0x00000003,0x00050048,0x0000000b,0x00000003,
	0x0000000b,0x00000004,0x00030047,0x0000000b,0x00000002,0x00040047,0x00000012,0x0000001e,
	0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,
	0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040015,0x00000008,0x00000020,
	0x00000000,0x0004002b,0x00000008,0x00000009,0x00000001,0x0004001c,0x0000000a,0x00000006,
	0x00000009,0x0006001e,0x0000000b,0x00000007,0x00000006,0x0000000a,0x0000000a,0x00040020,
	0x0000000c,0x00000003,0x0000000b,0x0004003b,0x0000000c,0x0000000d,0x00000003,0x00040015,
	0x0000000e,0x00000020,0x00000001,0x0004002b,0x0000000e,0x0000000f,0x00000000,0x00040017,
	0x00000010,0x00000006,0x00000002,0x00040020,0x00000011,0x00000001,0x00000010,0x0004003b,
	0x00000011,0x00000012,0x00000001,0x0004002b,0x00000006,0x00000014,0x00000000,0x0004002b,
	0x00000006,0x00000015,0x3f800000,0x00040020,0x00000019,0x00000003,0x00000007,0x00050036,
	0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,0x00000010,
	0x00000013,0x00000012,0x00050051,0x00000006,0x00000016,0x00000013,0x00000000,0x00050051,
	0x00000006,0x00000017,0x00000013,0x00000001,0x00070050,0x00000007,0x00000018,0x00000016,
	0x00000017,0x00000014,0x00000015,0x00050041,0x00000019,0x0000001a,0x0000000d,0x0000000f,
	0x0003003e,0x0000001a,0x00000018,0x000100fd,0x00010038
};

//#version 460
//
//layout(location = 0) out vec4 out_color;
//
//void main() {
//	out_color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
//}
//glslangValidator.exe --target-env vulkan1.2 -x fragment.frag
constexpr unsigned int FRAGMENT_SHADER_BYTECODE[] = {
	// 1011.5.0
	0x07230203, 0x00010500, 0x0008000a, 0x0000000d, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
	0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
	0x0006000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x00030010, 0x00000004,
	0x00000007, 0x00030003, 0x00000002, 0x000001cc, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000,
	0x00050005, 0x00000009, 0x5f74756f, 0x6f6c6f63, 0x00000072, 0x00040047, 0x00000009, 0x0000001e,
	0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006,
	0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020, 0x00000008, 0x00000003,
	0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000003, 0x0004002b, 0x00000006, 0x0000000a,
	0x3f800000, 0x0004002b, 0x00000006, 0x0000000b, 0x00000000, 0x0007002c, 0x00000007, 0x0000000c,
	0x0000000a, 0x0000000b, 0x0000000b, 0x0000000a, 0x00050036, 0x00000002, 0x00000004, 0x00000000,
	0x00000003, 0x000200f8, 0x00000005, 0x0003003e, 0x00000009, 0x0000000c, 0x000100fd, 0x00010038
};

//#version 460
//#extension GL_EXT_debug_printf : require
//
//#define MAX_LINES_PER_GLYPH 4096;
//#define NUM_PRINTABLE_CHARS 95;
//
//struct Line {
//	vec2 p1;
//	vec2 p2;
//};
//
//struct GlyphOffset {
//	uint offset;
//	uint num_lines;
//	vec2 padding;
//};
//
//layout(local_size_x = 1, local_size_y = 1) in;
//
//layout(binding = 0, r16ui) writeonly uniform uimage2D glyph_atlas;
//layout(binding = 1, std140) readonly buffer GlyphLines {
//	Line data[];
//} glyph_lines;
//layout(binding = 2, std140) readonly buffer GlyphOffsets {
//	GlyphOffset data[];
//} glyph_offsets;
//
//layout(push_constant) uniform PushConstants {
//	int glyph_width;
//	int glyph_height;
//	int ascent;
//	int descent;
//	uint num_glyphs;
//} pc;
//
//void main() {
//	int glyph_atlas_size = imageSize(glyph_atlas).x;
//	uint num_glyphs_per_row = glyph_atlas_size / pc.glyph_width;
//	uint x = uint(gl_GlobalInvocationID.x / pc.glyph_width);
//	uint y = uint(gl_GlobalInvocationID.y / pc.glyph_height);
//	uint char_index = y * num_glyphs_per_row + x;
//	if(char_index >= pc.num_glyphs) {
//		return;
//	}
//
//	vec2 pixel_top_left = {
//		gl_GlobalInvocationID.x % pc.glyph_width,
//		gl_GlobalInvocationID.y % pc.glyph_height
//	};
//
//	uint result = 0;
//	for(int i = 0; i < glyph_offsets.data[char_index].num_lines; ++i) {
//		Line l = glyph_lines.data[glyph_offsets.data[char_index].offset + i];
//		vec2 p1 = l.p1;
//		vec2 p2 = l.p2;
//
//		for(int x = 0; x < 4; ++x) {
//			for(int y = 0; y < 4; ++y) {
//				vec2 p = pixel_top_left + vec2(x / 4.0f, y / 4.0f);
//
//				if(p.y < p2.y && p.y >= p1.y) {
//					float dx = p2.x - p1.x;
//					float dy = p2.y - p1.y;
//
//					if(dy == 0.0f) {
//						continue;
//					}
//
//					float line_x = (p.y - p1.y) * (dx / dy) + p1.x;
//
//					if(p.x > line_x) {
//						uint mask = 1 << ((3 - x) + y * 4);
//						result ^= mask;
//					}
//				}
//			}
//		}
//	}
//
//	imageStore(glyph_atlas, ivec2(gl_GlobalInvocationID.xy), uvec4(result, 0, 0, 0));
//}
//glslangValidator.exe --target-env vulkan1.2 -x raster.comp
constexpr unsigned int COMPUTE_SHADER_BYTECODE[] = {
#include "C:\Users\Rasmus\Desktop\comp.spv"
};

