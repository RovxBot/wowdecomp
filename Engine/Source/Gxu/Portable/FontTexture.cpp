// Native callback-only CTexture ownership for original Gxu atlases. Adapted from
// product Services/Texture.cpp (12340); no SFile/CAsyncObject or donor ABI export.
#include "LegacyFonts.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Gx/Gx.h"
#include <memory>
#include <stdexcept>
HASHKEY_TEXTUREFILE::HASHKEY_TEXTUREFILE() : m_filename(nullptr), m_anyTexFlags(0) {}
CTexture::CTexture() : m_flags(0), m_bestMip(0), m_alphaBits(0), m_asyncObject(nullptr),
    m_gxTex(nullptr), m_gxTexTarget(GxTex_2d), m_gxWidth(0), m_gxHeight(0),
    m_gxTexFormat(GxTex_Unknown), m_dataFormat(GxTex_Unknown), m_atlas(nullptr),
    m_atlasBlockIndex(-1) { m_filename[0] = '\0'; }
CTexture::~CTexture() { if (m_gxTex) GxTexDestroy(m_gxTex); }
const char* CTexture::GetObjectName() { return m_filename; }
CTexture* TextureCreate(uint32_t width, uint32_t height, EGxTexFormat format,
    EGxTexFormat dataFormat, CGxTexFlags flags, void* user, GxTexUserFunc callback,
    const char* name, int32_t defaultFilter) {
    Gxu::Native::RequireFontOwner();
    if (defaultFilter || width != 256 || height != 256 || format != GxTex_Argb4444 ||
        dataFormat != format || !callback)
        throw std::invalid_argument("Native Gxu texture service requires a 256x256 ARGB4444 callback atlas");
    auto texture = std::make_unique<CTexture>();
    if (!GxTexCreate(GxTex_2d, width, height, 0, format, dataFormat, flags, user, callback,
                     name, texture->m_gxTex))
        throw std::runtime_error("Native Gxu atlas allocation failed");
    texture->m_gxWidth = width;
    texture->m_gxHeight = height;
    texture->m_gxTexFormat = format;
    texture->m_dataFormat = dataFormat;
    texture->m_gxTexFlags = flags;
    SStrCopy(texture->m_filename, name ? name : "GxuFont", sizeof(texture->m_filename));
    HandleCreate(texture.get(), "HTEXTURE");
    return texture.release();
}
CGxTex* TextureGetGxTex(CTexture* texture, int32_t, CStatus*) {
    Gxu::Native::RequireFontOwner();
    return texture ? texture->m_gxTex : nullptr;
}
