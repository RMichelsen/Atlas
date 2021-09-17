#include "glyph_rasterizer.h"
#include <wingdi.h>
#include <windows.h>

#define MAX_LINES_PER_GLYPH 4096
#define NUM_PRINTABLE_CHARS 95
static MAT2 MAT2_IDENTITY = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };

typedef struct GlyphOutline {
	Point origin;
	u32 num_lines;
} GlyphOutline;

Point fixed_to_float(POINTFX *p) {
	return (Point) {
		.x = (float)p->x.value + ((float)p->x.fract / 65536.0f),
		.y = (float)p->y.value + ((float)p->y.fract / 65536.0f)
	};
}

// https://members.loria.fr/samuel.hornus/quadratic-arc-length.html
float get_bezier_arc_length(Point a, Point b, Point c) {
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

	if(B_magn == 0.0f || F_magn == 0.0f || A_magn == 0.0f) {
		return 0.0f;
	}

	float l1 = (F_magn * A_dot_F - B_magn * A_dot_B) / (A_magn * A_magn);
	float l2 = ((B_magn * B_magn) / A_magn) - ((A_dot_B * A_dot_B) / (A_magn * A_magn * A_magn));
	float l3 = logf(A_magn * F_magn + A_dot_F) - logf(A_magn * B_magn + A_dot_B);

	return l1 + l2 * l3;
}

