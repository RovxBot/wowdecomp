#pragma once
#include <cstdint>
#include <freetype/freetype.h>
struct GLYPHBITMAPDATA;
namespace RovSlug {
// Product-private flags, distinct from bitmap outline/filter and MSDF.
inline constexpr uint32_t TextFlag = 0x40;
inline constexpr uint32_t GxFlag = 0x40;
inline constexpr uint32_t Cell = 32;
inline constexpr uint32_t MaxCurves = (Cell * Cell - 1) / 6;
// Produces quadratic control points, never an FT bitmap or a distance field.
bool BuildGlyph(FT_Face face, uint32_t size, uint32_t code, uint32_t baseline, GLYPHBITMAPDATA* data);
}
