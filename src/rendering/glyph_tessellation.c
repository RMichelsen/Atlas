#include "glyph_tessellation.h"

#include <windows.h>

#include "ttfparser/ttfparser.h"

#define MAX_TOTAL_GLYPH_LINES 65536

typedef struct TessellationContext {
	GlyphLine *lines;
	u32 num_lines;
	GlyphPoint last_point;
} TessellationContext;

// https://members.loria.fr/samuel.hornus/quadratic-arc-length.html
float get_bezier_arc_length(GlyphPoint a, GlyphPoint b, GlyphPoint c) {
	float Bx = b.x - a.x;
	float By = b.y - a.y;
	float Fx = c.x - b.x;
	float Fy = c.y - b.y;
	float Ax = Fx - Bx;
	float Ay = Fy - By;
	float A_dot_B = Ax * Bx + Ay * By;
	float A_dot_F = Ax * Fx + Ay * Fy;
	float B_magn = sqrtf(Bx * Bx + By * By);
	float F_magn = sqrtf(Fx * Fx + Fy * Fy);
	float A_magn = sqrtf(Ax * Ax + Ay * Ay);

	float l1 = (F_magn * A_dot_F - B_magn * A_dot_B) / (A_magn * A_magn);
	float l2 = ((B_magn * B_magn) / A_magn) - ((A_dot_B * A_dot_B) / (A_magn * A_magn * A_magn));
	float l3 = logf(A_magn * F_magn + A_dot_F) - logf(A_magn * B_magn + A_dot_B);

	return l1 + l2 * l3;
}