void add_straight_line(Point p1, Point p2, Line *lines, u32 *offset) {
	float length = sqrtf((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
	float step_size = 1.0f / (length / 0.5f);

	float t2 = step_size;
	while(1) {
		float t1 = t2 - step_size;

		float ax = p1.x * t1 + p2.x * (1 - t1);
		float ay = p1.y * t1 + p2.y * (1 - t1);
		float bx = p1.x * t2 + p2.x * (1 - t2);
		float by = p1.y * t2 + p2.y * (1 - t2);

		assert(*offset < MAX_LINES_PER_GLYPH);
		lines[(*offset)++] = ay > by ?
			(Line) { { ax, ay }, { bx, by } } :
			(Line) { { bx, by }, { ax, ay } };

		if(t2 + step_size > 1.0f) {
			float ax = p1.x * t2 + p2.x * (1 - t2);
			float ay = p1.y * t2 + p2.y * (1 - t2);
			float bx = p1.x;
			float by = p1.y;

			assert(*offset < MAX_LINES_PER_GLYPH);
			lines[(*offset)++] = ay > by ?
				(Line) { { ax, ay }, { bx, by } } :
				(Line) { { bx, by }, { ax, ay } };
			break;
		}

		t2 += step_size;
	}
}

void add_quadratic_spline(Point p1, Point p2, Point p3, Line *lines, u32 *offset) {
	float length = get_bezier_arc_length(p1, p2, p3);
	float step_size = 1.0f / (length / 0.5f);

	float t2 = step_size;
	while(1) {
		float t1 = t2 - step_size;

		float ax = (1 - t1) * ((1 - t1) * p1.x + t1 * p2.x) + t1 * ((1 - t1) * p2.x + t1 * p3.x);
		float ay = (1 - t1) * ((1 - t1) * p1.y + t1 * p2.y) + t1 * ((1 - t1) * p2.y + t1 * p3.y);
		float bx = (1 - t2) * ((1 - t2) * p1.x + t2 * p2.x) + t2 * ((1 - t2) * p2.x + t2 * p3.x);
		float by = (1 - t2) * ((1 - t2) * p1.y + t2 * p2.y) + t2 * ((1 - t2) * p2.y + t2 * p3.y);

		assert(*offset < MAX_LINES_PER_GLYPH);
		lines[(*offset)++] = ay > by ?
			(Line) { { ax, ay }, { bx, by } } :
			(Line) { { bx, by }, { ax, ay } };

		if(t2 + step_size > 1.0f) {
			float ax = (1 - t2) * ((1 - t2) * p1.x + t2 * p2.x) + t2 * ((1 - t2) * p2.x + t2 * p3.x);
			float ay = (1 - t2) * ((1 - t2) * p1.y + t2 * p2.y) + t2 * ((1 - t2) * p2.y + t2 * p3.y);
			float bx = p3.x;
			float by = p3.y;

			assert(*offset < MAX_LINES_PER_GLYPH);
			lines[(*offset)++] = ay > by ?
				(Line) { { ax, ay }, { bx, by } } :
				(Line) { { bx, by }, { ax, ay } };
			break;
		}

		t2 += step_size;
	};
}

void parse_polygon(TTPOLYGONHEADER *polygon_header, Line *lines, u32 *offset) {
	Point start_point = fixed_to_float(&polygon_header->pfxStart);
	u8 *end = (u8 *)polygon_header + polygon_header->cb;
	u8 *start = (u8 *)polygon_header + sizeof(TTPOLYGONHEADER);

	Point p1 = start_point;
	while(start < end) {
		TTPOLYCURVE *curve = (TTPOLYCURVE *)start;
		start += sizeof(WORD) * 2;

		switch(curve->wType) {
		case TT_PRIM_LINE:
		{
			for(int i = 0; i < curve->cpfx; ++i) {
				Point p2 = fixed_to_float((POINTFX *)start);

				add_straight_line(p1, p2, lines, offset);
				p1 = p2;
				start += sizeof(POINTFX);
			}
		} break;
		case TT_PRIM_QSPLINE:
		{
			for(int i = 0; i < curve->cpfx - 1; ++i) {
				Point p2 = fixed_to_float((POINTFX *)start);
				POINTFX *next = (POINTFX *)(start + sizeof(POINTFX));

				Point next_point = fixed_to_float(next);
				Point p3 = (i + 1 == curve->cpfx - 1) ?
					next_point :
					(Point) {
					.x = (p2.x + next_point.x) / 2.0f,
					.y = (p2.y + next_point.y) / 2.0f
				};

				add_quadratic_spline(p1, p2, p3, lines, offset);
				p1 = p3;
				start += sizeof(POINTFX);
			}
			start += sizeof(POINTFX);
		} break;
		case TT_PRIM_CSPLINE:
		{
			assert(false);
		} break;
		}
	}

	// Connect end to start by line segment
	add_straight_line(p1, start_point, lines, offset);
}

u32 get_glyph_max_outline_buffer_size(HDC device_context) {
	GLYPHMETRICS glyph_metrics = { 0 };
	u32 size_limit = 0;
	u32 max_width = 0;
	u32 max_height = 0;

	for(int i = 32; i < 256; ++i) {
		u32 size = GetGlyphOutline(device_context, (unsigned char)i, GGO_NATIVE | GGO_UNHINTED,
			&glyph_metrics, 0, NULL, &MAT2_IDENTITY);
		if(size > size_limit) {
			size_limit = size;
		}
	}

	return size_limit;
}

int cmp_lines(const void *l1, const void *l2) {
	float l1y = (*(Line *)l1).a.y;
	float l2y = (*(Line *)l2).a.y;
	return (l2y > l1y) - (l2y < l1y);
}

GlyphOutline ProcessGlyphOutline(HDC device_context, char c, void *buffer, Line *lines) {
	GLYPHMETRICS glyph_metrics = { 0 };
	u32 size = GetGlyphOutline(device_context, c, GGO_NATIVE | GGO_UNHINTED,
		&glyph_metrics, 0, NULL, &MAT2_IDENTITY);
	GetGlyphOutline(device_context, c, GGO_NATIVE | GGO_UNHINTED,
		&glyph_metrics, size, buffer, &MAT2_IDENTITY);

	u32 offset = 0;
	u32 bytes_processed = 0;
	while(bytes_processed < size) {
		TTPOLYGONHEADER *polygon_header = (TTPOLYGONHEADER *)((u8 *)buffer + bytes_processed);
		parse_polygon(polygon_header, lines, &offset);
		bytes_processed += polygon_header->cb;
	}

	qsort(lines, offset, sizeof(Line), cmp_lines);

	return (GlyphOutline) {
		.origin = {
			.x = (float)glyph_metrics.gmptGlyphOrigin.x,
			.y = (float)glyph_metrics.gmptGlyphOrigin.y
		},
		.num_lines = offset
	};
}

TesselatedGlyphs TesselateGlyphs(HWND hwnd, const wchar_t *font_name) {
	HDC device_context = GetDC(hwnd);
	HFONT font = CreateFont(-54, 0, 0, 0, FW_REGULAR, false, false, false,
		ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY, DEFAULT_PITCH, font_name);
	SelectObject(device_context, font);

	u32 glyph_max_outline_buffer_size = get_glyph_max_outline_buffer_size(device_context);
	void *scratch_buffer = malloc(glyph_max_outline_buffer_size);

	u32 text_metrics_size = GetOutlineTextMetrics(device_context, 0, NULL);
	OUTLINETEXTMETRIC *text_metrics = (OUTLINETEXTMETRIC *)malloc(text_metrics_size);
	assert(text_metrics);

	GetOutlineTextMetrics(device_context, text_metrics_size, text_metrics);
	u32 glyph_width = text_metrics->otmTextMetrics.tmAveCharWidth;
	u32 glyph_height = text_metrics->otmAscent - text_metrics->otmDescent;

	Line *lines = (Line *)malloc(MAX_LINES_PER_GLYPH * NUM_PRINTABLE_CHARS * sizeof(Line));
	GlyphOffset *glyph_offsets = (GlyphOffset *)malloc(NUM_PRINTABLE_CHARS * sizeof(GlyphOffset));
	u32 line_offset = 0;

	for(u32 c = 0x20; c <= 0x7E; ++c) {
		u32 index = c - 0x20;

		GlyphOutline glyph_outline = ProcessGlyphOutline(device_context, (char)c, scratch_buffer, lines + line_offset);

		// Adjust lines to match Vulkans coordinate system with downward Y axis and adjusted for descent
		for(u32 i = 0; i < glyph_outline.num_lines; ++i) {
			lines[line_offset + i].a.y = glyph_height - (lines[line_offset + i].a.y - text_metrics->otmDescent);
			lines[line_offset + i].b.y = glyph_height - (lines[line_offset + i].b.y - text_metrics->otmDescent);
		}

		glyph_offsets[index] = (GlyphOffset) {
			.offset = line_offset,
			.num_lines = glyph_outline.num_lines
		};

		line_offset += glyph_outline.num_lines;
	}

	GlyphPushConstants glyph_push_constants = {
		.glyph_width = text_metrics->otmTextMetrics.tmAveCharWidth,
		.glyph_height = text_metrics->otmAscent - text_metrics->otmDescent,
		.ascent = text_metrics->otmAscent,
		.descent = text_metrics->otmDescent
	};

	DeleteObject(font);
	free(text_metrics);

	return (TesselatedGlyphs) {
		.lines = lines,
		.num_lines = line_offset,
		.glyph_offsets = glyph_offsets,
		.num_glyphs = NUM_PRINTABLE_CHARS,
		.glyph_push_constants = glyph_push_constants
	};
}

