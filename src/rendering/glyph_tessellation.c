#include "glyph_tessellation.h"

#include <windows.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#define MAX_TOTAL_GLYPH_LINES 65536

typedef struct TessellationContext {
	GlyphLine *lines;
	u32 num_lines;
	GlyphPoint last_point;
} TessellationContext;

static inline float fixed_to_float(signed long x) {
	return (float)x / 64.0f;
}

// https://members.loria.fr/samuel.hornus/quadratic-arc-length.html
static float get_bezier_arc_length(GlyphPoint a, GlyphPoint b, GlyphPoint c) {
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

	if (B_magn == 0.0f || F_magn == 0.0f || A_magn == 0.0f) {
		return 0.0f;
	}

	float l1 = (F_magn * A_dot_F - B_magn * A_dot_B) / (A_magn * A_magn);
	float l2 = ((B_magn * B_magn) / A_magn) - ((A_dot_B * A_dot_B) / (A_magn * A_magn * A_magn));
	float l3 = logf(A_magn * F_magn + A_dot_F) - logf(A_magn * B_magn + A_dot_B);

	if (isnan(l3)) {
		return 0.0f;
	}

	return l1 + l2 * l3;
}

static void add_straight_line(GlyphPoint p1, GlyphPoint p2, TessellationContext *context) {
    assert(context->num_lines < MAX_TOTAL_GLYPH_LINES);
	context->lines[context->num_lines++] = p1.y > p2.y ?
		(GlyphLine) { p1, p2 } : (GlyphLine) { p2, p1 };
	return;
}

static void add_quadratic_spline(GlyphPoint p1, GlyphPoint p2, GlyphPoint p3, TessellationContext *context) {
	if ((p1.x == p2.x && p2.x == p3.x) || (p1.y == p2.y && p2.y == p3.y)) {
		add_straight_line(p1, p3, context);
		return;
	}

	float length = get_bezier_arc_length(p1, p2, p3);
	u32 number_of_steps = length > 0.0f ? (u32)ceilf(length / 0.5f) : 25;
	float step_fraction = 1.0f / number_of_steps;

	for(u32 i = 0; i < number_of_steps; ++i) {
		float t1 = i * step_fraction;
		float t2 = (i + 1) * step_fraction;
		float ax = (1 - t1) * ((1 - t1) * p1.x + t1 * p2.x) + t1 * ((1 - t1) * p2.x + t1 * p3.x);
		float ay = (1 - t1) * ((1 - t1) * p1.y + t1 * p2.y) + t1 * ((1 - t1) * p2.y + t1 * p3.y);
		float bx = (1 - t2) * ((1 - t2) * p1.x + t2 * p2.x) + t2 * ((1 - t2) * p2.x + t2 * p3.x);
		float by = (1 - t2) * ((1 - t2) * p1.y + t2 * p2.y) + t2 * ((1 - t2) * p2.y + t2 * p3.y);

		assert(context->num_lines < MAX_TOTAL_GLYPH_LINES);
		context->lines[context->num_lines++] =
			ay > by ? (GlyphLine) { { ax, ay }, { bx, by } } :
			(GlyphLine) { { bx, by }, { ax, ay }
		};
	}
}

int outline_move_to(const FT_Vector *to, void *user) {
	TessellationContext *context = (TessellationContext *)user;
	float x = fixed_to_float(to->x);
	float y = fixed_to_float(to->y);
	context->last_point = (GlyphPoint) { x, y };
	return 0;
}
int outline_line_to(const FT_Vector* to, void *user) {
	TessellationContext *context = (TessellationContext *)user;
	float x = fixed_to_float(to->x);
	float y = fixed_to_float(to->y);
	add_straight_line(
		context->last_point,
		(GlyphPoint) { x, y },
		context
	);
	context->last_point = (GlyphPoint) { x, y };
	return 0;
}
int outline_conic_to(const FT_Vector* control, const FT_Vector* to, void *user) {
	TessellationContext *context = (TessellationContext *)user;
	float cx = fixed_to_float(control->x);
	float cy = fixed_to_float(control->y);
	float x = fixed_to_float(to->x);
	float y = fixed_to_float(to->y);
	add_quadratic_spline(
		context->last_point,
		(GlyphPoint) { cx, cy },
		(GlyphPoint) { x, y },
		context
	);
	context->last_point = (GlyphPoint) { x, y };
	return 0;
}
int outline_cubic_to(const FT_Vector* control1, const FT_Vector* control2, 
	const FT_Vector* to, void *user) {
	assert(false);
	return 1;
}

