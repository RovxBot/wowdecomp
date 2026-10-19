#ifndef ENGINE_SOURCE_GXU_GXUFONTFACE_H
#define ENGINE_SOURCE_GXU_GXUFONTFACE_H

#include <cstddef>
#include <cstdint>
#include <freetype/freetype.h>
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Gxu/Types.h"
#include "Engine/Source/Storm/H/stpl.h"
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
#include <vector>
#endif

struct FACEDATA : public CHandleObject, public TSHashObject<FACEDATA, HASHKEY_STRI> {
    /* 0020 */ void* m_data;
    /* 0024 */ FT_Face m_face;
    /* 0028 */ HOBJECT m_selfReference;
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    // Native ABI ownership; address/layout annotations above describe 12340 only.
    std::vector<uint8_t> m_nativeBytes;
#endif

    FACEDATA();
    virtual ~FACEDATA();
};

extern TSHashTable<FACEDATA, HASHKEY_STRI> s_faceHash;

void FontFaceCloseHandle(HFACE handle);
FT_Face FontFaceGetFace(HFACE handle);
const char* FontFaceGetFontName(HFACE handle);
HFACE FontFaceGetHandle(const char* fileName, FT_Library library);

#endif
