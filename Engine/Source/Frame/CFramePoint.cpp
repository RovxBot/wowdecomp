#include <cstddef>
#include <cstdint>
#include <limits>
#include <typeinfo>
#include "Engine/Source/Frame/CFramePoint.h"
#include "Engine/Source/Frame/CLayoutFrame.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/CRect.h"

#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Runtime.h"
#endif

// STATIC: 0x00AC147C
float CFramePoint::UNDEFINED = std::numeric_limits<float>::infinity();

// STATIC: 0x00B499FC
CDataAllocator CFramePoint::s_framePointHeap(sizeof(CFramePoint), 0x1000);

// INLINED
void* CFramePoint::operator new(size_t bytes) noexcept {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    // noexcept allocation: reject foreign access before touching the pool.
    try { Frame::Native::RequireOwner(); } catch (...) { return nullptr; }
    return CFramePoint::s_framePointHeap.GetData(1, typeid(CFramePoint).name(), SLOG_OBJECT);
#else
    return CFramePoint::s_framePointHeap.GetData(0, typeid(CFramePoint).raw_name(), SLOG_OBJECT);
#endif
}

// INLINED
void CFramePoint::operator delete(void* ptr) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireOwner();
#endif
    if (ptr) {
        CFramePoint::s_framePointHeap.PutData(ptr, nullptr, 0);
    }
}

// VERIFY: 494f351d manual
// OFFSET: 0x0049C7B0
void CFramePoint::SetRelative(CLayoutFrame* relative, FRAMEPOINT framePoint, float x, float y) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireOwner();
#endif
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireFrame(relative);
    Frame::Native::RequirePoint(static_cast<uint32_t>(framePoint));
    Frame::Native::RequireFinite(x);
    Frame::Native::RequireFinite(y);
#endif
    this->m_x = x;
    this->m_framePoint = framePoint;
    this->m_y = y;
    this->m_relative = relative;

    uint32_t busy = this->m_flags & 2;
    this->m_flags = 0;

    if (busy) {
        this->m_flags |= 6;
    }
}

// VERIFY: 49ad92da manual
// OFFSET: 0x0049C7F0
void CFramePoint::MarkUnused() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireOwner();
#endif
    this->m_framePoint = FRAMEPOINT_NUMPOINTS;
    this->m_x = 0.0f;
    this->m_y = 0.0f;
    this->m_relative = nullptr;

    uint32_t busy = this->m_flags & 2;
    this->m_flags = 8;

    if (busy) {
        this->m_flags |= 6;
    }
}

// VERIFY: 6d9e350f manual
// OFFSET: 0x0049C830
int32_t CFramePoint::GetRelativeRect(CRect& rect) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireOwner();
#endif
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (!this->m_relative || (this->m_flags & 8)) return 0;
#endif
    uint32_t busy = this->m_flags & 2;
    int32_t result = 0;

    if (!busy) {
        this->m_flags |= 2;
    }

    this->m_flags &= ~4u;

    if (!this->m_relative->IsResizePending() || (this->m_relative->Resize(1), !(this->m_flags & 4))) {
        if (!(this->m_relative->m_layoutFlags & 0x200) || (this->m_relative->SetDeferredResize(0), !(this->m_flags & 4))) {
            if (this->m_relative->GetRect(&rect)) {
                if (this->m_relative->IsAttachmentOrigin()) {
                    float offsetX = -rect.minX;
                    float offsetY = -rect.minY;
                    rect.minY += offsetY;
                    rect.minX += offsetX;
                    rect.maxY += offsetY;
                    rect.maxX += offsetX;
                }

                result = 1;
            }
        }
    }

    if (!busy) {
        this->m_flags &= ~2u;
    }

    return result;
}

// VERIFY: 5879b0bd auto
// OFFSET: 0x0049C900
float CFramePoint::X(float scale) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireOwner();
    Frame::Native::RequireFinite(scale);
#endif
    CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (!this->GetRelativeRect(rect)) {
        return CFramePoint::UNDEFINED;
    }

    switch (this->m_framePoint) {
        case FRAMEPOINT_TOPLEFT:
        case FRAMEPOINT_LEFT:
        case FRAMEPOINT_BOTTOMLEFT:
            return this->m_x * scale + rect.minX;

        case FRAMEPOINT_TOP:
        case FRAMEPOINT_CENTER:
        case FRAMEPOINT_BOTTOM:
            return (rect.maxX + rect.minX) * 0.5f + this->m_x * scale;

        case FRAMEPOINT_TOPRIGHT:
        case FRAMEPOINT_RIGHT:
        case FRAMEPOINT_BOTTOMRIGHT:
            return this->m_x * scale + rect.maxX;

        default:
            return CFramePoint::UNDEFINED;
    }
}

// VERIFY: 73d76a03 auto
// OFFSET: 0x0049C9A0
float CFramePoint::Y(float scale) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireOwner();
    Frame::Native::RequireFinite(scale);
#endif
    CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (!this->GetRelativeRect(rect)) {
        return CFramePoint::UNDEFINED;
    }

    switch (this->m_framePoint) {
        case FRAMEPOINT_TOPLEFT:
        case FRAMEPOINT_TOP:
        case FRAMEPOINT_TOPRIGHT:
            return this->m_y * scale + rect.maxY;

        case FRAMEPOINT_LEFT:
        case FRAMEPOINT_CENTER:
        case FRAMEPOINT_RIGHT:
            return (rect.minY + rect.maxY) * 0.5f + this->m_y * scale;

        case FRAMEPOINT_BOTTOMLEFT:
        case FRAMEPOINT_BOTTOM:
        case FRAMEPOINT_BOTTOMRIGHT:
            return this->m_y * scale + rect.minY;

        default:
            return CFramePoint::UNDEFINED;
    }
}

// VERIFY: 9e24f527 manual
// OFFSET: 0x0049CA40
CFramePoint::CFramePoint(CLayoutFrame* relative, FRAMEPOINT framePoint, float x, float y) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireOwner();
    Frame::Native::RequireFrame(relative);
    Frame::Native::RequirePoint(static_cast<uint32_t>(framePoint));
    Frame::Native::RequireFinite(x);
    Frame::Native::RequireFinite(y);
    this->m_flags = 0;
#endif
    this->m_x = x;
    this->m_framePoint = framePoint;
    this->m_y = y;
    this->m_relative = relative;

    uint32_t busy = this->m_flags & 2;
    this->m_flags = 0;

    if (busy) {
        this->m_flags |= 6;
    }
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::PointCreated();
#endif
}

#ifdef WOWDECOMP_PORTABLE_RUNTIME
CFramePoint::~CFramePoint() { Frame::Native::PointDestroyed(); }
#endif
