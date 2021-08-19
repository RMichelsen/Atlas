#pragma once

//layout(location = 1) in vec2 in_uv;
//
//layout(location = 0) out vec2 out_uv;
//
//void main() {
//	out_uv = in_uv;
//	gl_Position = vec4(in_pos * vec2(2.0f / 2560.0f, 2.0f / 1440.0f) - vec2(1.0f), 0.0f, 1.0f);
//}
//glslangValidator.exe --target-env vulkan1.2 -x vertex.vert
constexpr unsigned int VERTEX_SHADER_BYTECODE[] = {
#include "C:\Users\Rasmus\Desktop\vert.spv"
};

//#version 460
//#extension GL_EXT_debug_printf : require
//
//layout(binding = 0) uniform usampler2D glyph_atlas;
//
//layout(location = 0) in vec2 in_uv;
//
//layout(location = 0) out vec4 out_color;
//
//uint left_masks[] = {
//	0xFFFF,
//	0x7777,
//	0x3333,
//	0x1111,
//	0x0
//};
//
//uint right_masks[] = {
//	0x0,
//	0x8888,
//	0xCCCC,
//	0xEEEE,
//	0xFFFF
//};
//
//uint left_mid_masks[] = {
//	0x3333,
//	0x3333,
//	0x3333,
//	0x1111,
//	0x0
//};
//
//uint right_mid_masks[] = {
//	0x0,
//	0x8888,
//	0xCCCC,
//	0xCCCC,
//	0xCCCC
//};
//
//vec3 get_subpixel_contributions(vec2 pixel_pos_left_corner) {
//	vec2 texel_pos_left_corner = floor(pixel_pos_left_corner);
//	vec2 weight = pixel_pos_left_corner - texel_pos_left_corner;
//
//	vec2 texel_center = (texel_pos_left_corner + vec2(1.0f, 1.0f)) / textureSize(glyph_atlas, 0);
//	uvec4 texel_values = textureGather(glyph_atlas, texel_center);
//
//	int horizontal_index = int(round(weight.x * 4.0f));
//	uint left_mask = left_masks[horizontal_index];
//	uint right_mask = right_masks[horizontal_index];
//	uint left_mid_mask = left_mid_masks[horizontal_index];
//	uint right_mid_mask = right_mid_masks[horizontal_index];
//
//	uint top_left = texel_values.x;
//	uint top_right = texel_values.y;
//	uint bot_right = texel_values.z;
//	uint bot_left = texel_values.w;
//
//	uint left = ((top_left & left_mask) << 16) | (bot_left & left_mask);
//	uint right = ((top_right & right_mask) << 16) | (bot_right & right_mask);
//	uint left_mid = ((top_left & left_mid_mask) << 16) | (bot_left & left_mid_mask);
//	uint right_mid = ((top_right & right_mid_mask) << 16) | (bot_right & right_mid_mask);
//
//	int shift_down = int(round(weight.y * 4.0f)) * 4;
//	left = (left >> shift_down) & 0xFFFF;
//	right = (right >> shift_down) & 0xFFFF;
//	left_mid = (left_mid >> shift_down) & 0xFFFF;
//	right_mid = (right_mid >> shift_down) & 0xFFFF;
//
//	float left_value = bitCount(left) / 8.0f;
//	float mid_value = (bitCount(left_mid) + bitCount(right_mid)) / 8.0f;
//	float right_value = bitCount(right) / 8.0f;
//
//	return vec3(left_value, mid_value, right_value);
//}
//
//void main() {
//	vec2 pixel_pos_left_corner = vec2(in_uv * vec2(30.0f, 55.0f) + vec2(0.0f, 55.0f) * 0.0f + vec2(30.0f, 0.0f) * 71.0f);
//
//	vec3 subpixel_left = get_subpixel_contributions(pixel_pos_left_corner + vec2(-1.0f, 0.0f));
//	vec3 subpixel_middle = get_subpixel_contributions(pixel_pos_left_corner);
//	vec3 subpixel_right = get_subpixel_contributions(pixel_pos_left_corner + vec2(1.0f, 0.0f));
//
//	float R = subpixel_left.y * 0.11111 +
//		subpixel_left.z * 0.22222 +
//		subpixel_middle.x * 0.33333 +
//		subpixel_middle.y * 0.22222 +
//		subpixel_middle.z * 0.11111;
//
//	float G = subpixel_left.z * 0.11111 +
//		subpixel_middle.x * 0.22222 +
//		subpixel_middle.y * 0.33333 +
//		subpixel_middle.z * 0.22222 +
//		subpixel_right.x * 0.11111;
//
//	float B = subpixel_middle.x * 0.11111 +
//		subpixel_middle.y * 0.22222 +
//		subpixel_middle.z * 0.33333 +
//		subpixel_right.x * 0.22222 +
//		subpixel_right.y * 0.11111;
//
//	out_color = vec4(R, G, B, 1.0f);
//}
//
//glslangValidator.exe --target-env vulkan1.2 -x fragment.frag
constexpr unsigned int FRAGMENT_SHADER_BYTECODE[] = {
#include "C:\Users\Rasmus\Desktop\frag.spv"
};

