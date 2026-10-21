#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Fonts.h"
#define FRAME_FONT_CHECK() Frame::Native::RequireFontFactories()
#else
#define FRAME_FONT_CHECK() ((void)0)
#endif
#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleFontable.h"
#include "Engine/Source/Frame/CSimpleFont.h"

// INLINED
CSimpleFontable::CSimpleFontable() {
    FRAME_FONT_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::FontableCreated(this);
#endif
    this->m_fontableFlags = 0x1F;
    this->m_fontObject = nullptr;
}

// VERIFY: 85bbcb87 auto
// OFFSET: [0x004851B0, 0x00485210]
CSimpleFontable::~CSimpleFontable() {
    FRAME_FONT_CHECK();
    this->SetFontObject(nullptr);
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::FontableDestroyed(this);
#endif
}

// VERIFY: c87fff35 auto
// OFFSET: 0x00485160
void CSimpleFontable::SetFontObject(CSimpleFont* font) {
    FRAME_FONT_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireFontParent(this, font);
#endif
    if (this->m_fontObject == font) {
        return;
    }

    if (this->m_fontObject) {
        this->m_fontObject->RemoveFontable(this);
    }

    this->m_fontObject = font;

    if (font) {
        font->m_fontableList.LinkToTail(this);
        font->m_attributes.m_flags |= 0x1F;
        this->FontObjectUpdated(font->m_attributes);
        font->m_attributes.m_flags &= ~0x1Fu;
    }
}
