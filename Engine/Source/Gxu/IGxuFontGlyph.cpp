#include <cstddef>
#include <cstdint>
#include <cstring>
#include "Engine/Source/Gxu/IGxuFontGlyph.h"
#include "Engine/Source/Gxu/GxuFontMiscClasses.h"
#include "Engine/Source/Storm/H/storm.h"
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
#include "Engine/Source/Gxu/Portable/LegacyFonts.h"
#include <algorithm>
#include <cmath>
#endif

// VERIFY: fff26b99 manual
// OFFSET: 0x006C8C10
static int32_t FREETYPE_RenderGlyph(FT_Face face, uint32_t code, bool monochrome) {
    FT_UInt index = FT_Get_Char_Index(face, code);

    if (!index || FT_Load_Glyph(face, index, monochrome ? FT_LOAD_LINEAR_DESIGN | FT_LOAD_PEDANTIC | FT_LOAD_CROP_BITMAP | FT_LOAD_NO_HINTING : FT_LOAD_LINEAR_DESIGN | FT_LOAD_PEDANTIC | FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING)) {
        return 0;
    }

    return FT_Render_Glyph(face->glyph, static_cast<FT_Render_Mode>(monochrome)) == FT_Err_Ok;
}

// VERIFY: d81e8b25 manual
// OFFSET: 0x006C8C60
static void CalculateYOffset(uint32_t pixelSize, int32_t baseline, FT_Face face, uint32_t glyphHeight, int32_t* yOffset, uint32_t* yStart) {
    int32_t offset = 0;
    int32_t start;

    if (glyphHeight <= pixelSize) {
        int32_t top = face->glyph->bitmap_top;

        if (top <= baseline) {
            start = baseline - top;
        } else {
            start = 0;
            offset = top - baseline;
        }
    } else {
        start = 0;
    }

    start = start <= 0 ? 0 : start;

    int32_t available = pixelSize - glyphHeight;

    if (available >= start) {
        *yOffset = offset;
        *yStart = start;
    } else {
        *yOffset = pixelSize - start - glyphHeight;
        *yStart = pixelSize - glyphHeight;
    }
}

// VERIFY: 86715750 manual
// OFFSET: 0x006C8CC0
bool IGxuFontGlyphRenderGlyph(FT_Face face, uint32_t pixelSize, uint32_t code, uint32_t baseline, GLYPHBITMAPDATA* dataPtr, int32_t monochrome, uint32_t padding) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    // Native ABI bounds and signed-pitch ownership; the 12340 VERIFY applies to
    // the retained Windows branch. Preserve original load flags and advances.
    Gxu::Native::RequireFontOwner();
    if (!face || !pixelSize || pixelSize > 32 || !dataPtr || padding > 4 ||
        code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return false;
    if (!FREETYPE_RenderGlyph(face, code, monochrome != 0)) return false;
    const auto& bitmap = face->glyph->bitmap;
    if (bitmap.width < 0 || bitmap.width > 256 - int32_t(padding) ||
        bitmap.rows < 0 || bitmap.rows > 4096) return false;
    uint32_t width = bitmap.width;
    uint32_t height = std::min<uint32_t>(bitmap.rows, pixelSize);
    bool placeholder = !width || !height || !bitmap.buffer || !bitmap.pitch;
    if (placeholder) { width = (pixelSize + 3) >> 2; height = pixelSize; }
    uint32_t dataPitch = monochrome ? (width + 7) / 8 : width;
    int64_t pitch = bitmap.pitch;
    uint64_t stride = pitch < 0 ? uint64_t(-pitch) : uint64_t(pitch);
    if (!placeholder && (stride < dataPitch || stride * bitmap.rows > 8 * 1024 * 1024u ||
        bitmap.pixel_mode != (monochrome ? ft_pixel_mode_mono : ft_pixel_mode_grays))) return false;
    GLYPHBITMAPDATA data;
    data.m_dataSize = dataPitch * height;
    data.m_data = SMemAlloc(data.m_dataSize, __FILE__, __LINE__, SMEM_FLAG_ZEROMEMORY);
    if (!data.m_data) return false;
    if (!placeholder) {
        for (uint32_t y = 0; y < height; ++y)
            memcpy(static_cast<uint8_t*>(data.m_data) + y * dataPitch,
                bitmap.buffer + static_cast<ptrdiff_t>(y) * pitch, dataPitch);
    }
    data.m_glyphCellWidth = width + padding;
    data.m_glyphPitch = dataPitch;
    data.m_glyphWidth = width;
    data.m_glyphHeight = height;
    data.m_glyphAdvance = (face->glyph->metrics.horiAdvance / 64) + 1.0;
    data.m_glyphBearing = face->glyph->metrics.horiBearingX * (1.0 / 64.0);
    if (!std::isfinite(data.m_glyphAdvance) || std::abs(data.m_glyphAdvance) > 4096 ||
        !std::isfinite(data.m_glyphBearing) || std::abs(data.m_glyphBearing) > 4096) return false;
    data.m_yOffset = 0;
    data.m_yStart = 0;
    if (!placeholder) CalculateYOffset(pixelSize, baseline, face, height, &data.m_yOffset, &data.m_yStart);
    dataPtr->CopyFrom(&data);
    return true;
#else
    if (!face || !pixelSize || !dataPtr) {
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return false;
    }

    if (!FT_Get_Char_Index(face, code)) {
        return false;
    }

    if (!FREETYPE_RenderGlyph(face, code, monochrome != 0)) {
        return false;
    }

    uint32_t width = face->glyph->bitmap.width;
    uint32_t height = pixelSize >= static_cast<uint32_t>(face->glyph->bitmap.rows) ? face->glyph->bitmap.rows : pixelSize;
    int32_t pitch = face->glyph->bitmap.pitch;
    uint32_t size = pitch * face->glyph->bitmap.rows;
    const uint8_t* source = face->glyph->bitmap.buffer;
    uint32_t dataPitch = pitch;
    int32_t placeholder = 0;

    if (!width || !height || !source || !pitch || !size) {
        width = (pixelSize + 3) >> 2;
        height = pixelSize;

        if (!width) {
            width = pixelSize;
        }

        if (monochrome) {
            dataPitch = (width + 7) & ~7;
        } else {
            dataPitch = width;
        }

        size = pixelSize * dataPitch;
        placeholder = 1;
    }

    void* data = SMemAlloc(size, __FILE__, __LINE__, 0);

    if (data) {
        memset(data, 0, size);
    }

    if (source) {
        memcpy(data, source, size);
    }

    dataPtr->m_glyphCellWidth = width + padding;
    dataPtr->m_dataSize = size;
    dataPtr->m_glyphPitch = dataPitch;
    dataPtr->m_data = data;
    dataPtr->m_glyphWidth = width;
    dataPtr->m_glyphHeight = height;
    dataPtr->m_glyphAdvance = (face->glyph->metrics.horiAdvance / 64) + 1.0;
    dataPtr->m_yOffset = 0;
    dataPtr->m_yStart = 0;
    dataPtr->m_glyphBearing = face->glyph->metrics.horiBearingX * (1.0 / 64.0);

    if (width && data && !placeholder) {
        CalculateYOffset(pixelSize, baseline, face, height, &dataPtr->m_yOffset, &dataPtr->m_yStart);
    }

    return true;
#endif
}
