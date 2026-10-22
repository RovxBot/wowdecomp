// Original base Frame allocation, shared with the Windows factory registry.
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Hierarchy.h"
#include "Engine/Source/Frame/Portable/Storage.h"
#endif

// VERIFY: 8993fe13 auto
// OFFSET: 0x008128A0
CSimpleFrame* Create_SimpleFrame(CSimpleFrame* parent) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    Frame::Native::RequireRoot();
    if (parent) Frame::Native::RequireSimpleFrame(parent);
    return Frame::Native::ConstructRegion<CSimpleFrame>(CSimpleFrame::s_allocator, parent);
#else
    void* data = CSimpleFrame::s_allocator.GetData(0, typeid(CSimpleFrame).raw_name(), SLOG_OBJECT);
    return data ? new (data) CSimpleFrame(parent) : nullptr;
#endif
}
