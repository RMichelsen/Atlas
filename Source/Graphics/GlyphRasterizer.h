#pragma once

constexpr static uint64_t MAX_LINES = 4096;

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

static inline Point FixedToFloat(POINTFX *p) {
	return Point {
		.x = (float)p->x.value + ((float)p->x.fract / 65536.0f),
		.y = (float)p->y.value + ((float)p->y.fract / 65536.0f)
	};
}

struct Line {
	Point a;
	Point b;
};

struct GlyphInformation {
	Line lines[MAX_LINES];
	int scanline_start_indices[128];
};

void RasterizeGlyphs(HWND hwnd, const wchar_t *font_name, VkCommandBuffer command_buffer,
					 GlyphInformation *glyph_information_gpu_mapped);

