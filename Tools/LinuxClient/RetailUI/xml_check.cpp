// Authored fixtures use the actual full-client Lua, XML nodes and compatibility
// implementation. Optional local reference execution never packages input files.
#include "Engine/Source/Frame/RetailUI/XMLCompat.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Base/RCString.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

static void check(bool ok, int line) {
    if (!ok) { std::fprintf(stderr, "retail XML check failed at %d\n", line); std::abort(); }
}
#define require(x) check(bool(x), __LINE__)

static void run(lua_State* L, const char* code) {
    if (luaL_loadstring(L, code) || lua_pcall(L, 0, 0, 0)) {
        std::fprintf(stderr, "Lua: %s\n", lua_tostring(L, -1));
        std::abort();
    }
}

static bool bind(lua_State* L, int object, FrameScript_Object::ScriptIx& handler,
                 const char* method, const char* inherit = nullptr,
                 const char* function = nullptr, const char* body = nullptr) {
    XMLNode node(nullptr, "OnEvent");
    if (method) node.SetAttribute("method", method);
    if (inherit) node.SetAttribute("inherit", inherit);
    if (function) node.SetAttribute("function", function);
    if (body) { node.m_bodyLen = std::strlen(body); node.m_body = SStrDupA(body, __FILE__, __LINE__); }
    CStatus status;
    FrameScript_Object::ScriptData data{"return function(self,event,...) %s end"};
    const int top = lua_gettop(L);
    const bool ok = RovRetailUI::LoadScript(L, object, &node, handler, data, &status);
    require(lua_gettop(L) == top);
    require(ok == (status.m_highestSeverity < STATUS_ERROR));
    return ok;
}

static int invoke(lua_State* L, int object, FrameScript_Object::ScriptIx& handler) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, handler.m_luaRef);
    lua_rawgeti(L, LUA_REGISTRYINDEX, object);
    lua_pushstring(L, "EVENT");
    lua_pushnumber(L, 7);
    lua_pushnil(L);
    return lua_pcall(L, 4, 0, 0);
}

static void fixture(lua_State* L) {
    run(L, R"lua(
        result = '';
        A = { value = 1, First = function(self, event, number, empty)
            assert(event == 'EVENT' and number == 7 and empty == nil);
            result = result .. 'A'; self.calls = (self.calls or 0) + 1;
        end };
        B = { value = 2, Second = function(self, event, ...)
            assert(select('#', ...) == 2); result = result .. 'B';
        end };
        Invalid = { [0] = 'overwrite' };
        Bad = { Fail = function() error('expected-handler-failure') end };
        GlobalHandler = function() result = result .. 'G' end;
        target = { [0] = 'native-identity', original = true };
    )lua");
    lua_getglobal(L, "target");
    const int object = luaL_ref(L, LUA_REGISTRYINDEX);
    auto mix = [&](const char* names) {
        CStatus status;
        const int top = lua_gettop(L);
        const bool ok = RovRetailUI::ApplyMixins(L, object, names, &status);
        require(lua_gettop(L) == top);
        require(ok == (status.m_highestSeverity < STATUS_ERROR));
        return ok;
    };
    unsetenv("WOWDECOMP_RETAIL_UI_COMPAT");
    require(!mix("A"));
    setenv("WOWDECOMP_RETAIL_UI_COMPAT", "wrong-version", 1);
    require(!mix("A"));
    setenv("WOWDECOMP_RETAIL_UI_COMPAT", RovRetailUI::Version, 1);
    for (const char* names : {"A, Missing", "A, Invalid", "A,", "A,,B", "A.B", "", "A; error('injection')"}) {
        require(!mix(names));
        run(L, "assert(target.value == nil and target[0] == 'native-identity')");
    }
    require(mix(" A,\tB, Bad "));
    run(L, "assert(target.value == 2 and target.original and target[0] == 'native-identity')");
    {
        FrameScript_Object::ScriptIx handler;
        require(bind(L, object, handler, "First"));
        require(bind(L, object, handler, "Second", "append"));
        require(invoke(L, object, handler) == 0);
        run(L, "assert(result == 'AB' and target.calls == 1); result = ''");
        require(bind(L, object, handler, nullptr, "prepend", "GlobalHandler"));
        require(invoke(L, object, handler) == 0);
        run(L, "assert(result == 'GAB'); result = ''");
        require(bind(L, object, handler, nullptr, "append", nullptr, "assert(event == 'EVENT'); result = result .. 'I'"));
        require(invoke(L, object, handler) == 0);
        run(L, "assert(result == 'GABI'); result = ''");
        const int old = handler.m_luaRef;
        require(!bind(L, object, handler, "Missing"));
        require(!bind(L, object, handler, "First", "invalid"));
        require(!bind(L, object, handler, "First", nullptr, "GlobalHandler"));
        require(!bind(L, object, handler, "First", nullptr, nullptr, "result = ''"));
        require(handler.m_luaRef == old);
        unsetenv("WOWDECOMP_RETAIL_UI_COMPAT");
        require(!bind(L, object, handler, "First"));
        require(handler.m_luaRef == old);
        setenv("WOWDECOMP_RETAIL_UI_COMPAT", RovRetailUI::Version, 1);
        // First failure propagates through pcall and does not invoke the tail.
        require(bind(L, object, handler, "Fail", "prepend"));
        require(invoke(L, object, handler) != 0);
        require(std::strstr(lua_tostring(L, -1), "expected-handler-failure"));
        lua_pop(L, 1);
        run(L, "assert(result == '')");
        require(bind(L, object, handler, "First")); // ordinary override retires chain
        require(invoke(L, object, handler) == 0);
        run(L, "assert(result == 'A'); result = ''");
    }
    // Never erase or merge a different script owner to make inheritance pass.
    {
        FrameScript_Object::ScriptIx handler;
        static const char owner[] = "RetailFixtureOwner";
        lua_tainted = owner;
        require(mix("A"));
        require(bind(L, object, handler, "First"));
        require(handler.m_tainted == owner && lua_tainted == owner);
        lua_tainted = nullptr;
        require(!bind(L, object, handler, "Second", "append"));
        require(handler.m_tainted == owner);
    }
    luaL_unref(L, LUA_REGISTRYINDEX, object);
    lua_gc(L, LUA_GCCOLLECT, 0);
}

