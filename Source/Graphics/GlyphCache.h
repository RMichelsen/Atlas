#pragma once

#include "Common/DynArray.h"
#include "Common/GlyphPrimitives.h"

struct GlyphCache {
	DynArray<Line> glyphs[256];
};

GlyphCache GlyphCacheInitialize(HWND hwnd);

