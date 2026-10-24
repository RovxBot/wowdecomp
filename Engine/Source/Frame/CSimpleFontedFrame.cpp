// Original 12340 private font shared by EditBox and MessageFrame consumers.
#include "Engine/Source/Frame/CSimpleFontedFrame.h"

// INLINED
CSimpleFontedFrameFont::CSimpleFontedFrameFont(CSimpleFontedFrame* frame) {
    this->m_frame = frame;
}

// VERIFY: ab4308a9 auto
// OFFSET: 0x00963540
void CSimpleFontedFrameFont::UpdateObjects() {
    uint32_t flags = this->m_attributes.m_flags;

    CSimpleFont::UpdateObjects();

    if (this->m_frame) {
        this->m_frame->FontUpdated(this, flags);
    }
}
