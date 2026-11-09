#include "Engine/Source/Frame/RetailUI/XMLCompat.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdlib>
#include <cstring>

namespace RovRetailUI {
static bool identifier(const char* name) {
    if (!name || !*name) return false;
    for (size_t i = 0; name[i]; ++i) {
        const unsigned char c = name[i];
        if (!(c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (i && c >= '0' && c <= '9'))) return false;
    }
    return true;
}

bool ApplyMixins(lua_State* L, int objectRef, const char* names, CStatus* status) {
    if (!names) return true;
    if (!Enabled()) {
        status->Add(STATUS_ERROR, "Retail XML mixin requires WOWDECOMP_RETAIL_UI_COMPAT=%s", Version);
        return false;
    }
    const int top = lua_gettop(L);
    auto fail = [&](const char* reason) {
        lua_settop(L, top);
        status->Add(STATUS_ERROR, "Retail XML mixin: %s", reason);
        return false;
    };
    if (!*names || std::strlen(names) > 4096) return fail("empty or excessive list");
    lua_rawgeti(L, LUA_REGISTRYINDEX, objectRef);
    if (!lua_istable(L, -1)) return fail("missing frame script object");
    const int target = lua_gettop(L);
    // Stage the entire list before touching the frame. A late missing mixin or
    // bad key cannot leave a half-applied behaviour or overwrite native slot 0.
    lua_newtable(L);
    const int staged = lua_gettop(L);
    unsigned count = 0;
    const char* cursor = names;
    for (;;) {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
        char name[256];
        size_t length = 0;
        while (*cursor && *cursor != ',') {
            if (length + 1 == sizeof(name)) return fail("mixin name too long");
            name[length++] = *cursor++;
        }
        while (length && (name[length - 1] == ' ' || name[length - 1] == '\t' ||
                          name[length - 1] == '\r' || name[length - 1] == '\n')) --length;
        name[length] = 0;
        if (!identifier(name) || ++count > 32) return fail("invalid mixin name or excessive count");
        lua_pushstring(L, name);
        lua_rawget(L, LUA_GLOBALSINDEX);
        if (!lua_istable(L, -1)) {
            status->Add(STATUS_ERROR, "Retail XML mixin table unavailable: %s", name);
            lua_settop(L, top);
            return false;
        }
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            if (lua_type(L, -2) != LUA_TSTRING) return fail("only string fields are supported; native identity is reserved");
            lua_pushvalue(L, -2);
            lua_pushvalue(L, -2);
            lua_rawset(L, staged);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        if (!*cursor) break;
        ++cursor;
    }
    lua_pushnil(L);
    while (lua_next(L, staged)) {
        lua_pushvalue(L, -2);
        lua_pushvalue(L, -2);
        lua_rawset(L, target);
        lua_pop(L, 1);
    }
    lua_settop(L, top);
    return true;
}

bool HasScriptAttributes(XMLNode* node) {
    return Enabled() && (node->GetAttributeByName("method") || node->GetAttributeByName("inherit"));
}

static int chain(lua_State* L) {
    const int args = lua_gettop(L);
    if (!lua_checkstack(L, args + 1)) return luaL_error(L, "Retail XML handler argument stack exhausted");
    for (int i = 1; i <= 2; ++i) {
        lua_pushvalue(L, lua_upvalueindex(i));
        for (int j = 1; j <= args; ++j) lua_pushvalue(L, j);
        // Use the VM's normal call/error/taint path. An error stops the chain.
        lua_call(L, args, 0);
    }
    return 0;
}

bool LoadScript(lua_State* L, int objectRef, XMLNode* node,
                FrameScript_Object::ScriptIx& script,
                const FrameScript_Object::ScriptData& data, CStatus* status) {
    const int top = lua_gettop(L);
    auto fail = [&](const char* reason) {
        lua_settop(L, top);
        status->Add(STATUS_ERROR, "Retail XML %s: %s", node->m_name.m_str, reason);
        return false;
    };
    if (!Enabled()) return fail("compatibility version not enabled");
    const char* method = node->GetAttributeByName("method");
    const char* function = node->GetAttributeByName("function");
    const char* inherit = node->GetAttributeByName("inherit");
    const char* body = node->m_body;
    bool hasBody = false;
    for (const char* p = body; p && *p; ++p)
        hasBody |= *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n';
    const bool append = inherit && !std::strcmp(inherit, "append");
    const bool prepend = inherit && !std::strcmp(inherit, "prepend");
    if (inherit && !append && !prepend) return fail("inherit must be append or prepend");
    // No untainting or ownership merging across addon/security boundaries.
    if (inherit && script.m_luaRef && script.m_tainted != lua_tainted)
        return fail("cross-owner script inheritance is unsupported");
    if (method) {
        if (!identifier(method) || function || hasBody) return fail("method needs one identifier and no function/body");
        lua_rawgeti(L, LUA_REGISTRYINDEX, objectRef);
        if (!lua_istable(L, -1)) return fail("missing frame script object");
        lua_getfield(L, -1, method);
        lua_remove(L, -2);
    } else if (function && *function) {
        if (hasBody) return fail("ambiguous function/body");
        lua_pushstring(L, function);
        lua_rawget(L, LUA_GLOBALSINDEX);
    } else if (hasBody) {
        const int ref = FrameScript_CompileFunction("*:RetailXML", data.m_wrapper, body, status);
        if (ref <= 0) return fail("inline handler compilation failed");
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
    } else {
        return fail("missing handler");
    }
    if (!lua_isfunction(L, -1)) return fail("handler function unavailable");
    if (inherit && script.m_luaRef) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, script.m_luaRef);
        if (!lua_isfunction(L, -1)) return fail("inherited handler unavailable");
        // C closure upvalues are execution order: old,new for append.
        if (append) lua_insert(L, -2);
        lua_pushcclosure(L, chain, 2);
    }
    const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    if (script.m_luaRef) luaL_unref(L, LUA_REGISTRYINDEX, script.m_luaRef);
    script.m_luaRef = ref;
    script.m_tainted = lua_tainted;
    lua_settop(L, top);
    return true;
}
}
