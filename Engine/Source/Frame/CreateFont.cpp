// Original 12340 global font factory, separated from region/model factories.
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/Frame/SimpleFrameScript.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include <exception>

// VERIFY: 5d8fa6da auto
// OFFSET: 0x0081B7B0
int32_t SimpleFrameScript_CreateFont(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: CreateFont(\"name\")");
        return 0;
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CSimpleFont* font = nullptr;
    try { font = CSimpleFont::GetFont(lua_tolstring(L, 1, nullptr), 1); }
    catch (const std::exception&) {} // Leave C++ exception scope before Lua's longjmp.
    if (!font) return luaL_error(L, "CreateFont(): invalid name or native font capacity exceeded");
#else
    CSimpleFont* font = CSimpleFont::GetFont(lua_tolstring(L, 1, nullptr), 1);
#endif

    if (!font->m_luaRegistered) {
        font->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, font->m_luaObjectRef);
    return 1;
}

