// Bounded selection of original CharacterSelection.cpp state and
// CharacterSelection_Scripts.cpp::Script_GetNumCharacters (0x004E3140).
// The unchanged AddonList OnLoad queries this real pre-authentication list.
// Transport, character displays and model/component producers are not selected.
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/Glue/CharacterCreation.h"
#include "WoW/Source/Glue/CSimpleModelFFX.h"
#include "Engine/Source/Frame/Portable/Scripts.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include <exception>
static_assert(sizeof(CHARACTER_INFO) == 0x188, "Original fixed-width character record changed");

TSGrowableArray<CharacterSelectionDisplay> CCharacterSelection::s_characterList;
CSimpleModelFFX* CCharacterSelection::m_modelFrame;
CSimpleModelFFX* CCharacterCreation::m_charCustomizeFrame;

CharacterSelectionDisplay::CharacterSelectionDisplay() {
    m_component = nullptr;
    m_petAlpha = 1.0f;
    m_petModel = nullptr;
}
CharacterSelectionDisplay::~CharacterSelectionDisplay() {
    // Do not silently leak or impersonate unavailable component/model release.
    // Native producers must connect their real owners before populating these.
    if (m_petModel || m_component) std::terminate();
}
namespace {
bool selectionRegistered = false, creationRegistered = false;
// Original CharacterSelection_Scripts.cpp 0x004E2F60 and
// CharacterCreation_Scripts.cpp 0x004E05B0. Use the canonical live-object lookup;
// native selection/customization requires the actual ModelFFX derived object.
int32_t BindModelFrame(lua_State* L, CSimpleModelFFX*& destination, const char* method) {
    if (!lua_isstring(L, 1)) return luaL_error(L, "Usage: %s(\"frameName\")", method);
    auto* object = CScriptObject::GetScriptObjectByName(lua_tostring(L, 1), CSimpleModel::GetObjectType());
    auto* frame = dynamic_cast<CSimpleModelFFX*>(object);
    if (object && !frame) return luaL_error(L, "%s requires an original ModelFFX frame", method);
    if (frame) destination = frame;
    return 0;
}
int32_t SetCharSelectModelFrame(lua_State* L) {
    if (!selectionRegistered) return luaL_error(L, "Native character selection scripts are closed");
    return BindModelFrame(L, CCharacterSelection::m_modelFrame, "SetCharSelectModelFrame");
}
int32_t SetCharCustomizeFrame(lua_State* L) {
    if (!creationRegistered) return luaL_error(L, "Native character creation scripts are closed");
    return BindModelFrame(L, CCharacterCreation::m_charCustomizeFrame, "SetCharCustomizeFrame");
}
int32_t GetNumCharacters(lua_State* L) {
    FrameScript::Native::RequireOwner();
    lua_pushnumber(L, CCharacterSelection::s_characterList.Count());
    return 1;
}
}
void CharSelectRegisterScriptFunctions() {
    selectionRegistered = true;
    FrameScript_RegisterFunction("GetNumCharacters", GetNumCharacters);
    FrameScript_RegisterFunction("SetCharSelectModelFrame", Frame::Native::ScriptMethod<SetCharSelectModelFrame, true, false, true>);
}
void CharSelectUnregisterScriptFunctions() {
    FrameScript_UnregisterFunction("GetNumCharacters");
    FrameScript_UnregisterFunction("SetCharSelectModelFrame");
    selectionRegistered = false;
    CCharacterSelection::m_modelFrame = nullptr;
}
void CharacterCreateRegisterScriptFunctions() {
    creationRegistered = true;
    FrameScript_RegisterFunction("SetCharCustomizeFrame", Frame::Native::ScriptMethod<SetCharCustomizeFrame, true, false, true>);
}
void CharacterCreateUnregisterScriptFunctions() {
    FrameScript_UnregisterFunction("SetCharCustomizeFrame");
    creationRegistered = false;
    CCharacterCreation::m_charCustomizeFrame = nullptr;
}
