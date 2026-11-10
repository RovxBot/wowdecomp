// Product-authored TrueType outline packing for the Slug shader. No game assets.
#include "Engine/Source/Gxu/Slug.h"
#include "Engine/Source/Gxu/GxuFontMiscClasses.h"
#include "Engine/Source/Storm/H/storm.h"
#include <freetype/ftoutln.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace RovSlug {
struct Point { float x, y; };
struct Curve { Point p[3]; };
struct Builder {
    std::vector<Curve> curves;
    Point first{}, last{};
    bool active = false;
    static Point point(const FT_Vector* p) { return {float(p->x) / 64, float(p->y) / 64}; }
    bool add(Point control, Point end) {
        if (curves.size() >= MaxCurves) return false;
        curves.push_back({{last, control, end}});
        last = end;
        return true;
    }
    bool close() {
        if (active && (last.x != first.x || last.y != first.y)) return add(first, first);
        return true;
    }
    static int move(FT_Vector* to, void* user) {
        auto& b = *static_cast<Builder*>(user);
        if (!b.close()) return 1;
        b.first = b.last = point(to); b.active = true;
        return 0;
    }
    static int line(FT_Vector* to, void* user) {
        auto& b = *static_cast<Builder*>(user);
        const auto p = point(to);
        return b.add(p, p) ? 0 : 1;
    }
    static int conic(FT_Vector* control, FT_Vector* to, void* user) {
        return static_cast<Builder*>(user)->add(point(control), point(to)) ? 0 : 1;
    }
    // Selected 70862 fonts are TrueType quadratic outlines. Never approximate
    // a cubic or silently fall back to a different renderer.
    static int cubic(FT_Vector*, FT_Vector*, FT_Vector*, void*) { return 1; }
};
bool BuildGlyph(FT_Face face, uint32_t size, uint32_t code, uint32_t baseline, GLYPHBITMAPDATA* data) {
    if (!face || !size || !data) return false;
    const auto index = FT_Get_Char_Index(face, code);
    if (!index || FT_Set_Pixel_Sizes(face, size, 0) ||
        FT_Load_Glyph(face, index, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) ||
        face->glyph->format != ft_glyph_format_outline) return false;
    // The bundled FreeType predates FT_Outline_Check. Bound the contours before
    // its decomposer indexes their points; malformed owners never reach it.
    const auto& outline = face->glyph->outline;
    if (outline.n_points < 0 || outline.n_contours < 0 || outline.n_contours > outline.n_points ||
        (outline.n_points && (!outline.points || !outline.tags || !outline.contours || !outline.n_contours))) return false;
    int previous = -1;
    for (int i = 0; i < outline.n_contours; ++i) {
        const int end = outline.contours[i];
        if (end <= previous || end >= outline.n_points) return false;
        previous = end;
    }
    if (previous + 1 != outline.n_points) return false;
    Builder b;
    FT_Outline_Funcs callbacks{Builder::move, Builder::line, Builder::conic, Builder::cubic, 0, 0};
    if (FT_Outline_Decompose(&face->glyph->outline, &callbacks, &b) || !b.close()) return false;
    FT_BBox bounds;
    FT_Outline_Get_CBox(&face->glyph->outline, &bounds);
    // Geometry and packed control points have separate dimensions. The fixed
    // 32x32 data cell is a float array, not the glyph's image or screen bounds.
    const float left = std::floor(float(bounds.xMin) / 64) - 16;
    const float bottom = std::floor(float(bounds.yMin) / 64) - 16;
    const float width = std::ceil(float(bounds.xMax) / 64) + 16 - left;
    const float height = std::ceil(float(bounds.yMax) / 64) + 16 - bottom;
    if (!(width > 0 && width <= 4096 && height > 0 && height <= 4096)) return false;
    uint32_t packed[Cell * Cell]{};
    packed[0] = uint32_t(b.curves.size());
    size_t cursor = 1;
    for (const auto& c : b.curves) for (const auto& p : c.p) {
        const float x = (p.x - left) / width, y = (p.y - bottom) / height;
        std::memcpy(&packed[cursor++], &x, sizeof(float));
        std::memcpy(&packed[cursor++], &y, sizeof(float));
    }
    void* allocation = SMemAlloc(sizeof(packed), __FILE__, __LINE__, 0);
    if (!allocation) return false;
    std::memcpy(allocation, packed, sizeof(packed));
    data->m_data = allocation; data->m_dataSize = sizeof(packed);
    data->m_glyphWidth = uint32_t(width); data->m_glyphHeight = uint32_t(height);
    data->m_glyphCellWidth = Cell; data->m_glyphPitch = Cell * sizeof(uint32_t);
    data->m_glyphBearing = left;
    data->m_glyphAdvance = float(face->glyph->advance.x) / 64 + 1;
    data->m_yOffset = int32_t(size) - int32_t(baseline) + int32_t(bottom);
    data->m_yStart = 0;
    return true;
}
}
