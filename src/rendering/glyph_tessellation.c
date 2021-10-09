#include "glyph_tessellation.h"

#include <windows.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include "ttfparser/ttfparser.h"

#define MAX_TOTAL_GLYPH_LINES 65536

typedef struct TessellationContext {
	GlyphLine *lines;
	u32 num_lines;
	GlyphPoint last_point;
} TessellationContext;

static inline float fixed_to_float(signed long x)
{
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

	float l1 = (F_magn * A_dot_F - B_magn * A_dot_B) / (A_magn * A_magn);
	float l2 = ((B_magn * B_magn) / A_magn) - ((A_dot_B * A_dot_B) / (A_magn * A_magn * A_magn));
	float l3 = logf(A_magn * F_magn + A_dot_F) - logf(A_magn * B_magn + A_dot_B);

	return l1 + l2 * l3;
}

static void add_straight_line(GlyphPoint p1, GlyphPoint p2, TessellationContext *context) {
	float length = sqrtf((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
	u32 number_of_steps = (u32)ceilf(length / 0.8f);
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

static void add_quadratic_spline(GlyphPoint p1, GlyphPoint p2, GlyphPoint p3, TessellationContext *context) {
	float length = get_bezier_arc_length(p1, p2, p3);
	u32 number_of_steps = (u32)ceilf(length / 0.8f);
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

TesselatedGlyphs tessellate_glyphs(const char *path, u32 font_size) {
	FT_Library freetype_library;
	FT_Error err = FT_Init_FreeType(&freetype_library);
	assert(!err);

	FT_Face freetype_face;
	err = FT_New_Face(freetype_library, "C:/Windows/Fonts/consola.ttf", 0, &freetype_face);
	assert(!err);
	
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

		err = FT_Load_Char(freetype_face, c, FT_LOAD_FORCE_AUTOHINT);
		assert(!err);

		err = FT_Outline_Decompose(&freetype_face->glyph->outline, &outline_funcs, &tessellation_context);
		assert(!err);

		glyph_offsets[index] = (GlyphOffset) {
			.offset = offset,
			.num_lines = tessellation_context.num_lines - offset
		};
	}

	float glyph_width = freetype_face->glyph->linearHoriAdvance / 65536.0f;
	float glyph_height = (float)(freetype_face->size->metrics.height >> 6);
	float descender = (float)(freetype_face->size->metrics.descender >> 6);

	for(u32 i = 0; i < tessellation_context.num_lines; ++i) {
		tessellation_context.lines[i].a.y = glyph_height - (tessellation_context.lines[i].a.y - descender);
		tessellation_context.lines[i].b.y = glyph_height - (tessellation_context.lines[i].b.y - descender);
	}

	GlyphMetrics glyph_metrics = {
		.glyph_width = glyph_width,
		.glyph_height = glyph_height,
		.cell_width = (u32)ceilf(glyph_width),
		.cell_height = (u32)ceilf(glyph_height)
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
