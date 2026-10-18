#include "FontFace.h"
#include "Engine/Source/Storm/H/storm.h"
#include <freetype/freetype.h>
#include <freetype/ftmodule.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>
namespace Gxu { namespace Native {
namespace {
void CheckCode(uint32_t code) {
    if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
        throw std::invalid_argument("Invalid Unicode font code point");
}
void* Allocate(FT_Memory, long size) {
    return size > 0 ? SMemAlloc(static_cast<size_t>(size), __FILE__, __LINE__, 0) : nullptr;
}
void Free(FT_Memory, void* block) { if (block) SMemFree(block, __FILE__, __LINE__, 0); }
void* Reallocate(FT_Memory, long, long size, void* block) {
    if (size < 0) return nullptr;
    if (!size) { Free(nullptr, block); return nullptr; }
    return SMemReAlloc(block, static_cast<size_t>(size), __FILE__, __LINE__, 0);
}
}
struct FontFace::State {
    std::thread::id owner = std::this_thread::get_id();
    std::vector<uint8_t> bytes;
    FT_MemoryRec_ memory{nullptr, Allocate, Free, Reallocate};
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    explicit State(std::vector<uint8_t> source) : bytes(std::move(source)) {}
    ~State() {
        if (owner != std::this_thread::get_id()) std::terminate();
        if (face) FT_Done_Face(face);
        if (library) FT_Done_Library(library);
    }
    void Check() const {
        if (owner != std::this_thread::get_id()) throw std::logic_error("Font face requires its owner thread");
    }
    void Size(uint32_t pixels) {
        Check();
        if (!pixels || pixels > 32) throw std::out_of_range("Original Gxu font size must be 1..32 pixels");
        if (FT_Set_Pixel_Sizes(face, pixels, 0)) throw std::runtime_error("FreeType rejected native pixel size");
    }
};
FontFace::FontFace(std::vector<uint8_t> bytes) {
    if (bytes.empty() || bytes.size() > 32 * 1024 * 1024u)
        throw std::invalid_argument("Native font source must be 1..32 MiB");
    state = std::make_unique<State>(std::move(bytes));
    auto& s = *state;
    if (FT_New_Library(&s.memory, &s.library)) throw std::runtime_error("Create bundled FreeType library failed");
    FT_Add_Default_Modules(s.library);
    FT_Face parsed = nullptr;
    if (FT_New_Memory_Face(s.library, s.bytes.data(), static_cast<FT_Long>(s.bytes.size()), 0, &parsed))
        throw std::runtime_error("Native font face decoding failed");
    s.face = parsed;
    if (!s.face || FT_Select_Charmap(s.face, ft_encoding_unicode) || !FT_IS_SCALABLE(s.face) || !s.face->units_per_EM)
        throw std::runtime_error("Native Unicode scalable font face unavailable");
}
FontFace::~FontFace() = default;
std::string FontFace::Family() const {
    state->Check();
    return state->face->family_name ? state->face->family_name : "";
}
Glyph FontFace::Raster(uint32_t code, uint32_t pixels, bool monochrome) {
    auto& s = *state;
    s.Check(); CheckCode(code); s.Size(pixels);
    auto index = FT_Get_Char_Index(s.face, code);
    if (!index) throw std::out_of_range("Font does not contain requested glyph");
    FT_Int32 flags = FT_LOAD_LINEAR_DESIGN | FT_LOAD_PEDANTIC | FT_LOAD_NO_HINTING |
        (monochrome ? FT_LOAD_CROP_BITMAP : FT_LOAD_NO_BITMAP);
    if (FT_Load_Glyph(s.face, index, flags) || FT_Render_Glyph(s.face->glyph, static_cast<FT_Render_Mode>(monochrome)))
        throw std::runtime_error("Bundled FreeType glyph rendering failed");
    const auto& bitmap = s.face->glyph->bitmap;
    if (bitmap.width < 0 || bitmap.rows < 0 || bitmap.width > 4096 || bitmap.rows > 4096 ||
        uint64_t(bitmap.width) * bitmap.rows > 8 * 1024 * 1024u)
        throw std::out_of_range("Font glyph bitmap exceeds native bounds");
    Glyph result;
    result.code = code; result.width = bitmap.width; result.height = bitmap.rows;
    result.bearing = s.face->glyph->bitmap_left; result.top = s.face->glyph->bitmap_top;
    result.advance = static_cast<float>(s.face->glyph->metrics.horiAdvance / 64) + 1;
    if (!std::isfinite(result.advance) || std::abs(result.advance) > 4096 ||
        std::abs(int64_t(result.bearing)) > 4096 || std::abs(int64_t(result.top)) > 4096)
        throw std::out_of_range("Font glyph metrics exceed native bounds");
    if (!result.width || !result.height) return result;
    bool mono = bitmap.pixel_mode == ft_pixel_mode_mono;
    if ((!mono && bitmap.pixel_mode != ft_pixel_mode_grays) || (!mono && bitmap.num_grays != 256) || !bitmap.buffer)
        throw std::runtime_error("Unsupported native font bitmap pixel mode");
    int64_t pitch = bitmap.pitch;
    uint64_t stride = pitch < 0 ? uint64_t(-pitch) : uint64_t(pitch);
    if (stride < (mono ? (result.width + 7) / 8 : result.width) || stride * result.height > 8 * 1024 * 1024u)
        throw std::out_of_range("Font glyph pitch exceeds native bounds");
    result.coverage.resize(size_t(result.width) * result.height);
    for (uint32_t y = 0; y < result.height; ++y) {
        const auto* row = bitmap.buffer + size_t(pitch < 0 ? result.height - 1 - y : y) * stride;
        for (uint32_t x = 0; x < result.width; ++x)
            result.coverage[size_t(y) * result.width + x] = mono ? ((row[x >> 3] >> (7 - (x & 7))) & 1) * 255 : row[x];
    }
    return result;
}
float FontFace::Step(uint32_t code, uint32_t next, uint32_t pixels) {
    auto glyph = Raster(code, pixels);
    auto& s = *state; CheckCode(next);
    FT_Vector kerning{};
    auto index = FT_Get_Char_Index(s.face, next);
    if (!index) throw std::out_of_range("Font does not contain next glyph");
    if (FT_HAS_KERNING(s.face) && FT_Get_Kerning(s.face, FT_Get_Char_Index(s.face, code), index, ft_kerning_unscaled, &kerning))
        throw std::runtime_error("Bundled FreeType kerning failed");
    double step = glyph.advance + std::min<FT_Pos>(0, kerning.x) *
        (double(s.face->size->metrics.x_ppem) / s.face->units_per_EM);
    if (!std::isfinite(step) || std::abs(step) > 4096) throw std::out_of_range("Font advance exceeds native bounds");
    return static_cast<float>(std::ceil(step));
}
}}
