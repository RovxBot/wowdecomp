#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Scripts.h"
#include "Engine/Source/Frame/Portable/Storage.h"
#include "Engine/Source/Frame/CSimpleButton.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include <cstring>
#include <memory>
#define BUTTON_CHECK() Frame::Native::RequireScriptTextMutation()
namespace {
struct ClickFlag final {
    uint32_t& flag;
    uint32_t saved;
    explicit ClickFlag(uint32_t& value) : flag(value), saved(value) {}
    ~ClickFlag() { flag = saved; }
};
void CheckState(const CSimpleButtonState& state) {
    uint32_t raw;
    static_assert(sizeof(state) == sizeof(raw));
    std::memcpy(&raw, &state, sizeof(raw));
    if (raw >= NUM_BUTTONSTATES) throw std::invalid_argument("Invalid native Button state");
}
void CheckTexture(CSimpleButton* button, CSimpleTexture* texture) {
    Frame::Native::RequireTreeMutation();
    if (!texture) return;
    Frame::Native::RequireScript(texture);
    // One region cannot occupy two independently owned state/highlight slots.
    for (auto* slot : button->m_stateTextures)
        if (slot == texture) throw std::invalid_argument("Native Button texture already occupies a state slot");
    if (texture == button->m_highlightTexture)
        throw std::invalid_argument("Native Button texture already occupies the highlight slot");
}
}
#else
#define BUTTON_CHECK() ((void)0)
#endif
#include <cstddef>
#include <cstdint>
#include <new>
#include <typeinfo>
#include "Engine/Source/Frame/CSimpleButton.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Event/CMouseEvent.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CScriptRegion.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// STATIC: 0x00B2D528
int32_t CSimpleButton::s_metatable = LUA_NOREF;

// STATIC: 0x00DCE650
int32_t CSimpleButton::s_objectType;

// STATIC: 0x00B2CD1C
const char* CSimpleButton::s_objectTypeName = "Button";

// STATIC: 0x00DCE654
CDataAllocator CSimpleButton::s_allocator(sizeof(CSimpleButton), 100);

// INLINED
void CSimpleButton::operator delete(void* ptr) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::FreeRegionStorage(CSimpleButton::s_allocator, ptr);
#else
    if (ptr) {
        CSimpleButton::s_allocator.PutData(ptr, nullptr, 0);
    }
#endif
}

// INLINED
int32_t CSimpleButton::GetObjectType() {
    if (!CSimpleButton::s_objectType) {
        CSimpleButton::s_objectType = ++FrameScript_Object::s_objectTypes;
    }

    return CSimpleButton::s_objectType;
}

// VERIFY: cf256644 auto
// OFFSET: 0x009621E0
bool CSimpleButton::IsA(int32_t type) {
    BUTTON_CHECK();
    return type == CSimpleButton::s_objectType || type == CSimpleFrame::s_objectType || type == CScriptRegion::s_objectType || type == CScriptObject::s_objectType;
}

// VERIFY: 296f9512 auto
// OFFSET: 0x00962220
bool CSimpleButton::IsA(const char* typeName) {
    BUTTON_CHECK();
    return !SStrCmpI(typeName, CSimpleButton::s_objectTypeName, STORM_MAX_STR) || CSimpleFrame::IsA(typeName);
}

// VERIFY: 80dad073 auto
// OFFSET: 0x00962120
void CSimpleButton::LockHighlight(int32_t lock) {
    BUTTON_CHECK();
    CSimpleFrame::LockHighlight(lock);
    this->UpdateTextState(this->m_state);
}

// VERIFY: ab04eabe manual
// OFFSET: 0x00962150
void CSimpleButton::SetMotionScriptsWhileDisabled(int32_t enabled) {
    BUTTON_CHECK();
    this->m_motionScriptsWhileDisabled = enabled;
}

// VERIFY: 3131b961 auto
// OFFSET: 0x00962170
int32_t CSimpleButton::GetMotionScriptsWhileDisabled() {
    BUTTON_CHECK();
    return this->m_motionScriptsWhileDisabled;
}

