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