static void reference(lua_State* L, const char* path) {
    std::ifstream input(path, std::ios::binary);
    require(input.good());
    std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    require(source.size() < 1024 * 1024);
    require(luaL_loadbuffer(L, source.data(), source.size(), "@local-Forever-reference") == 0);
    require(lua_pcall(L, 0, 0, 0) == 0);
    // These are table-only markers, not reconstructed gameplay frames. Verify
    // the genuine Camelot mixin's ordering through the production XML binder.
    run(L, "MainActionBar = {}; MicroMenu = {}; BagsBar = {}; OverrideActionBar = {}; MultiBarBottomLeft = {}; MicroMenuContainer = {}; target = {[0] = 'native-identity'}");
    lua_getglobal(L, "target");
    const int object = luaL_ref(L, LUA_REGISTRYINDEX);
    CStatus status;
    require(RovRetailUI::ApplyMixins(L, object, "EditModeUtil", &status));
    {
        FrameScript_Object::ScriptIx handler;
        require(bind(L, object, handler, "GetCenterManagedSystems"));
        lua_rawgeti(L, LUA_REGISTRYINDEX, handler.m_luaRef);
        lua_rawgeti(L, LUA_REGISTRYINDEX, object);
        require(lua_pcall(L, 1, 1, 0) == 0);
        lua_setglobal(L, "centers");
        run(L, "assert(#centers == 3 and centers[1] == MainActionBar and centers[2] == MicroMenu and centers[3] == BagsBar)");
        require(bind(L, object, handler, "GetBottomActionBarHierarchy"));
        lua_rawgeti(L, LUA_REGISTRYINDEX, handler.m_luaRef);
        lua_rawgeti(L, LUA_REGISTRYINDEX, object);
        require(lua_pcall(L, 1, 1, 0) == 0);
        lua_setglobal(L, "hierarchy");
        run(L, "assert(#hierarchy == 3 and hierarchy[1] == OverrideActionBar and hierarchy[2] == MultiBarBottomLeft and hierarchy[3] == MicroMenuContainer)");
    }
    luaL_unref(L, LUA_REGISTRYINDEX, object);
    std::puts("genuine-Camelot-EditModeUtil=passed (Lua table fixture; no gameplay/UI rendering claim)");
}

int main(int argc, char** argv) {
    require(argc == 1 || (argc == 3 && !std::strcmp(argv[1], "--reference")));
    StormRtlInitialize();
    OsTimeManagerCreate(OsTimeManager::BestAvailable);
    for (int profile : {0, 1}) {
        require(FrameScript_Initialize(profile));
        lua_State* L = FrameScript_GetContext();
        fixture(L);
        if (argc == 3) reference(L, argv[2]);
        require(lua_gettop(L) == 0);
        FrameScript_Destroy();
        XMLTree_Cleanup();
    }
    CStringManager::DestroyManager();
    std::puts("retail-xml-v1=passed: opt-in, atomic mixins, methods, inheritance, errors, owner/taint, GC; both VM profiles");
}