void add_straight_line(GlyphPoint p1, GlyphPoint p2, TessellationContext *context) {
	float length = sqrtf((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
	u32 number_of_steps = (u32)ceilf(length / 10.0f);
	float step_fraction = 1.0f / number_of_steps;

	for(u32 i = 0; i < number_of_steps; ++i) {
		float t1 = i * step_fraction;
		float t2 = (i + 1) * step_fraction;
		float ax = p1.x * t1 + p2.x * (1 - t1);
		float ay = p1.y * t1 + p2.y * (1 - t1);
		float bx = p1.x * t2 + p2.x * (1 - t2);
		float by = p1.y * t2 + p2.y * (1 - t2);

		if(i == number_of_steps - 1) {
			bx = p1.x;
			by = p1.y;
		}

		assert(context->num_lines < MAX_TOTAL_GLYPH_LINES);
		context->lines[context->num_lines++] =
			ay > by ? (GlyphLine) { { ax, ay }, { bx, by } } :
			(GlyphLine) { { bx, by }, { ax, ay }
		};
	}
}

void add_quadratic_spline(GlyphPoint p1, GlyphPoint p2, GlyphPoint p3, TessellationContext *context) {
	float length = get_bezier_arc_length(p1, p2, p3);
	u32 number_of_steps = (u32)ceilf(length / 10.0f);
	float step_fraction = 1.0f / number_of_steps;

	for(u32 i = 0; i < number_of_steps; ++i) {
		float t1 = i * step_fraction;
		float t2 = (i + 1) * step_fraction;
		float ax = (1 - t1) * ((1 - t1) * p1.x + t1 * p2.x) + t1 * ((1 - t1) * p2.x + t1 * p3.x);
		float ay = (1 - t1) * ((1 - t1) * p1.y + t1 * p2.y) + t1 * ((1 - t1) * p2.y + t1 * p3.y);
		float bx = (1 - t2) * ((1 - t2) * p1.x + t2 * p2.x) + t2 * ((1 - t2) * p2.x + t2 * p3.x);
		float by = (1 - t2) * ((1 - t2) * p1.y + t2 * p2.y) + t2 * ((1 - t2) * p2.y + t2 * p3.y);

		if(i == number_of_steps - 1) {
			bx = p3.x;
			by = p3.y;
		}

		assert(context->num_lines < MAX_TOTAL_GLYPH_LINES);
		context->lines[context->num_lines++] =
			ay > by ? (GlyphLine) { { ax, ay }, { bx, by } } :
			(GlyphLine) { { bx, by }, { ax, ay }
		};
	}
}

void outline_move_to(float x, float y, void *data) {
	TessellationContext *context = (TessellationContext *)data;
	context->last_point = (GlyphPoint) { x, y };
}
void outline_line_to(float x, float y, void *data) {
	TessellationContext *context = (TessellationContext *)data;
	add_straight_line(
		context->last_point,
		(GlyphPoint) { x, y },
		context
	);
	context->last_point = (GlyphPoint) { x, y };
}
void outline_quad_to(float x1, float y1, float x, float y, void *user) {
	TessellationContext *context = (TessellationContext *)user;
	add_quadratic_spline(
		context->last_point,
		(GlyphPoint) { x1, y1 },
		(GlyphPoint) { x, y },
		context
	);
	context->last_point = (GlyphPoint) { x, y };
}
void outline_curve_to(float x1, float y1, float x2, float y2,
	float x, float y, void *data) {
	assert(false);
}
void outline_close_path(void *data) {}

static ttfp_outline_builder outline_builder = {
	.move_to = outline_move_to,
	.line_to = outline_line_to,
	.quad_to = outline_quad_to,
	.curve_to = outline_curve_to,
	.close_path = outline_close_path
};

TesselatedGlyphs tessellate_glyphs(const char *path, u32 font_size) {
	FILE *file = fopen(path, "rb");
	fseek(file, 0, SEEK_END);
	u32 file_size = ftell(file);
	rewind(file);

	void *font_data = malloc(file_size);
	fread(font_data, 1, file_size, file);
	fclose(file);

	void *font_face = malloc(ttfp_face_size_of());

	bool ok = ttfp_face_init(font_data, file_size, 0, font_face);
	assert(ok);

	assert(ttfp_is_monospaced(font_face));

	TessellationContext tessellation_context = {
		.lines = (GlyphLine *)malloc(MAX_TOTAL_GLYPH_LINES * sizeof(GlyphLine)),
		.num_lines = 0
	};
	GlyphOffset *glyph_offsets = (GlyphOffset *)malloc(NUM_PRINTABLE_CHARS * sizeof(GlyphOffset));

	// For the space character an empty entry is fine
	glyph_offsets[0] = (GlyphOffset) { 0 };

	for(u32 c = 0x21; c <= 0x7E; ++c) {
		u32 index = c - 0x20;

		u32 offset = tessellation_context.num_lines;

		ttfp_rect bounding_box = { 0 };
		ok = ttfp_outline_glyph(font_face, outline_builder, &tessellation_context,
			ttfp_get_glyph_index(font_face, c), &bounding_box);
		assert(ok);

		glyph_offsets[index] = (GlyphOffset) {
			.offset = offset,
			.num_lines = tessellation_context.num_lines - offset
		};
	}

	float descent = ttfp_get_descender(font_face);
	float height = ttfp_get_height(font_face);
	float ascent = ttfp_get_ascender(font_face);

	float font_scale = font_size * GetDpiForSystem() / (72.0f * ttfp_get_units_per_em(font_face));

	// Ensure that the baseline falls exactly in the middle of a pixel
	float baseline = ttfp_get_ascender(font_face);
	float target = ceilf(font_scale * baseline) + 0.5f;
	font_scale = target / baseline;

	// Adjust lines to match Vulkans coordinate system with downward Y axis and adjusted for descent
	for(u32 i = 0; i < tessellation_context.num_lines; ++i) {
		tessellation_context.lines[i].a.y = height - (tessellation_context.lines[i].a.y - descent);
		tessellation_context.lines[i].b.y = height - (tessellation_context.lines[i].b.y - descent);

		// Scale all lines with the chosen font size
		tessellation_context.lines[i].a.x *= font_scale;
		tessellation_context.lines[i].a.y *= font_scale;
		tessellation_context.lines[i].b.x *= font_scale;
		tessellation_context.lines[i].b.y *= font_scale;
	}

	float glyph_width = font_scale * ttfp_get_glyph_hor_advance(font_face, ttfp_get_glyph_index(font_face, (u32)'M'));
	float glyph_height = font_scale * height;

	GlyphMetrics glyph_metrics = {
		.glyph_width = glyph_width,
		.glyph_height = glyph_height,
		.cell_width = (u32)ceilf(glyph_width) + 1,
		.cell_height = (u32)ceilf(glyph_height) + 1
	};

	free(font_data);
	free(font_face);

	return (TesselatedGlyphs) {
		.lines = tessellation_context.lines,
		.num_lines = tessellation_context.num_lines,
		.glyph_offsets = glyph_offsets,
		.num_glyphs = NUM_PRINTABLE_CHARS,
		.metrics = glyph_metrics
	};
}
