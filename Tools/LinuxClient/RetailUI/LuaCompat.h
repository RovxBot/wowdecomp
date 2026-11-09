#pragma once

struct lua_State;

namespace RovRetailUI {
inline constexpr const char* Version = "forever-1.60.1-70862-v1";
bool Enabled();
void RegisterLuaFunctions(lua_State* L);
bool IsHookRestrictedFunction(lua_State* L, int index);
}
