// Native callback-only CTexture ownership for original Gxu atlases. Adapted from
// product Services/Texture.cpp (12340); no SFile/CAsyncObject or donor ABI export.
#include "LegacyFonts.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Services/Portable/Texture.h"
#include <memory>
#include <stdexcept>
HASHKEY_TEXTUREFILE::HASHKEY_TEXTUREFILE() : m_filename(nullptr), m_anyTexFlags(0) {}
CTexture::CTexture() : m_flags(0), m_bestMip(0), m_alphaBits(0), m_asyncObject(nullptr),
    m_gxTex(nullptr), m_gxTexTarget(GxTex_2d), m_gxWidth(0), m_gxHeight(0),
    m_gxTexFormat(GxTex_Unknown), m_dataFormat(GxTex_Unknown), m_atlas(nullptr),
    m_atlasBlockIndex(-1) { m_filename[0] = '\0'; }
CTexture::~CTexture() {
    NativeCheckOwner();
    // Drop native Gx bindings before releasing retained callback pixels.
    if (m_gxTex) { GxTexDestroy(m_gxTex); m_gxTex = nullptr; }
    Services::Native::ReleaseFileTexture(this);
}
const char* CTexture::GetObjectName() { return m_filename; }
CTexture* TextureCreate(uint32_t width, uint32_t height, EGxTexFormat format,
    EGxTexFormat dataFormat, CGxTexFlags flags, void* user, GxTexUserFunc callback,
    const char* name, int32_t defaultFilter) {
    return TextureCreate(GxTex_2d, width, height, 0, format, dataFormat, flags,
        user, callback, name, defaultFilter);
}
CTexture* TextureCreate(EGxTexTarget target, uint32_t width, uint32_t height,
    uint32_t depth, EGxTexFormat format, EGxTexFormat dataFormat, CGxTexFlags flags,
    void* user, GxTexUserFunc callback, const char* name, int32_t defaultFilter) {
    Gxu::Native::RequireFontOwner();
    if (defaultFilter || !width || !height || width > UINT16_MAX || height > UINT16_MAX || !callback)
        throw std::invalid_argument("Unavailable native callback texture dimensions or filtering");
    auto texture = std::make_unique<CTexture>();
    if (!GxTexCreate(target, width, height, depth, format, dataFormat, flags, user, callback,
                     name, texture->m_gxTex))
        throw std::runtime_error("Native callback texture allocation failed");
    texture->m_gxTexTarget = target;
    texture->m_gxWidth = width;
    texture->m_gxHeight = height;
    texture->m_gxTexFormat = format;
    texture->m_dataFormat = dataFormat;
    texture->m_gxTexFlags = flags;
    SStrCopy(texture->m_filename, name ? name : "GxuFont", sizeof(texture->m_filename));
    HandleCreate(texture.get(), "HTEXTURE");
    return texture.release();
}
void GxTexNullUserFunc(EGxTexCommand, uint32_t, uint32_t, uint32_t, uint32_t,
    void*, uint32_t& stride, const void*& pixels) { stride = 0; pixels = nullptr; }
int32_t TextureCanUpdate(CTexture* texture) {
    if (!texture) return 0;
    texture->NativeCheckOwner();
    return texture->m_gxTex && g_theGxDevicePtr->TexCanUpdate(texture->m_gxTex);
}
CGxTex* TextureGetGxTex(CTexture* texture, int32_t wait, CStatus* status) {
    return Services::Native::GetFileTexture(texture, wait, status);
}

namespace {
class SolidTexture final : public CTexture {
public:
    CImVector pixels[64];
    ~SolidTexture() override {
        if (m_gxTex) { GxTexDestroy(m_gxTex); m_gxTex = nullptr; }
    }
    static void Source(EGxTexCommand command, uint32_t width, uint32_t height,
        uint32_t face, uint32_t level, void* user, uint32_t& stride, const void*& bytes) {
        if (width != 8 || height != 8 || face || level)
            throw std::logic_error("Invalid native solid-texture callback extent");
        if (command == GxTex_Latch) {
            stride = 8 * sizeof(CImVector);
            bytes = static_cast<SolidTexture*>(user)->pixels;
        }
    }
};
}
// Bounded native adaptation of TextureCreateSolid/FillInSolidTexture (12340).
// Original 8x8 ARGB storage and alpha threshold; no file ticket or atlas/cache key.
CTexture* TextureCreateSolid(const CImVector& color) {
    Gxu::Native::RequireFontOwner();
    auto texture = std::make_unique<SolidTexture>();
    for (auto& pixel : texture->pixels) pixel = color;
    texture->m_gxWidth = texture->m_gxHeight = 8;
    texture->m_gxTexFormat = texture->m_dataFormat = GxTex_Argb8888;
    texture->m_gxTexFlags = CGxTexFlags(GxTex_Nearest);
    texture->m_flags = color.a >= 0xFE ? 1 : 0;
    texture->m_alphaBits = color.a >= 0xFE ? 0 : 8;
    SStrCopy(texture->m_filename, "SolidTexture", sizeof(texture->m_filename));
    if (!GxTexCreate(GxTex_2d, 8, 8, 0, GxTex_Argb8888, GxTex_Argb8888,
        texture->m_gxTexFlags, texture.get(), &SolidTexture::Source, texture->m_filename, texture->m_gxTex))
        throw std::runtime_error("Native solid-texture allocation failed");
    GxTexUpdate(texture->m_gxTex, 0, 0, 8, 8, 1);
    HandleCreate(texture.get(), "HTEXTURE");
    return texture.release();
}
