#include "LoginWidgets.h"
#include "Storage.h"
#include "Scripts.h"
#include "Engine/Source/Frame/CSimpleHTML.h"
#include "Engine/Source/Frame/CSimpleScrollFrame.h"
#include "Engine/Source/Frame/CSimpleSlider.h"
#include "Engine/Source/FrameXML/Portable/Factories.h"
namespace Frame { namespace Native {
namespace {
template<class T> CSimpleFrame* Create(CSimpleFrame* parent) {
    return ConstructRegion<T>(T::s_allocator, parent);
}
}
LoginWidgets::LoginWidgets() {
    RequireRegionScripts(); RequireNoScripts();
    CSimpleHTML::CreateScriptMetaTable();
    CSimpleScrollFrame::CreateScriptMetaTable();
    CSimpleSlider::CreateScriptMetaTable();
    FrameXML::Native::RegisterLoginFactory("SimpleHTML", Create<CSimpleHTML>);
    FrameXML::Native::RegisterLoginFactory("ScrollFrame", Create<CSimpleScrollFrame>);
    FrameXML::Native::RegisterLoginFactory("Slider", Create<CSimpleSlider>);
}
LoginWidgets::~LoginWidgets() {
    try {
        RequireNoScripts();
        CSimpleSlider::DestroyScriptMetaTable();
        CSimpleScrollFrame::DestroyScriptMetaTable();
        CSimpleHTML::DestroyScriptMetaTable();
    } catch (...) { std::terminate(); }
}
}}
