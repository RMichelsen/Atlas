#pragma once

#include <stdint.h>
#include <Wingdi.h>

// NOTE: To change these, corresponding shader variables must be changed as well
constexpr static uint64_t MAX_LINES_PER_GLYPH = 4096;
constexpr static uint32_t NUM_PRINTABLE_CHARS = 95;

constexpr static uint32_t GLYPH_ATLAS_SIZE = 4096;
constexpr static MAT2 identity = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };

struct Point {
	float x;
	float y;
};

inline Point operator-(Point a, Point b) {
	return Point {
		.x = a.x - b.x,
		.y = a.y - b.y,
	};
}
inline Point operator+(Point a, Point b) {
	return Point {
		.x = a.x + b.x,
		.y = a.y + b.y,
	};
}
inline Point operator*(Point a, float s) {
	return Point {
		.x = a.x * s,
		.y = a.y * s
	};
}
inline Point operator*(float s, Point a) {
	return a * s;
}
inline float operator*(Point a, Point b) {
	return a.x * b.x + a.y * b.y;
}
inline Point operator/(Point a, float s) {
	return Point {
		.x = a.x / s,
		.y = a.y / s
	};
}

inline Point FixedToFloat(POINTFX *p) {
	return Point {
		.x = (float)p->x.value + ((float)p->x.fract / 65536.0f),
		.y = (float)p->y.value + ((float)p->y.fract / 65536.0f)
	};
}

struct Line {
	Point a;
	Point b;
};

struct GlyphOffset {
	uint32_t offset;
	uint32_t num_lines;
    uint64_t padding;
};

struct GlyphPushConstants {
	int glyph_width;
	int glyph_height;
	int ascent;
	int descent;
	uint32_t num_glyphs;
};

struct TesselatedGlyphs {
	Line *lines;
	uint64_t num_lines;
	GlyphOffset *glyph_offsets;
	uint64_t num_glyphs;
	GlyphPushConstants glyph_push_constants;
};