static FT_Outline_Funcs outline_funcs = {
	.move_to = outline_move_to,
	.line_to = outline_line_to,
	.conic_to = outline_conic_to,
	.cubic_to = outline_cubic_to
};

int cmp_glyph_lines(const void* l1, const void* l2) {
	float l1y = (*(GlyphLine *)l1).a.y;
	float l2y = (*(GlyphLine *)l2).a.y;
	return (l2y > l1y) - (l2y < l1y);
}

TesselatedGlyphs tessellate_glyphs(const char *font_path, u32 font_size) {
	FT_Library freetype_library;
	FT_Error err = FT_Init_FreeType(&freetype_library);
	assert(!err);

	FT_Face freetype_face;
	err = FT_New_Face(freetype_library, font_path, 0, &freetype_face);
	assert(!err);

	// TODO: Support variable .ttf fonts with overlapping outlines.
	// https://github.com/microsoft/cascadia-code/issues/350

	err = FT_Set_Char_Size(freetype_face, 0, font_size << 6, 0, 0);
	assert(!err);

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

		err = FT_Load_Char(freetype_face, c, FT_LOAD_TARGET_LIGHT);
		assert(!err);

		err = FT_Outline_Decompose(&freetype_face->glyph->outline, &outline_funcs, &tessellation_context);
		assert(!err);

		u32 num_lines_in_glyph = tessellation_context.num_lines - offset;
		qsort(&tessellation_context.lines[offset], num_lines_in_glyph, sizeof(GlyphLine), cmp_glyph_lines);

		glyph_offsets[index] = (GlyphOffset) {
			.offset = offset,
			.num_lines = tessellation_context.num_lines - offset
		};
	}

	float glyph_width = freetype_face->glyph->linearHoriAdvance / 65536.0f;
	float glyph_height = (float)(freetype_face->size->metrics.height >> 6);
	float descender = (float)(freetype_face->size->metrics.descender >> 6);

	// Pre-rasterization pass to adjust coordinates to be +Y down
	// and to find the minimum y coordinate so the glyph atlas can take
	// an early out in case a pixel is strictly above the glyph outlines.
	for (u32 c = 0x21; c <= 0x7E; ++c) {
		u32 index = c - 0x20;
		u32 offset = glyph_offsets[index].offset;
		float min_y = glyph_height;
		for (u32 i = 0; i < glyph_offsets[index].num_lines; ++i) {
			tessellation_context.lines[offset + i].a.y = 
				glyph_height - (tessellation_context.lines[offset + i].a.y - descender);
			tessellation_context.lines[offset + i].b.y = 
				glyph_height - (tessellation_context.lines[offset + i].b.y - descender);
			if (tessellation_context.lines[offset + i].a.y < min_y) {
				min_y = tessellation_context.lines[offset + i].a.y;
			}
			if (tessellation_context.lines[offset + i].b.y < min_y) {
				min_y = tessellation_context.lines[offset + i].b.y;
			}
		}
	}

	GlyphMetrics glyph_metrics = {
		.glyph_width = glyph_width,
		.glyph_height = glyph_height,
		.cell_width = (u32)ceilf(glyph_width) + 1,
		.cell_height = (u32)ceilf(glyph_height) + 1
	};

	FT_Done_Face(freetype_face);
	FT_Done_FreeType(freetype_library);

	return (TesselatedGlyphs) {
		.lines = tessellation_context.lines,
		.num_lines = tessellation_context.num_lines,
		.glyph_offsets = glyph_offsets,
		.num_glyphs = NUM_PRINTABLE_CHARS,
		.metrics = glyph_metrics
	};
}
