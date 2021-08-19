#pragma once

constexpr const char *VERTEX_SHADER_SOURCE = R"(
	struct PSInput {
		float4 position : SV_POSITION;
		float2 uv		: TEXCOORD0;
	};

	PSInput VSMain(float2 position : POSITION0, float2 uv : TEXCOORD0) {
		PSInput result;
		result.position = float4(position * float2(2.0f / 2560.0f, 2.0f / 1440.0f) - float2(1.0f, 1.0f), 0.0f, 1.0f); 
		result.uv = uv;
		return result;
	}
)";

constexpr const char *FRAGMENT_SHADER_SOURCE = R"(
	[[vk::binding(0)]] Texture2D<uint> glyph_atlas;
	[[vk::binding(1)]] SamplerState glyph_atlas_sampler;

	struct PSInput {
		float4 position : SV_POSITION;
		float2 uv		: TEXCOORD0;
	};

	static const uint R_left_masks[] = {
		0xCCCC,
		0x6666,
		0x3333,
		0x1111,
		0x0000
	};
	static const uint R_right_masks[] = {
		0x0000,
		0x0000,
		0x0000,
		0x8888,
		0xCCCC
	};
	static const uint G_left_masks[] = {
		0x6666,
		0x3333,
		0x1111,
		0x0000,
		0x0000
	};
	static const uint G_right_masks[] = {
		0x0000,
		0x0000,
		0x8888,
		0xcccc,
		0x6666
	};
	static const uint B_left_masks[] = {
		0x3333,
		0x1111,
		0x0000,
		0x0000,
		0x0000
	};
	static const uint B_right_masks[] = {
		0x0000,
		0x8888,
		0xCCCC,
		0x6666,
		0x3333
	};

	float3 get_subpixel_contributions(float2 pixel_pos_left_corner) {
		float2 texel_pos_left_corner = floor(pixel_pos_left_corner);
		float2 weight = pixel_pos_left_corner - texel_pos_left_corner;

		float width;
		float height;
		glyph_atlas.GetDimensions(width, height);

		float2 texel_center = (texel_pos_left_corner + float2(1.0f, 1.0f)) / float2(width, height);
		uint4 texel_values = glyph_atlas.Gather(glyph_atlas_sampler, texel_center);

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
		
		float R_alpha = (countbits(R_left) + countbits(R_right)) * 0.125f;
		float G_alpha = (countbits(G_left) + countbits(G_right)) * 0.125f;
		float B_alpha = (countbits(B_left) + countbits(B_right)) * 0.125f;
		
		return float3(R_alpha, G_alpha, B_alpha);
	}

	float4 PSMain(PSInput input) : SV_TARGET {
		float2 pixel_pos_left_corner = float2(input.uv * float2(16.0f, 30.0f) + float2(0.0f, 30.0f) * 0.0f + float2(16.0f, 0.0f) * 71.0f);
		
		float3 text_color = float3(0.83137f, 0.83137f, 0.83137f);
		float3 bg_color = float3(0.117647f, 0.117647f, 0.117647f);

		float3 subpixel_left = get_subpixel_contributions(pixel_pos_left_corner + float2(-1.0f, 0.0f));
		float3 subpixel_middle = get_subpixel_contributions(pixel_pos_left_corner);
		float3 subpixel_right = get_subpixel_contributions(pixel_pos_left_corner + float2(1.0f, 0.0f));

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
		float3 subpixel_alphas = float3(R_alpha, G_alpha, B_alpha);
		
		return float4(lerp(bg_color, text_color, subpixel_alphas), 1.0f);
	}
)";

constexpr const char *COMPUTE_SHADER_SOURCE = R"(
	#define MAX_LINES_PER_GLYPH 4096;
	#define NUM_PRINTABLE_CHARS 95;

	struct Line {
		float2 p1;
		float2 p2;
	};

	struct GlyphOffset {
		uint offset;
		uint num_lines;
		float2 padding;
	};

	struct PushConstants {
		int glyph_width;
		int glyph_height;
		int ascent;
		int descent;
		uint num_glyphs;
	};

	[[vk::binding(0)]] RWTexture2D<uint> glyph_atlas;
	[[vk::binding(1)]] StructuredBuffer<Line> glyph_lines;
	[[vk::binding(2)]] StructuredBuffer<GlyphOffset> glyph_offsets;
	[[vk::push_constant]] PushConstants pc;

	[numthreads(1, 1, 1)]
	void CSMain(uint3 id : SV_DispatchThreadID) {
		int glyph_atlas_size;
		glyph_atlas.GetDimensions(glyph_atlas_size, glyph_atlas_size);
		uint num_glyphs_per_row = glyph_atlas_size / pc.glyph_width;
		uint x = uint(id.x / pc.glyph_width);
		uint y = uint(id.y / pc.glyph_height);
		uint char_index = y * num_glyphs_per_row + x;
		if(char_index >= pc.num_glyphs) {
			return;
		}

		float2 pixel_top_left = {
			id.x % pc.glyph_width,
			id.y % pc.glyph_height
		};

		uint result = 0;
		for(int i = 0; i < glyph_offsets[char_index].num_lines; ++i) {
			Line l = glyph_lines[glyph_offsets[char_index].offset + i];
			float2 p1 = l.p1;
			float2 p2 = l.p2;

			for(uint y = 0; y < 4; ++y) {
				for(uint x = 0; x < 4; ++x) {
					float2 p = pixel_top_left + float2(x / 4.0f, y / 4.0f);
				
					if(p.y < p2.y && p.y >= p1.y) {
						float dx = p2.x - p1.x;
						float dy = p2.y - p1.y;

						if(dy == 0.0f) {
						   continue;
						}

						float line_x = (p.y - p1.y) * (dx / dy) + p1.x;

						if(p.x > line_x) {
							uint mask = 1U << ((3U - x) + y * 4U);
							result ^= mask;
						}
					}
				}
			}
		}

		glyph_atlas[id.xy] = result;
	}
)";