//#version 460
//#extension GL_EXT_debug_printf : require
//
//#define MAX_LINES_PER_GLYPH 4096;
//#define NUM_PRINTABLE_CHARS 95;
//
//struct Line {
//    vec2 p1;
//    vec2 p2;
//};
//
//struct GlyphOffset {
//    uint offset;
//    uint num_lines;
//    vec2 padding;
//};
//
//layout(local_size_x = 1, local_size_y = 1) in;
//
//layout(binding = 0, r16ui) writeonly uniform uimage2D glyph_atlas;
//layout(binding = 1, std140) readonly buffer GlyphLines {
//    Line data[];
//} glyph_lines;
//layout(binding = 2, std140) readonly buffer GlyphOffsets {
//    GlyphOffset data[];
//} glyph_offsets;
//
//layout(push_constant) uniform PushConstants {
//    int glyph_width;
//    int glyph_height;
//    int ascent;
//    int descent;
//    uint num_glyphs;
//} pc;
//
//void main() {
//    int glyph_atlas_size = imageSize(glyph_atlas).x;
//    uint num_glyphs_per_row = glyph_atlas_size / pc.glyph_width;
//    uint x = uint(gl_GlobalInvocationID.x / pc.glyph_width);
//    uint y = uint(gl_GlobalInvocationID.y / pc.glyph_height);
//    uint char_index = y * num_glyphs_per_row + x;
//    if(char_index >= pc.num_glyphs) {
//        return;
//    }
//
//    vec2 pixel_top_left = {
//        gl_GlobalInvocationID.x % pc.glyph_width,
//        gl_GlobalInvocationID.y % pc.glyph_height
//    };
//
//    uint result = 0;
//    for(int i = 0; i < glyph_offsets.data[char_index].num_lines; ++i) {
//        Line l = glyph_lines.data[glyph_offsets.data[char_index].offset + i];
//        vec2 p1 = l.p1;
//        vec2 p2 = l.p2;
//
//        for(int y = 0; y < 4; ++y) {
//            for(int x = 0; x < 4; ++x) {
//                vec2 p = pixel_top_left + vec2(x / 4.0f, y / 4.0f);
//
//                if(p.y < p2.y && p.y >= p1.y) {
//                    float dx = p2.x - p1.x;
//                    float dy = p2.y - p1.y;
//
//                    if(dy == 0.0f) {
//                        continue;
//                    }
//
//                    float line_x = (p.y - p1.y) * (dx / dy) + p1.x;
//
//                    if(p.x > line_x) {
//                        uint mask = 1 << ((3 - x) + y * 4);
//                        result ^= mask;
//                    }
//                }
//            }
//        }
//    }
//
//    imageStore(glyph_atlas, ivec2(gl_GlobalInvocationID.xy), uvec4(result, 0, 0, 0));
//}
//glslangValidator.exe --target-env vulkan1.2 -x raster.comp
constexpr unsigned int COMPUTE_SHADER_BYTECODE[] = {
#include "C:\Users\Rasmus\Desktop\comp.spv"
};

