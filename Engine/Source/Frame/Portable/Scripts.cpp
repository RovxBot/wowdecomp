// Lifetime for selected original 12340 region/frame/FontString Lua methods.
#include "Scripts.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CSimpleTitleRegion.h"
namespace Frame { namespace Native {
namespace { bool installed = false; }
void RequireRegionScripts() {
    RequireRegions();
    if (!installed) throw std::logic_error("Native region script metatables are not installed");
}
Scripts::Scripts() {
    RequireRegions();
    RequireNoScripts();
    if (installed) throw std::logic_error("Only one native region script owner may be installed");
    try {
        CSimpleTitleRegion::CreateScriptMetaTable();
        CSimpleFrame::CreateScriptMetaTable();
        CSimpleFontString::CreateScriptMetaTable();
        installed = true;
    } catch (...) {
        CSimpleFontString::DestroyScriptMetaTable();
        CSimpleFrame::DestroyScriptMetaTable();
        CSimpleTitleRegion::DestroyScriptMetaTable();
        throw;
    }
}
void Scripts::CheckCanClose() const {
    RequireRegionScripts();
    RequireNoScripts();
}
Scripts::~Scripts() {
    try {
        CheckCanClose();
        CSimpleFontString::DestroyScriptMetaTable();
        CSimpleFrame::DestroyScriptMetaTable();
        CSimpleTitleRegion::DestroyScriptMetaTable();
        installed = false;
    } catch (...) { std::terminate(); }
}
}}
