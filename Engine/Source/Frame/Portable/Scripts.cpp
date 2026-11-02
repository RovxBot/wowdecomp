// Lifetime for selected original 12340 region/frame/FontString Lua methods.
#include "Scripts.h"
#include "Engine/Source/Frame/CSimpleEditBox.h"
#include "Engine/Source/Frame/CSimpleButton.h"
#include "Engine/Source/Frame/CSimpleCheckbox.h"
#include "Engine/Source/Frame/CSimpleMessageFrame.h"
#include "Engine/Source/Frame/CSimpleStatusBar.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/FrameXML/Portable/Factories.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CSimpleTitleRegion.h"
#include "Engine/Source/Frame/CScriptRegion.h"
namespace Frame { namespace Native {
namespace { bool installed = false; }
int CallControlScript(lua_State* L,int (*method)(lua_State*),bool enabled) {
    if (!ControlScriptContext()) return -1;
    CScriptRegion* region=nullptr;
    if (lua_istable(L,1)) region=dynamic_cast<CScriptRegion*>(FrameScript_GetObjectThis(L,CScriptObject::GetObjectType()));
    const bool admitted=enabled && region && IsControlRegion(region);
    int status;
    {
        // Foreign targets and unqualified methods suspend an enclosing admission.
        // Contain Lua longjmp before restoring the prior per-call context.
        ControlScriptEdit edit(admitted);
        const int arguments=lua_gettop(L);
        lua_pushcfunction(L,method); lua_insert(L,1);
        status=lua_pcall(L,arguments,LUA_MULTRET,0);
    }
    if (status) return lua_error(L);
    return lua_gettop(L);
}
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
        CSimpleTexture::CreateScriptMetaTable();
        CSimpleButton::CreateScriptMetaTable();
        CSimpleCheckbox::CreateScriptMetaTable();
        CSimpleMessageFrame::CreateScriptMetaTable();
        CSimpleEditBox::CreateScriptMetaTable();
        CSimpleStatusBar::CreateScriptMetaTable();
        CSimpleTexture::Init();
        installed = true;
        FrameXML::Native::InstallFactories();
    } catch (...) {
        installed = false;
        CSimpleTexture::Destroy();
        CSimpleStatusBar::DestroyScriptMetaTable();
        CSimpleEditBox::DestroyScriptMetaTable();
        CSimpleMessageFrame::DestroyScriptMetaTable();
        CSimpleCheckbox::DestroyScriptMetaTable();
        CSimpleButton::DestroyScriptMetaTable();
        CSimpleTexture::DestroyScriptMetaTable();
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
        FrameXML::Native::CloseFactories();
        CSimpleTexture::Destroy();
        CSimpleStatusBar::DestroyScriptMetaTable();
        CSimpleEditBox::DestroyScriptMetaTable();
        CSimpleMessageFrame::DestroyScriptMetaTable();
        CSimpleCheckbox::DestroyScriptMetaTable();
        CSimpleButton::DestroyScriptMetaTable();
        CSimpleTexture::DestroyScriptMetaTable();
        CSimpleFontString::DestroyScriptMetaTable();
        CSimpleFrame::DestroyScriptMetaTable();
        CSimpleTitleRegion::DestroyScriptMetaTable();
        installed = false;
    } catch (...) { std::terminate(); }
}
}}
