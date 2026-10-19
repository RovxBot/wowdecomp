#ifndef WOWDECOMP_NATIVE_FONT_FACE_H
#define WOWDECOMP_NATIVE_FONT_FACE_H
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace Gxu { namespace Native {
struct Glyph {
    uint32_t code = 0, width = 0, height = 0;
    int32_t bearing = 0, top = 0;
    float advance = 0;
    std::vector<uint8_t> coverage; // top-down, tightly packed 8-bit coverage
};
// Owner-thread, retained memory face through bundled FreeType. This native CPU
// boundary is not a legacy HFACE/CGxFont/cache/layout export. Font bytes must
// come through the authenticated package service in the application.
class FontFace {
public:
    explicit FontFace(std::vector<uint8_t> bytes);
    ~FontFace();
    FontFace(const FontFace&) = delete;
    FontFace& operator=(const FontFace&) = delete;
    std::string Family() const;
    // Match original unhinted pedantic load/render flags, pixel-size cap and
    // integral advance +1. Missing glyphs fail explicitly. Empty coverage for
    // whitespace remains valid and keeps its advance.
    Glyph Raster(uint32_t code, uint32_t pixels, bool monochrome = false);
    // Original negative-only unscaled kerning and ceil(advance + pixel kerning).
    float Step(uint32_t code, uint32_t next, uint32_t pixels);
private:
    struct State;
    std::unique_ptr<State> state;
};
}}
#endif
