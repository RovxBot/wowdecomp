#pragma once

#include "Engine/Source/FrameScript/FrameScript_Object.h"

class CStatus;
class XMLNode;
struct lua_State;

// Product-authored compatibility prerequisite, not imported Blizzard engine code.
// Only the full Linux client selects this module. No new protected-action grant.
namespace RovRetailUI {
inline constexpr const char* Version = "forever-1.60.1-70862-v1";
bool Enabled();
bool ApplyMixins(lua_State* L, int objectRef, const char* names, CStatus* status);
bool HasScriptAttributes(XMLNode* node);
bool LoadScript(lua_State* L, int objectRef, XMLNode* node,
                FrameScript_Object::ScriptIx& script,
                const FrameScript_Object::ScriptData& data, CStatus* status);
}
