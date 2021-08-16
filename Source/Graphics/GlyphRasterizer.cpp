#include "PCH.h"
#include "GlyphRasterizer.h"

#include "Graphics/RenderTypes.h"

struct GlyphOutline {
	Point origin;
	uint32_t num_lines;
};

// https://members.loria.fr/samuel.hornus/quadratic-arc-length.html
float GetBezierArcLength(Point a, Point b, Point c) {
	Point B = b - a;
	Point F = c - b;
	Point A = F - B;
	float A_dot_B = A * B;
	float A_dot_F = A * F;
	float B_magn = sqrtf(B.x * B.x + B.y * B.y);
	float F_magn = sqrtf(F.x * F.x + F.y * F.y);
	float A_magn = sqrtf(A.x * A.x + A.y * A.y);

	float l1 = (F_magn * A_dot_F - B_magn * A_dot_B) / (A_magn * A_magn);
	float l2 = ((B_magn * B_magn) / A_magn) - ((A_dot_B * A_dot_B) / (A_magn * A_magn * A_magn));
	float l3 = logf(A_magn * F_magn + A_dot_F) - logf(A_magn * B_magn + A_dot_B);
	return l1 + l2 * l3;
}

void AddStraightLine(Point p1, Point p2, Line *lines, uint32_t *offset) {
	float length = sqrtf((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
	float step_size = 1.0f / (length / 0.5f);

	float t2 = step_size;
	while(true) {
		float t1 = t2 - step_size;

		Point a = p1 * t1 + p2 * (1 - t1);
		Point b = p1 * t2 + p2 * (1 - t2);
		assert(*offset < MAX_LINES_PER_GLYPH);
		lines[(*offset)++] = a.y > b.y ? Line { a, b } : Line { b, a };

		if(t2 + step_size > 1.0f) {
			a = p1 * t2 + p2 * (1 - t2);
			b = p1;
			assert(*offset < MAX_LINES_PER_GLYPH);
			lines[(*offset)++] = a.y > b.y ? Line { a, b } : Line { b, a };
			break;
		}

		t2 += step_size;
	}
}

void AddQuadraticSpline(Point p1, Point p2, Point p3, Line *lines, uint32_t *offset) {
	float length = GetBezierArcLength(p1, p2, p3);
	float step_size = 1.0f / (length / 0.5f);

	float t2 = step_size;
	while(true) {
		float t1 = t2 - step_size;

		Point a = (1 - t1) * ((1 - t1) * p1 + t1 * p2) + t1 * ((1 - t1) * p2 + t1 * p3);
		Point b = (1 - t2) * ((1 - t2) * p1 + t2 * p2) + t2 * ((1 - t2) * p2 + t2 * p3);
		assert(*offset < MAX_LINES_PER_GLYPH);
		lines[(*offset)++] = a.y > b.y ? Line { a, b } : Line { b, a };

		if(t2 + step_size > 1.0f) {
			a = (1 - t2) * ((1 - t2) * p1 + t2 * p2) + t2 * ((1 - t2) * p2 + t2 * p3);
			b = p3;
			assert(*offset < MAX_LINES_PER_GLYPH);
			lines[(*offset)++] = a.y > b.y ? Line { a, b } : Line { b, a };
			break;
		}

		t2 += step_size;
	};
}

void ParsePolygon(TTPOLYGONHEADER *polygon_header, Line *lines, uint32_t *offset) {
	Point start_point = FixedToFloat(&polygon_header->pfxStart);
	uint8_t *end = (uint8_t *)polygon_header + polygon_header->cb;
	uint8_t *start = (uint8_t *)polygon_header + sizeof(TTPOLYGONHEADER);

	Point p1 = start_point;
	while(start < end) {
		TTPOLYCURVE *curve = (TTPOLYCURVE *)start;
		start += sizeof(WORD) * 2;

		switch(curve->wType) {
		case TT_PRIM_LINE:
		{
			for(int i = 0; i < curve->cpfx; ++i) {
				Point p2 = FixedToFloat((POINTFX *)start);

				AddStraightLine(p1, p2, lines, offset);
				p1 = p2;
				start += sizeof(POINTFX);
			}
		} break;
		case TT_PRIM_QSPLINE:
		{
			for(int i = 0; i < curve->cpfx - 1; ++i) {
				Point p2 = FixedToFloat((POINTFX *)start);
				POINTFX *next = (POINTFX *)(start + sizeof(POINTFX));
				Point p3 = (i + 1 == curve->cpfx - 1) ? FixedToFloat(next) : (p2 + FixedToFloat(next)) / 2.0f;

				AddQuadraticSpline(p1, p2, p3, lines, offset);
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
	AddStraightLine(p1, start_point, lines, offset);
}

uint32_t GetGlyphMaxOutlineBufferSize(HDC device_context) {
	GLYPHMETRICS glyph_metrics {};
	uint32_t size_limit = 0;
	uint32_t max_width = 0;
	uint32_t max_height = 0;
	for(int i = 32; i < 256; ++i) {
		uint32_t size = GetGlyphOutline(device_context, (unsigned char)i, GGO_NATIVE | GGO_UNHINTED,
									 &glyph_metrics, 0, nullptr, &identity);
		if(size > size_limit) {
			size_limit = size;
		}
	}

	return size_limit;
}

GlyphOutline ProcessGlyphOutline(HDC device_context, char c, void *buffer, Line *lines) {
	GLYPHMETRICS glyph_metrics {};
	uint32_t size = GetGlyphOutline(device_context, c, GGO_NATIVE | GGO_UNHINTED,
									&glyph_metrics, 0, nullptr, &identity);
	GetGlyphOutline(device_context, c, GGO_NATIVE | GGO_UNHINTED,
					&glyph_metrics, size, buffer, &identity);

	uint32_t offset = 0;
	uint32_t bytes_processed = 0;
	while(bytes_processed < size) {
		TTPOLYGONHEADER *polygon_header = (TTPOLYGONHEADER *)((uint8_t *)buffer + bytes_processed);
		ParsePolygon(polygon_header, lines, &offset);
		bytes_processed += polygon_header->cb;
	}

	qsort(lines, offset, sizeof(Line), 
		  [](const void *l1, const void *l2) { 
			  float l1y = (*(Line *)l1).a.y;
			  float l2y = (*(Line *)l2).a.y;
			  return (l2y > l1y) - (l2y < l1y);
		  });
	
	return GlyphOutline {
		.origin = Point { (float)glyph_metrics.gmptGlyphOrigin.x, (float)glyph_metrics.gmptGlyphOrigin.y },
		.num_lines = offset
	};
}

TesselatedGlyphs TesselateGlyphs(HWND hwnd, const wchar_t *font_name) {
	HDC device_context = GetDC(hwnd);
	HFONT font = CreateFont(128, 0, 0, 0, FW_NORMAL, false, false, false,
							ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
							DEFAULT_QUALITY, DEFAULT_PITCH, font_name);
	SelectObject(device_context, font);

	uint32_t glyph_max_outline_buffer_size = GetGlyphMaxOutlineBufferSize(device_context);
	void *scratch_buffer = malloc(glyph_max_outline_buffer_size);

	uint32_t text_metrics_size = GetOutlineTextMetrics(device_context, 0, nullptr);
	OUTLINETEXTMETRIC *text_metrics = (OUTLINETEXTMETRIC *)malloc(text_metrics_size);
	GetOutlineTextMetrics(device_context, text_metrics_size, text_metrics);
	uint32_t glyph_width = text_metrics->otmTextMetrics.tmAveCharWidth;
	uint32_t glyph_height = text_metrics->otmAscent - text_metrics->otmDescent;

	Line *lines = (Line *)malloc(MAX_LINES_PER_GLYPH * NUM_PRINTABLE_CHARS * sizeof(Line));
	GlyphOffset *glyph_offsets = (GlyphOffset *)malloc(NUM_PRINTABLE_CHARS * sizeof(GlyphOffset));
	uint32_t line_offset = 0;

	for(uint32_t c = 0x20; c <= 0x7E; ++c) {
		uint32_t index = c - 0x20;

		GlyphOutline glyph_outline = ProcessGlyphOutline(device_context, (char)c, scratch_buffer, lines + line_offset);

		// Adjust lines to match Vulkans coordinate system with downward Y axis and adjusted for descent
		for(uint32_t i = 0; i < glyph_outline.num_lines; ++i) {
			lines[line_offset + i].a.y = glyph_height - (lines[line_offset + i].a.y - text_metrics->otmDescent);
			lines[line_offset + i].b.y = glyph_height - (lines[line_offset + i].b.y - text_metrics->otmDescent);
		}

		glyph_offsets[index] = GlyphOffset {
			.offset = line_offset,
			.num_lines = glyph_outline.num_lines
		};

		line_offset += glyph_outline.num_lines;
	}

	GlyphPushConstants glyph_push_constants = {
		.glyph_width = text_metrics->otmTextMetrics.tmAveCharWidth,
		.glyph_height = text_metrics->otmAscent - text_metrics->otmDescent,
		.ascent = text_metrics->otmAscent,
		.descent = text_metrics->otmDescent,
		.num_glyphs = NUM_PRINTABLE_CHARS
	};

	DeleteObject(font);
	free(text_metrics);

	return TesselatedGlyphs {
		.lines = lines,
		.num_lines = line_offset,
		.glyph_offsets = glyph_offsets,
		.num_glyphs = NUM_PRINTABLE_CHARS,
		.glyph_push_constants = glyph_push_constants
	};
}