// VERIFY: a0ee9c2b auto
// OFFSET: 0x0096E9C0
uint64_t MouseButtonStringToBitmask(const char* string) {
    if (!string || !*string) {
        return CLICK_ON_NONE;
    }

    if (!SStrCmpI(string, "LeftButtonDown", STORM_MAX_STR)) {
        return CLICK_ON_LEFTDOWN;
    }

    if (!SStrCmpI(string, "LeftButtonUp", STORM_MAX_STR)) {
        return CLICK_ON_LEFTUP;
    }

    if (!SStrCmpI(string, "MiddleButtonDown", STORM_MAX_STR)) {
        return CLICK_ON_MIDDLEDOWN;
    }

    if (!SStrCmpI(string, "MiddleButtonUp", STORM_MAX_STR)) {
        return CLICK_ON_MIDDLEUP;
    }

    if (!SStrCmpI(string, "RightButtonDown", STORM_MAX_STR)) {
        return CLICK_ON_RIGHTDOWN;
    }

    if (!SStrCmpI(string, "RightButtonUp", STORM_MAX_STR)) {
        return CLICK_ON_RIGHTUP;
    }

    if (!SStrCmpI(string, "Button4Down", STORM_MAX_STR)) {
        return CLICK_ON_X1DOWN;
    }

    if (!SStrCmpI(string, "Button4Up", STORM_MAX_STR)) {
        return CLICK_ON_X1UP;
    }

    if (!SStrCmpI(string, "Button5Down", STORM_MAX_STR)) {
        return CLICK_ON_X2DOWN;
    }

    if (!SStrCmpI(string, "Button5Up", STORM_MAX_STR)) {
        return CLICK_ON_X2UP;
    }

    if (!SStrCmpI(string, "Button6Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON6DOWN;
    }

    if (!SStrCmpI(string, "Button6Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON6UP;
    }

    if (!SStrCmpI(string, "Button7Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON7DOWN;
    }

    if (!SStrCmpI(string, "Button7Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON7UP;
    }

    if (!SStrCmpI(string, "Button8Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON8DOWN;
    }

    if (!SStrCmpI(string, "Button8Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON8UP;
    }

    if (!SStrCmpI(string, "Button9Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON9DOWN;
    }

    if (!SStrCmpI(string, "Button9Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON9UP;
    }

    if (!SStrCmpI(string, "Button10Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON10DOWN;
    }

    if (!SStrCmpI(string, "Button10Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON10UP;
    }

    if (!SStrCmpI(string, "Button11Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON11DOWN;
    }

    if (!SStrCmpI(string, "Button11Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON11UP;
    }

    if (!SStrCmpI(string, "Button12Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON12UP;
    }

    if (!SStrCmpI(string, "Button12Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON12DOWN;
    }

    if (!SStrCmpI(string, "Button13Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON13UP;
    }

    if (!SStrCmpI(string, "Button13Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON13DOWN;
    }

    if (!SStrCmpI(string, "Button14Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON14UP;
    }

    if (!SStrCmpI(string, "Button14Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON14DOWN;
    }

    if (!SStrCmpI(string, "Button15Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON15UP;
    }

    if (!SStrCmpI(string, "Button15Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON15DOWN;
    }

    if (!SStrCmpI(string, "Button16Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON16UP;
    }

    if (!SStrCmpI(string, "Button16Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON16DOWN;
    }

    if (!SStrCmpI(string, "Button17Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON17UP;
    }

    if (!SStrCmpI(string, "Button17Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON17DOWN;
    }

    if (!SStrCmpI(string, "Button18Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON18UP;
    }

    if (!SStrCmpI(string, "Button18Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON18DOWN;
    }

    if (!SStrCmpI(string, "Button19Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON19UP;
    }

    if (!SStrCmpI(string, "Button19Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON19DOWN;
    }

    if (!SStrCmpI(string, "Button20Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON20DOWN;
    }

    if (!SStrCmpI(string, "Button20Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON20UP;
    }

    if (!SStrCmpI(string, "Button21Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON21DOWN;
    }

    if (!SStrCmpI(string, "Button21Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON21UP;
    }

    if (!SStrCmpI(string, "Button22Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON22UP;
    }

    if (!SStrCmpI(string, "Button22Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON22DOWN;
    }

    if (!SStrCmpI(string, "Button23Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON23UP;
    }

    if (!SStrCmpI(string, "Button23Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON23DOWN;
    }

    if (!SStrCmpI(string, "Button24Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON24UP;
    }

    if (!SStrCmpI(string, "Button24Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON24DOWN;
    }

    if (!SStrCmpI(string, "Button25Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON25UP;
    }

    if (!SStrCmpI(string, "Button25Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON25DOWN;
    }

    if (!SStrCmpI(string, "Button26Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON26UP;
    }

    if (!SStrCmpI(string, "Button26Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON26DOWN;
    }

    if (!SStrCmpI(string, "Button27Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON27UP;
    }

    if (!SStrCmpI(string, "Button27Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON27DOWN;
    }

    if (!SStrCmpI(string, "Button28Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON28UP;
    }

    if (!SStrCmpI(string, "Button28Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON28DOWN;
    }

    if (!SStrCmpI(string, "Button29Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON29UP;
    }

    if (!SStrCmpI(string, "Button29Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON29DOWN;
    }

    if (!SStrCmpI(string, "Button30Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON30DOWN;
    }

    if (!SStrCmpI(string, "Button30Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON30UP;
    }

    if (!SStrCmpI(string, "Button31Down", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON31DOWN;
    }

    if (!SStrCmpI(string, "Button31Up", STORM_MAX_STR)) {
        return CLICK_ON_BUTTON31UP;
    }

    if (!SStrCmpI(string, "AnyDown", STORM_MAX_STR)) {
        return CLICK_ON_ANYDOWN;
    }

    if (!SStrCmpI(string, "AnyUp", STORM_MAX_STR)) {
        return CLICK_ON_ANYUP;
    }

    return CLICK_ON_NONE;
}

// VERIFY: 7ac18ca5 auto
// OFFSET: 0x0096F090
void CSimpleButton::RunOnClickScript(const char* btn, int32_t down) {
    BUTTON_CHECK();
    if (this->m_onPreClick.m_luaRef) {
        lua_State* L = FrameScript_GetContext();
        lua_pushstring(L, btn);
        lua_pushboolean(L, down);
        this->RunScript(this->m_onPreClick, 2, nullptr);
    }

    if (this->m_onClick.m_luaRef) {
        lua_State* L = FrameScript_GetContext();
        lua_pushstring(L, btn);
        lua_pushboolean(L, down);
        this->RunScript(this->m_onClick, 2, nullptr);
    }

    if (this->m_onPostClick.m_luaRef) {
        lua_State* L = FrameScript_GetContext();
        lua_pushstring(L, btn);
        lua_pushboolean(L, down);
        this->RunScript(this->m_onPostClick, 2, nullptr);
    }
}

// VERIFY: c1dafa11 auto
// OFFSET: 0x0096F150
void CSimpleButton::RunOnDoubleClickScript(const char* btn) {
    BUTTON_CHECK();
    if (this->m_onDoubleClick.m_luaRef) {
        lua_State* L = FrameScript_GetContext();
        lua_pushstring(L, btn);
        this->RunScript(this->m_onDoubleClick, 1, nullptr);
    }
}

// VERIFY: 1a2379ee auto
// OFFSET: [0x0096F190, 0x0096F520, 0x0096F740]
CSimpleButton::~CSimpleButton() {
}

// VERIFY: 6244a24f auto
// OFFSET: 0x0096F1E0
const char* CSimpleButton::GetObjectTypeName() {
    BUTTON_CHECK();
    return CSimpleButton::s_objectTypeName;
}

// VERIFY: ea565c62 auto
// OFFSET: 0x0096F1F0
int32_t CSimpleButton::GetScriptMetaTable() {
    BUTTON_CHECK();
    return CSimpleButton::s_metatable;
}

// VERIFY: d100e369 auto
// OFFSET: 0x0096F200
FrameScript_Object::ScriptIx* CSimpleButton::GetScriptByName(const char* name, FrameScript_Object::ScriptData& data) {
    BUTTON_CHECK();
    FrameScript_Object::ScriptIx* script = CSimpleFrame::GetScriptByName(name, data);

    if (script) {
        return script;
    }

    if (!SStrCmpI(name, "PreClick", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,button,down) %s end";
        return &this->m_onPreClick;
    }

    if (!SStrCmpI(name, "OnClick", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,button,down) %s end";
        return &this->m_onClick;
    }

    if (!SStrCmpI(name, "PostClick", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,button,down) %s end";
        return &this->m_onPostClick;
    }

    if (!SStrCmpI(name, "OnDoubleClick", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,button) %s end";
        return &this->m_onDoubleClick;
    }

    return nullptr;
}

// VERIFY: 781d19ca auto
// OFFSET: 0x0096F2D0
void CSimpleButton::GetScriptTime(lua_State* L, double* time, uint32_t* count, int32_t recurse) {
    BUTTON_CHECK();
    FrameScript_Object::GetScriptTime(L, this->m_onPreClick, time, count);
    FrameScript_Object::GetScriptTime(L, this->m_onClick, time, count);
    FrameScript_Object::GetScriptTime(L, this->m_onPostClick, time, count);
    FrameScript_Object::GetScriptTime(L, this->m_onDoubleClick, time, count);
    CSimpleFrame::GetScriptTime(L, time, count, recurse);
}

// VERIFY: d62e2602 auto
// OFFSET: 0x0096F350
void CSimpleButton::UnregisterRegion(CSimpleRegion* region) {
    BUTTON_CHECK();
    if (region == this->m_fontString) {
        this->m_fontString = nullptr;
    }

    if (region == this->m_stateTextures[BUTTONSTATE_DISABLED]) {
        this->m_stateTextures[BUTTONSTATE_DISABLED] = nullptr;
    }

    if (region == this->m_stateTextures[BUTTONSTATE_NORMAL]) {
        this->m_stateTextures[BUTTONSTATE_NORMAL] = nullptr;
    }

    if (region == this->m_stateTextures[BUTTONSTATE_PUSHED]) {
        this->m_stateTextures[BUTTONSTATE_PUSHED] = nullptr;
    }

    if (region == this->m_activeTexture) {
        this->m_activeTexture = nullptr;
    }

    if (region == this->m_highlightTexture) {
        this->m_highlightTexture = nullptr;
    }

    CSimpleFrame::UnregisterRegion(region);
}

// VERIFY: 3ee789b5 auto
// OFFSET: 0x0096F3C0
void CSimpleButton::SetHighlight(CSimpleTexture* texture, EGxBlend blend) {
    BUTTON_CHECK();
    if (texture == this->m_highlightTexture) {
        return;
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CheckTexture(this, texture);
#endif
    if (this->m_highlightTexture) {
        delete this->m_highlightTexture;
    }

    if (texture) {
        texture->SetFrame(this, 4, 1);
        texture->SetBlendMode(blend);
    }

    this->m_highlightTexture = texture;
}

// VERIFY: 8c6874f5 auto
// OFFSET: 0x0096F410
void CSimpleButton::OnLayerHide() {
    BUTTON_CHECK();
    if (this->m_state != BUTTONSTATE_DISABLED && !this->m_stateLocked) {
        this->SetButtonState(BUTTONSTATE_NORMAL, 0);
    }

    CSimpleFrame::OnLayerHide();
}

// VERIFY: 7324b618 auto
// OFFSET: 0x0096F440
void CSimpleButton::OnDragStart(const CMouseEvent& evt) {
    BUTTON_CHECK();
    if (this->m_state != BUTTONSTATE_DISABLED && !this->m_stateLocked) {
        this->SetButtonState(BUTTONSTATE_NORMAL, 0);
    }

    if (this->m_state != BUTTONSTATE_DISABLED) {
        CSimpleFrame::OnDragStart(evt);
    }
}

// VERIFY: 78f1af9e auto
// OFFSET: 0x0096F480
void CSimpleButton::SetClickAction(uint64_t action) {
    BUTTON_CHECK();
    this->m_clickAction = action;
}

// VERIFY: e5ddb2a8 auto
// OFFSET: 0x0096F4A0
void CSimpleButton::UpdateTextState(CSimpleButtonState state) {
    BUTTON_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CheckState(state); Frame::Native::RequireScriptTextMutation();
#endif
    if (!this->m_fontString) {
        return;
    }

    if (state == BUTTONSTATE_PUSHED) {
        this->m_fontString->SetJustificationOffset(this->m_pushedTextOffset.x, this->m_pushedTextOffset.y);
    } else {
        this->m_fontString->SetJustificationOffset(0.0f, 0.0f);

        if (state == BUTTONSTATE_DISABLED) {
            this->m_fontString->SetFontObject(this->m_disabledFont);
            return;
        }
    }

    if (this->m_drawLayerEnabled[DRAWLAYER_HIGHLIGHT]) {
        this->m_fontString->SetFontObject(this->m_highlightFont);
    } else {
        this->m_fontString->SetFontObject(this->m_normalFont);
    }
}

// VERIFY: 791e4917 auto
// OFFSET: 0x0096F7B0
void CSimpleButton::SetFontString(CSimpleFontString* fontString) {
    BUTTON_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    if (fontString) Frame::Native::RequireScript(fontString);
#endif
    if (fontString == this->m_fontString) {
        return;
    }

    if (this->m_fontString) {
        delete this->m_fontString;
    }

    if (fontString) {
        fontString->SetFrame(this, 2, 1);

        if (!fontString->HasAnyFramepoints()) {
            uint32_t styleFlags = this->m_normalFont ? this->m_normalFont->m_attributes.m_styleFlags : 0;

            if (styleFlags & 0x1) {
                fontString->SetPoint(FRAMEPOINT_LEFT, this, FRAMEPOINT_LEFT, 0.0f, 0.0f, 1);
            } else if (styleFlags & 0x4) {
                fontString->SetPoint(FRAMEPOINT_RIGHT, this, FRAMEPOINT_RIGHT, 0.0f, 0.0f, 1);
            } else {
                fontString->SetPoint(FRAMEPOINT_CENTER, this, FRAMEPOINT_CENTER, 0.0f, 0.0f, 1);
            }
        }
    }

    this->m_fontString = fontString;
    this->UpdateTextState(this->m_state);
}

// VERIFY: 9b5e101b manual
// OFFSET: 0x0096F860
void CSimpleButton::SetText(const char* text) {
    BUTTON_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireScriptTextMutation();
    // Existing text may change in callbacks; allocation changes the region tree.
    if (!this->m_fontString && text && *text) Frame::Native::RequireTreeMutation();
#endif
    if ((!text || !*text) && !this->m_fontString) {
        return;
    }

    if (!this->m_fontString) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
        this->SetFontString(Frame::Native::CreateFontString(this, 2, true));
#else
        void* data = CSimpleFontString::s_allocator.GetData(0, typeid(CSimpleFontString).raw_name(), SLOG_OBJECT);
        this->SetFontString(data ? new (data) CSimpleFontString(this, 2, true) : nullptr);
#endif
    }

    this->m_fontString->SetText(text, 1);
}

// VERIFY: 0a129d6e manual
// OFFSET: 0x0096F8D0
void CSimpleButton::SetNormalFontColor(const CImVector& color) {
    BUTTON_CHECK();
    CSimpleFont* font = this->m_normalFont;

    if (!font) {
        return;
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (std::memcmp(&color, &font->m_attributes.m_color, sizeof(color)) || !(font->m_attributes.m_flags & 0x400)) {
#else
    if (*reinterpret_cast<const uint32_t*>(&color) != *reinterpret_cast<uint32_t*>(&font->m_attributes.m_color) || !(font->m_attributes.m_flags & 0x400)) {
#endif
        font->m_attributes.m_color = color;
        font->m_attributes.m_flags |= 0x404;
    }

    this->m_normalFont->m_fontableFlags &= ~0x4u;
    this->m_normalFont->UpdateObjects();
}

// VERIFY: 5e94ba11 auto
// OFFSET: 0x0096F920
void CSimpleButton::SetStateTexture(CSimpleButtonState state, CSimpleTexture* texture) {
    BUTTON_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CheckState(state);
    Frame::Native::RequireTreeMutation();
#endif
    if (texture == this->m_stateTextures[state]) {
        return;
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CheckTexture(this, texture);
#endif
    if (this->m_activeTexture == this->m_stateTextures[state]) {
        this->m_activeTexture = nullptr;
    }

    if (this->m_stateTextures[state]) {
        delete this->m_stateTextures[state];
    }

    if (texture) {
        texture->SetFrame(this, 2, 0);
    }

    this->m_stateTextures[state] = texture;

    if (texture && state == this->m_state) {
        this->m_activeTexture = texture;
        texture->m_shown = 1;
        texture->ShowThis();
    }
}

// VERIFY: 8dd71c35 manual
// OFFSET: 0x0096F9B0
int32_t CSimpleButton::SetHighlight(const char* fileName, EGxBlend blend) {
    BUTTON_CHECK();
    CSimpleTexture* texture = this->m_highlightTexture;

    if (texture) {
        texture->SetTexture(fileName, 0, 0, CSimpleTexture::s_textureFilterMode, ImageMode_Normal);
        texture->SetBlendMode(blend);
        return 1;
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    std::unique_ptr<CSimpleTexture> owner(Frame::Native::CreateTexture(this, 4, true));
    texture = owner.get();
#else
    void* data = CSimpleTexture::s_allocator.GetData(0, typeid(CSimpleTexture).raw_name(), SLOG_OBJECT);
    texture = data ? new (data) CSimpleTexture(this, 4, 1) : nullptr;
#endif

    if (!texture->SetTexture(fileName, 0, 0, CSimpleTexture::s_textureFilterMode, ImageMode_Normal)) {
#ifndef WOWDECOMP_PORTABLE_RUNTIME
        if (texture) {
            delete texture;
        }
#endif

        return 0;
    }

    texture->SetAllPoints(this, 1);
    texture->SetBlendMode(blend);
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    owner.release();
#endif
    this->m_highlightTexture = texture;
    return 1;
}

// VERIFY: 093d46bd auto
// OFFSET: 0x0096FA80
int32_t CSimpleButton::OnLayerMouseDown(const CMouseEvent& evt, const char* btn) {
    BUTTON_CHECK();
    int32_t result = CSimpleFrame::OnLayerMouseDown(evt, btn);

    if (result) {
        return result;
    }

    if (!btn) {
        C2Vector pt;
        pt.x = evt.m_data.m_mousePosition.x;
        pt.y = evt.m_data.m_mousePosition.y;

        if (!this->TestHitRect(pt)) {
            return 0;
        }
    }

    if (this->m_state == BUTTONSTATE_DISABLED) {
        return 1;
    }

    uint32_t button = btn ? static_cast<uint32_t>(CLICK_ON_ANYDOWN) : evt.m_data.m_button;

    if (this->m_clickAction & button) {
        if (!btn) {
            btn = GetMouseButtonName(evt.m_data.m_button);
        }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
        ClickFlag dispatchFlag(this->m_simpleTop->unused_1250);
#endif
        this->m_simpleTop->unused_1250 = 1;
        this->OnClick(btn, 1);
        this->m_simpleTop->unused_1250 = 0;
    }

    if (!this->m_stateLocked && this->m_state != BUTTONSTATE_DISABLED) {
        this->SetButtonState(BUTTONSTATE_PUSHED, 0);
    }

    return 1;
}

// VERIFY: 3e18137e auto
// OFFSET: 0x0096FB60
int32_t CSimpleButton::OnLayerMouseUp(const CMouseEvent& evt, const char* btn) {
    BUTTON_CHECK();
    int32_t result = CSimpleFrame::OnLayerMouseUp(evt, btn);

    if (result) {
        return result;
    }

    if (this->m_state != BUTTONSTATE_PUSHED) {
        return 1;
    }

    uint64_t button = 0;

    if (btn) {
        button = CLICK_ON_ANYUP;
    } else {
        C2Vector pt;
        pt.x = evt.m_data.m_mousePosition.x;
        pt.y = evt.m_data.m_mousePosition.y;

        if (this->TestHitRect(pt)) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
            button = uint64_t(uint32_t(evt.m_data.m_button)) << 31;
#else
            button = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(evt.m_data.m_button)) << 31);
#endif
        }
    }

    if (this->m_clickAction & button) {
        if (!btn) {
            btn = GetMouseButtonName(evt.m_data.m_button);
        }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
        ClickFlag dispatchFlag(this->m_simpleTop->unused_1250);
#endif
        this->m_simpleTop->unused_1250 = 1;
        uint32_t time = OsGetAsyncTimeMs();

        if (this->m_onDoubleClick.m_luaRef && this->m_lastClickTime && time - this->m_lastClickTime <= 300) {
            this->OnDoubleClick(btn);
            this->m_lastClickTime = 0;
        } else {
            this->OnClick(btn, 0);
            this->m_lastClickTime = time;
        }

        this->m_simpleTop->unused_1250 = 0;
    }

    if (!this->m_stateLocked && this->m_state != BUTTONSTATE_DISABLED) {
        this->SetButtonState(BUTTONSTATE_NORMAL, 0);
    }

    return 1;
}

// VERIFY: a6303e9e auto
// OFFSET: 0x0096FCA0
void CSimpleButton::OnLayerCursorEnter(int32_t motion) {
    BUTTON_CHECK();
    if (this->m_state != BUTTONSTATE_DISABLED) {
        CSimpleFrame::OnLayerCursorEnter(motion);
        this->UpdateTextState(this->m_state);
        return;
    }

    if (this->GetMotionScriptsWhileDisabled()) {
        if (this->m_hoverFrame) {
            this->m_hoverFrame->Show();
        }

        this->RunOnEnterScript(motion);
    }
}

// VERIFY: f683541a auto
// OFFSET: 0x0096FD00
void CSimpleButton::OnLayerCursorExit(int32_t motion, int32_t a3) {
    BUTTON_CHECK();
    if (this->m_state != BUTTONSTATE_DISABLED) {
        CSimpleFrame::OnLayerCursorExit(motion, a3);
        this->UpdateTextState(this->m_state);
        return;
    }

    if (this->GetMotionScriptsWhileDisabled()) {
        if (this->m_hoverFrame) {
            this->m_hoverFrame->Hide();
        }

        this->RunOnLeaveScript(motion);
    }
}

// VERIFY: cddc7e35 auto
// OFFSET: 0x0096FD70
void CSimpleButton::OnClick(const char* btn, int32_t down) {
    BUTTON_CHECK();
    if (this->m_inClick || this->m_state == BUTTONSTATE_DISABLED) {
        return;
    }

    const char* savedButtonName = this->m_simpleTop->m_mouseButtonName;
    this->m_simpleTop->m_mouseButtonName = btn;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    struct Restore {
        CSimpleButton* button;
        const char* name;
        ~Restore() { button->m_inClick = 0; button->m_simpleTop->m_mouseButtonName = name; }
    } restore{this, savedButtonName};
    Frame::Native::ResizeScope callbackGuard;
#endif
    this->m_inClick = 1;
    this->RunOnClickScript(btn, down);
    this->m_inClick = 0;
    this->m_simpleTop->m_mouseButtonName = savedButtonName;
}

// VERIFY: 7a969f93 auto
// OFFSET: 0x0096FDD0
void CSimpleButton::OnDoubleClick(const char* btn) {
    BUTTON_CHECK();
    if (this->m_inClick || this->m_state == BUTTONSTATE_DISABLED) {
        return;
    }

    const char* savedButtonName = this->m_simpleTop->m_mouseButtonName;
    this->m_simpleTop->m_mouseButtonName = btn;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    struct Restore {
        CSimpleButton* button;
        const char* name;
        ~Restore() { button->m_inClick = 0; button->m_simpleTop->m_mouseButtonName = name; }
    } restore{this, savedButtonName};
    Frame::Native::ResizeScope callbackGuard;
#endif
    this->m_inClick = 1;
    this->RunOnDoubleClickScript(btn);
    this->m_inClick = 0;
    this->m_simpleTop->m_mouseButtonName = savedButtonName;
}

// VERIFY: d16dc184 manual
// OFFSET: 0x0096FE30
void CSimpleButton::SetButtonState(CSimpleButtonState state, int32_t stateLocked) {
    BUTTON_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CheckState(state); Frame::Native::RequireScriptTextMutation();
#endif
    this->m_stateLocked = stateLocked;

    if (state == this->m_state) {
        return;
    }

    if (this->m_activeTexture && (this->m_stateTextures[state] || state == BUTTONSTATE_NORMAL)) {
        this->m_activeTexture->m_shown = 0;
        this->m_activeTexture->HideThis();
        this->m_activeTexture = nullptr;
    }

    if (this->m_stateTextures[state]) {
        this->m_activeTexture = this->m_stateTextures[state];
        this->m_activeTexture->m_shown = 1;
        this->m_activeTexture->ShowThis();
    }

    this->UpdateTextState(state);
    this->m_state = state;
}

// VERIFY: 45a8641e auto
// OFFSET: 0x0096FED0
void CSimpleButton::LoadXML(XMLNode* node, CStatus* status) {
    BUTTON_CHECK();
    CSimpleFrame::LoadXML(node, status);

    CSimpleFontString* fontString = nullptr;

    for (XMLNode* child = node->m_child; child; child = child->m_next) {
        if (!SStrCmpI(child->m_name.m_str, "NormalTexture", STORM_MAX_STR)) {
            this->SetStateTexture(BUTTONSTATE_NORMAL, LoadXML_Texture(child, this, status));
        } else if (!SStrCmpI(child->m_name.m_str, "PushedTexture", STORM_MAX_STR)) {
            this->SetStateTexture(BUTTONSTATE_PUSHED, LoadXML_Texture(child, this, status));
        } else if (!SStrCmpI(child->m_name.m_str, "DisabledTexture", STORM_MAX_STR)) {
            this->SetStateTexture(BUTTONSTATE_DISABLED, LoadXML_Texture(child, this, status));
        } else if (!SStrCmpI(child->m_name.m_str, "HighlightTexture", STORM_MAX_STR)) {
            CSimpleTexture* texture = LoadXML_Texture(child, this, status);
            this->SetHighlight(texture, texture->m_alphaMode);
        } else if (!SStrCmpI(child->m_name.m_str, "ButtonText", STORM_MAX_STR)) {
            fontString = LoadXML_String(child, this, status);
        } else if (!SStrCmpI(child->m_name.m_str, "NormalFont", STORM_MAX_STR)) {
            const char* style = child->GetAttributeByName("style");

            if (style && *style) {
                this->m_normalFont = CSimpleFont::GetFont(style, 0);

                if (!this->m_normalFont) {
                    status->Add(STATUS_WARNING, "Couldn't find NORMAL font: %s", style);
                }
            }
        } else if (!SStrCmpI(child->m_name.m_str, "DisabledFont", STORM_MAX_STR)) {
            const char* style = child->GetAttributeByName("style");

            if (style && *style) {
                this->m_disabledFont = CSimpleFont::GetFont(style, 0);

                if (!this->m_disabledFont) {
                    status->Add(STATUS_WARNING, "Couldn't find DISABLED font: %s", style);
                }
            }
        } else if (!SStrCmpI(child->m_name.m_str, "HighlightFont", STORM_MAX_STR)) {
            const char* style = child->GetAttributeByName("style");

            if (style && *style) {
                this->m_highlightFont = CSimpleFont::GetFont(style, 0);

                if (!this->m_highlightFont) {
                    status->Add(STATUS_WARNING, "Couldn't find HIGHLIGHT font: %s", style);
                }
            }
        } else if (!SStrCmpI(child->m_name.m_str, "NormalColor", STORM_MAX_STR)) {
            CImVector color;
            LoadXML_Color(child, color, status);
            this->SetNormalFontColor(color);
        } else if (!SStrCmpI(child->m_name.m_str, "DisabledColor", STORM_MAX_STR)) {
            CImVector color;
            LoadXML_Color(child, color, status);
            this->SetNormalFontColor(color);
        } else if (!SStrCmpI(child->m_name.m_str, "HighlightColor", STORM_MAX_STR)) {
            CImVector color;
            LoadXML_Color(child, color, status);
            this->SetNormalFontColor(color);
        } else if (!SStrCmpI(child->m_name.m_str, "PushedTextOffset", STORM_MAX_STR)) {
            float x;
            float y;

            if (LoadXML_Dimensions(child, x, y, status)) {
                this->m_pushedTextOffset.x = x;
                this->m_pushedTextOffset.y = y;
            }
        }
    }

    if (fontString) {
        this->SetFontString(fontString);
    } else {
        this->UpdateTextState(this->m_state);
    }

    const char* text = node->GetAttributeByName("text");

    if (text && *text) {
        const char* localized = FrameScript_GetText(text, -1, GENDER_NOT_APPLICABLE);

        if (!localized || !*localized) {
            localized = text;
        }

        this->SetText(localized);
    }

    const char* registerForClicks = node->GetAttributeByName("registerForClicks");

    if (registerForClicks && *registerForClicks) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
        uint64_t clickAction = 0;
#else
        uint32_t clickAction = 0;
#endif
        char token[1024];

        SStrTokenize(&registerForClicks, token, sizeof(token), " ,", nullptr);

        while (token[0]) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
            clickAction |= MouseButtonStringToBitmask(token);
#else
            clickAction |= static_cast<uint32_t>(MouseButtonStringToBitmask(token));
#endif
            SStrTokenize(&registerForClicks, token, sizeof(token), " ,", nullptr);
        }

        this->m_clickAction = clickAction;
    }

    const char* motionScriptsWhileDisabled = node->GetAttributeByName("motionScriptsWhileDisabled");

    if (motionScriptsWhileDisabled) {
        this->SetMotionScriptsWhileDisabled(StringToBOOL(motionScriptsWhileDisabled));
    }
}

// VERIFY: b8ad7d54 manual
// OFFSET: 0x00970370
int32_t CSimpleButton::SetStateTexture(CSimpleButtonState state, const char* fileName) {
    BUTTON_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CheckState(state); Frame::Native::RequireTreeMutation();
#endif
    if (this->m_stateTextures[state]) {
        this->m_stateTextures[state]->SetTexture(fileName, 0, 0, CSimpleTexture::s_textureFilterMode, ImageMode_Normal);
        return 1;
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    std::unique_ptr<CSimpleTexture> owner(Frame::Native::CreateTexture(nullptr, 2, true));
    CSimpleTexture* texture = owner.get();
#else
    void* data = CSimpleTexture::s_allocator.GetData(0, typeid(CSimpleTexture).raw_name(), SLOG_OBJECT);
    CSimpleTexture* texture = data ? new (data) CSimpleTexture(nullptr, 2, 1) : nullptr;
#endif

    if (!texture->SetTexture(fileName, 0, 0, CSimpleTexture::s_textureFilterMode, ImageMode_Normal)) {
#ifndef WOWDECOMP_PORTABLE_RUNTIME
        if (texture) {
            delete texture;
        }
#endif

        return 0;
    }

    texture->SetAllPoints(this, 1);
    this->SetStateTexture(state, texture);
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    owner.release();
#endif
    return 1;
}

// VERIFY: 3f2ae434 manual
// OFFSET: 0x00970440
void CSimpleButton::Enable(int32_t enabled) {
    BUTTON_CHECK();
    if (enabled) {
        if (this->m_state != BUTTONSTATE_DISABLED) {
            return;
        }

        this->SetButtonState(BUTTONSTATE_NORMAL, 0);

        if (this == this->m_simpleTop->m_mouseFocus) {
            this->OnLayerCursorEnter(0);
        }

        if (this->m_highlightLocked) {
            this->EnableDrawLayer(DRAWLAYER_HIGHLIGHT);
        }
    } else {
        if (this->m_state == BUTTONSTATE_DISABLED) {
            return;
        }

        if (this == this->m_simpleTop->m_mouseFocus) {
            this->OnLayerCursorExit(0, 1);
        }

        this->DisableDrawLayer(DRAWLAYER_HIGHLIGHT);
        this->SetButtonState(BUTTONSTATE_DISABLED, 0);
    }

    int32_t disabled = enabled == 0;
    this->SetFrameFlag(0x400, disabled);

    FrameScript_Object::ScriptIx& script = disabled ? this->m_onDisable : this->m_onEnable;

    if (script.m_luaRef && !this->m_isLoading) {
        this->RunScript(script, 0, nullptr);
    }
}

// VERIFY: 7a7f3ab5 auto
// OFFSET: 0x00970520
CSimpleButton::CSimpleButton(CSimpleFrame* parent) : CSimpleFrame(parent) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CSimpleButton::GetObjectType();
#endif
    this->m_state = BUTTONSTATE_DISABLED;
    this->m_inClick = 0;
    this->m_stateLocked = 0;
    this->m_motionScriptsWhileDisabled = 0;
    this->m_lastClickTime = 0;
    this->m_clickAction = CLICK_ON_LEFTUP;
    this->m_fontString = nullptr;
    this->m_normalFont = nullptr;
    this->m_highlightFont = nullptr;
    this->m_disabledFont = nullptr;
    this->m_activeTexture = nullptr;
    this->m_highlightTexture = nullptr;
    this->m_pushedTextOffset.x = 0.001f;
    this->m_pushedTextOffset.y = -0.001f;

    for (uint32_t i = 0; i < NUM_BUTTONSTATES; i++) {
        this->m_stateTextures[i] = nullptr;
    }

    this->Enable(1);
    this->EnableEvent(SIMPLE_EVENT_MOUSE, -1);
    this->SetFrameFlag(0x10000, 1);
}
