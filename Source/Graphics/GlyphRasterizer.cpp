#include "PCH.h"
#include "GlyphRasterizer.h"

constexpr static MAT2 identity = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };

struct GlyphOutline {
	Line *lines;
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
	float step_size = 1.0f / (length / 2.0f);

	float t2 = step_size;
	while(t2 < 1.0f) {
		float t1 = t2 - step_size;

		Point a = p1 * t1 + p2 * (1 - t1);
		Point b = p1 * t2 + p2 * (1 - t2);
		
		assert(*offset < MAX_LINES);
		lines[(*offset)++] = Line { a, b };
		t2 += step_size;
	}
}

void AddQuadraticSpline(Point p1, Point p2, Point p3, Line *lines, uint32_t *offset) {
	float length = GetBezierArcLength(p1, p2, p3);
	float step_size = 1.0f / (length / 2.0f);

	float t2 = step_size;
	while(t2 < 1.0f) {
		float t1 = t2 - step_size;

		Point a = (1 - t1) * ((1 - t1) * p1 + t1 * p2) + t1 * ((1 - t1) * p2 + t1 * p3);
		Point b = (1 - t2) * ((1 - t2) * p1 + t2 * p2) + t2 * ((1 - t2) * p2 + t2 * p3);

		assert(*offset < MAX_LINES);
		lines[(*offset)++] = Line { a, b };
		t2 += step_size;
	}
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

uint32_t GetScratchBufferSize(HDC device_context) {
	GLYPHMETRICS glyph_metrics {};
	uint32_t scratch_buffer_size = 0;
	for(int i = 32; i < 256; ++i) {
		uint32_t size = GetGlyphOutline(device_context, (unsigned char)i, GGO_NATIVE | GGO_UNHINTED,
									 &glyph_metrics, 0, nullptr, &identity);
		if(size > scratch_buffer_size) {
			scratch_buffer_size = size;
		}
	}
	return scratch_buffer_size;
}

GlyphOutline ProcessGlyphOutline(HDC device_context, char c, void *buffer) {
	GLYPHMETRICS glyph_metrics {};
	uint32_t size = GetGlyphOutline(device_context, c, GGO_NATIVE | GGO_UNHINTED,
									&glyph_metrics, 0, nullptr, &identity);
	GetGlyphOutline(device_context, c, GGO_NATIVE | GGO_UNHINTED,
					&glyph_metrics, size, buffer, &identity);

	Line *lines = (Line *)malloc(MAX_LINES * sizeof(Line));

	uint32_t offset = 0;
	uint32_t bytes_processed = 0;
	while(bytes_processed < size) {
		TTPOLYGONHEADER *polygon_header = (TTPOLYGONHEADER *)((uint8_t *)buffer + bytes_processed);
		ParsePolygon(polygon_header, lines, &offset);
		bytes_processed += polygon_header->cb;
	}

	qsort(lines, offset, sizeof(Line), 
		  [](const void *l1, const void *l2) { 
			  float ay = (*(Line *)l1).a.y;
			  float by = (*(Line *)l2).b.y;
			  return (by > ay) - (by < ay);
		  });
	
	return GlyphOutline {
		.lines = lines,
		.num_lines = offset
	};
}

void RasterizeGlyphs(HWND hwnd, const wchar_t *font_name, VkCommandBuffer command_buffer,
					 GlyphInformation *glyph_information_gpu_mapped) {
	HDC device_context = GetDC(hwnd);
	HFONT font = CreateFont(128, 0, 0, 0, FW_NORMAL, false, false, false,
							ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
							DEFAULT_QUALITY, DEFAULT_PITCH, font_name);
	SelectObject(device_context, font);

	uint32_t scratch_buffer_size = GetScratchBufferSize(device_context);
	void *scratch_buffer = malloc(scratch_buffer_size);


	for(int i = 32; i < 256; ++i) {
		GlyphOutline glyph_outline = ProcessGlyphOutline(device_context, (unsigned char)i, scratch_buffer);
		memcpy(glyph_information_gpu_mapped[i].lines, glyph_outline.lines, glyph_outline.num_lines * sizeof(Line));

		vkCmdDispatch(command_buffer, 1024, 1024, 1);

		free(glyph_outline.lines);
	}

	DeleteObject(font);
}
