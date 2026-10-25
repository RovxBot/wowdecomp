// Bounded selection of original CharacterSelection.cpp state and
// CharacterSelection_Scripts.cpp::Script_GetNumCharacters (0x004E3140).
// The unchanged AddonList OnLoad queries this real pre-authentication list.
// Transport, character displays and model/component producers are not selected.
#include "WoW/Source/Glue/CharacterSelection.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include <exception>
static_assert(sizeof(CHARACTER_INFO) == 0x188, "Original fixed-width character record changed");

TSGrowableArray<CharacterSelectionDisplay> CCharacterSelection::s_characterList;

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
int32_t GetNumCharacters(lua_State* L) {
    FrameScript::Native::RequireOwner();
    lua_pushnumber(L, CCharacterSelection::s_characterList.Count());
    return 1;
}
}
void CharSelectRegisterScriptFunctions() {
    FrameScript_RegisterFunction("GetNumCharacters", GetNumCharacters);
}
void CharSelectUnregisterScriptFunctions() {
    FrameScript_UnregisterFunction("GetNumCharacters");
}
