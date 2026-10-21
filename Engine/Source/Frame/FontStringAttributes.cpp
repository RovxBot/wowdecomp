// Original 12340 string consumer separated from reusable font attributes.
#include "Engine/Source/Frame/CSimpleFontStringAttributes.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Storm/H/storm.h"

// VERIFY: 988687b9 manual
// OFFSET: 0x004872F0
void CSimpleFontStringAttributes::Update(CSimpleFontString* string, int32_t flags) {
    if (!string) {
        DebugValidate("string");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    uint32_t mask = this->m_flags & flags;

    if ((this->m_flags & 0x100) && (mask & 0x1)) {
        string->SetFont(this->m_font.m_str, this->m_fontHeight, this->m_fontFlags, false);
    }

    if ((this->m_flags & 0x200) && (mask & 0x2)) {
        string->SetStyleFlags(this->m_styleFlags);
    }

    if ((this->m_flags & 0x400) && (mask & 0x4)) {
        string->SetVertexColor(this->m_color);
    }

    if ((this->m_flags & 0x800) && (mask & 0x8)) {
        if (this->m_shadowOffset.x == 0.0f && this->m_shadowOffset.y == 0.0f) {
            string->RemoveShadow();
        } else {
            string->AddShadow(this->m_shadowColor, this->m_shadowOffset);
        }
    }

    if ((this->m_flags & 0x1000) && (mask & 0x10)) {
        string->SetSpacing(this->m_spacing);
    }
}

