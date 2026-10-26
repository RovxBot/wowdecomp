#include "LoginWidgets.h"
#include "Storage.h"
#include "WoW/Source/Glue/Portable/Models.h"
#include "Scripts.h"
#include "Engine/Source/Frame/CSimpleHTML.h"
#include "Engine/Source/Frame/CSimpleMovieFrame.h"
#include "Engine/Source/Frame/CSimpleScrollFrame.h"
#include "Engine/Source/Frame/CSimpleSlider.h"
#include "Engine/Source/FrameXML/Portable/Factories.h"
namespace Frame { namespace Native {
namespace {
template<class T> CSimpleFrame* Create(CSimpleFrame* parent) {
    return ConstructRegion<T>(T::s_allocator, parent);
}
void ScrollChanged(CSimpleFrame* frame) {
    if (!(frame->m_frameFlags & 0x2000) || !frame->m_isVisible) return;
    size_t depth = 0;
    while (frame && !(frame->m_frameFlags & 0x4000)) {
        if (++depth > 64) throw std::logic_error("Invalid scroll ancestor graph");
        frame = frame->m_parent;
    }
    if (!frame) throw std::logic_error("Scrolled Frame has no scroll child root");
    auto* parent = dynamic_cast<CSimpleScrollFrame*>(frame->m_parent);
    if (parent && parent->m_lifecycleState != 3) parent->m_scrollChildUpdate = 1;
}
}
LoginWidgets::LoginWidgets() {
    RequireRegionScripts(); RequireNoScripts();
    CSimpleHTML::CreateScriptMetaTable();
    CSimpleScrollFrame::CreateScriptMetaTable();
    CSimpleSlider::CreateScriptMetaTable();
    CSimpleMovieFrame::CreateScriptMetaTable();
    FrameXML::Native::RegisterLoginFactory("SimpleHTML", Create<CSimpleHTML>);
    FrameXML::Native::RegisterLoginFactory("ScrollFrame", Create<CSimpleScrollFrame>);
    FrameXML::Native::RegisterLoginFactory("Slider", Create<CSimpleSlider>);
    FrameXML::Native::RegisterLoginFactory("MovieFrame", Create<CSimpleMovieFrame>);
    GlueNative::RegisterModels();
    SetScrollNotify(ScrollChanged);
}
LoginWidgets::~LoginWidgets() {
    try {
        RequireNoScripts();
        SetScrollNotify(nullptr);
        GlueNative::UnregisterModels();
        CSimpleMovieFrame::DestroyScriptMetaTable();
        CSimpleSlider::DestroyScriptMetaTable();
        CSimpleScrollFrame::DestroyScriptMetaTable();
        CSimpleHTML::DestroyScriptMetaTable();
    } catch (...) { std::terminate(); }
}
}}
