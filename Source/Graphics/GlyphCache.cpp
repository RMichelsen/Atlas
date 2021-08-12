#include "PCH.h"
#include "GlyphCache.h"

float GetBezierArcLength(POINTFX *p1, POINTFX *p2) {

	return 0.0f;
}

GlyphCache GlyphCacheInitialize(HWND hwnd) {
	HDC device_context = GetDC(hwnd);
	HFONT font = CreateFont(1024, 0, 0, 0, FW_NORMAL, false, false, false,
							ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
							DEFAULT_QUALITY, DEFAULT_PITCH, TEXT("Consolas"));
	SelectObject(device_context, font);

	GLYPHMETRICS glyph_metrics {};
	MAT2 transformation_matrix = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
	DWORD buffer_size = GetGlyphOutline(device_context, 'a', GGO_NATIVE | GGO_UNHINTED, 
										&glyph_metrics, 0, nullptr, &transformation_matrix);

	void *data = malloc(buffer_size);
	GetGlyphOutline(device_context, 'a', GGO_NATIVE | GGO_UNHINTED,
					&glyph_metrics, buffer_size, data, &transformation_matrix);

	TTPOLYGONHEADER *polygon_header = (TTPOLYGONHEADER *)data;
	POINTFX *p1 = &polygon_header->pfxStart;
	uint8_t *end = (uint8_t *)polygon_header + polygon_header->cb;
	uint8_t *start = (uint8_t *)data + sizeof(TTPOLYGONHEADER);


	while(start < end) {
		TTPOLYCURVE *curve = (TTPOLYCURVE *)start;
		start += sizeof(DWORD);

		printf("Type: %i\n", curve->wType);

		switch(curve->wType) {
		case TT_PRIM_LINE:
		{
			for(int i = 0; i < curve->cpfx; ++i) {
				POINTFX *p2 = (POINTFX *)start;

				p1 = p2;
				start += sizeof(POINTFX);
			}
		} break;
		case TT_PRIM_QSPLINE:
		{
			for(int i = 0; i < curve->cpfx - 1; i += 2) {
				POINTFX *p2 = (POINTFX *)start;
				POINTFX *p3 = (POINTFX *)(start + sizeof(POINTFX));

				p1 = p3;
				start += sizeof(POINTFX) * 2;
			}
		} break;
		case TT_PRIM_CSPLINE:
		{
			assert(false);
		} break;
		}

	}


	DeleteObject(font);

	return {};
}
