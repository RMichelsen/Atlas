#include "PCH.h"
#include "GlyphCache.h"

#include "Common/DynArray.h"

struct Point {
	float x;
	float y;
};

Point operator-(Point a, Point b) {
	return Point {
		.x = a.x - b.x,
		.y = a.y - b.y,
	};
}
Point operator+(Point a, Point b) {
	return Point {
		.x = a.x + b.x,
		.y = a.y + b.y,
	};
}
Point operator*(Point a, float s) {
	return Point {
		.x = a.x * s,
		.y = a.y * s
	};
}
Point operator*(float s, Point a) {
	return a * s;
}
float operator*(Point a, Point b) {
	return a.x * b.x + a.y * b.y;
}
Point operator/(Point a, float s) {
	return Point {
		.x = a.x / s,
		.y = a.y / s
	};
}

Point FixedToFloat(POINTFX *p) {
	return Point {
		.x = (float)p->x.value + ((float)p->x.fract / 65536.0f),
		.y = (float)p->y.value + ((float)p->y.fract / 65536.0f)
	};
}

struct Line {
	Point a;
	Point b;
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

void AddStraightLine(Point p1, Point p2, DynArray<Line> *lines) {
	float length = sqrtf((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
	float step_size = 1.0f / (length / 2.0f);

	float t2 = step_size;
	while(t2 < 1.0f) {
		float t1 = t2 - step_size;

		Point a = p1 * t1 + p2 * (1 - t1);
		Point b = p1 * t2 + p2 * (1 - t2);

		lines->push_back(Line { a, b });

		t2 += step_size;
	}
}

void AddQuadraticSpline(Point p1, Point p2, Point p3, DynArray<Line> *lines) {
	float length = GetBezierArcLength(p1, p2, p3);
	float step_size = 1.0f / (length / 2.0f);

	float t2 = step_size;
	while(t2 < 1.0f) {
		float t1 = t2 - step_size;

		Point a = (1 - t1) * ((1 - t1) * p1 + t1 * p2) + t1 * ((1 - t1) * p2 + t1 * p3);
		Point b = (1 - t2) * ((1 - t2) * p1 + t2 * p2) + t2 * ((1 - t2) * p2 + t2 * p3);

		lines->push_back(Line { a, b });

		t2 += step_size;
	}
}

void ParsePolygon(TTPOLYGONHEADER *polygon_header, DynArray<Line> *lines) {
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

				AddStraightLine(p1, p2, lines);
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

				AddQuadraticSpline(p1, p2, p3, lines);
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
	AddStraightLine(p1, start_point, lines);
}

GlyphCache GlyphCacheInitialize(HWND hwnd) {
	HDC device_context = GetDC(hwnd);
	HFONT font = CreateFont(128, 0, 0, 0, FW_NORMAL, false, false, false,
							ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
							DEFAULT_QUALITY, DEFAULT_PITCH, TEXT("Consolas"));
	SelectObject(device_context, font);

	GLYPHMETRICS glyph_metrics {};
	MAT2 transformation_matrix = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
	DWORD buffer_size = GetGlyphOutline(device_context, 'g', GGO_NATIVE | GGO_UNHINTED, 
										&glyph_metrics, 0, nullptr, &transformation_matrix);

	void *data = malloc(buffer_size);
	GetGlyphOutline(device_context, 'g', GGO_NATIVE | GGO_UNHINTED,
					&glyph_metrics, buffer_size, data, &transformation_matrix);

	DynArray<Line> lines;
	uint32_t bytes_processed = 0;
	while(bytes_processed < buffer_size) {
		TTPOLYGONHEADER *polygon_header = (TTPOLYGONHEADER *)((uint8_t *)data + bytes_processed);
		ParsePolygon(polygon_header, &lines);
		bytes_processed += polygon_header->cb;
	}

	DeleteObject(font);
	return {};
}
