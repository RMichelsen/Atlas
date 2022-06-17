#version 460

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

void main() {
	vec2 abs_pixel_origin = in_uv * vec2(pc.glyph_width, pc.glyph_height);
	vec2 pixel_origin = abs_pixel_origin + in_glyph_offset * vec2(pc.cell_width, pc.cell_height);

    uint pixel_xxleft = texture(glyph_atlas, (pixel_origin + vec2(-2.0, 0.0)) / pc.glyph_atlas_size).x;
    uint pixel_xleft = texture(glyph_atlas, (pixel_origin + vec2(-1.0, 0.0)) / pc.glyph_atlas_size).x;
    uint pixel_left = texture(glyph_atlas, pixel_origin / pc.glyph_atlas_size).x;
    uint pixel_middle = texture(glyph_atlas, (pixel_origin + vec2(1.0, 0.0)) / pc.glyph_atlas_size).x;
    uint pixel_right = texture(glyph_atlas, (pixel_origin + vec2(2.0, 0.0)) / pc.glyph_atlas_size).x;
    uint pixel_xright = texture(glyph_atlas, (pixel_origin + vec2(3.0, 0.0)) / pc.glyph_atlas_size).x;
    uint pixel_xxright = texture(glyph_atlas, (pixel_origin + vec2(4.0, 0.0)) / pc.glyph_atlas_size).x;

    float R = (bitCount(pixel_xxleft) * 0.03125 + 
              bitCount(pixel_xleft) * 0.30078125 +
              bitCount(pixel_left) * 0.3359375 +
              bitCount(pixel_middle) * 0.30078125 +
              bitCount(pixel_right) * 0.03125) / 16.0;

    float G = (bitCount(pixel_xleft) * 0.03125 + 
              bitCount(pixel_left) * 0.30078125 +
              bitCount(pixel_middle) * 0.3359375 +
              bitCount(pixel_right) * 0.30078125 +
              bitCount(pixel_xright) * 0.03125) / 16.0;

    float B = (bitCount(pixel_left) * 0.03125 + 
              bitCount(pixel_middle) * 0.30078125 +
              bitCount(pixel_right) * 0.3359375 +
              bitCount(pixel_xright) * 0.30078125 +
              bitCount(pixel_xxright) * 0.03125) / 16.0;

	vec3 text_color = vec3(0.91796875, 0.85546875, 0.6953125);
	vec3 bg_color = vec3(0.15625, 0.15625, 0.15625);
	vec3 coverage = vec3(R, G, B);

    // Gamma correct
    vec3 color = pow(text_color, vec3(1.43)) * coverage + pow(bg_color, vec3(1.43)) * (1.0 - coverage);
    out_color = vec4(pow(color, vec3(1.0 / 1.43)), 1.0);
}

