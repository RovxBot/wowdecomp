#ifndef WOWDECOMP_NATIVE_FRAME_FONTS_H
#define WOWDECOMP_NATIVE_FRAME_FONTS_H
#include "Runtime.h"
#include "Engine/Source/Gxu/Portable/LegacyFonts.h"
#include <cstddef>
class CSimpleFont;
class CSimpleFontable;
class CScriptObject;
namespace Frame { namespace Native {
// Install inside the layout/XML/VM and Gxu owners. Factory pointers are borrowed;
// close external fontables/script objects before this owner clears named fonts.
class Fonts final {
public:
    Fonts();
    ~Fonts();
    Fonts(const Fonts&) = delete;
    Fonts& operator=(const Fonts&) = delete;
    void CheckCanClose() const;
private:
    OwnerLease layout;
    Gxu::Native::FontOwnerLease gxu;
};
void RequireFontFactories();
// Batcher brackets borrowed Frame consumers during callback execution.
void SetFontRenderActive(bool);
void FontCreated(CSimpleFont*);
void FontDestroyed(CSimpleFont*);
void FontableCreated(CSimpleFontable*);
void FontableDestroyed(CSimpleFontable*);
void ScriptCreated(CScriptObject*);
void ScriptDestroyed(CScriptObject*);
void RequireFont(CSimpleFont*);
void RequireFontParent(CSimpleFontable*, CSimpleFont*);
void RequireFontName(const char*);
void RequireFontCapacity();
void RequireScript(CScriptObject*);
struct FontUpdate final {
    FontUpdate();
    ~FontUpdate();
    FontUpdate(const FontUpdate&) = delete;
    FontUpdate& operator=(const FontUpdate&) = delete;
};
}}
#endif
