#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lualib.h"
#include "Engine/Source/lua-5.1/src/lmemPool.h"
#include "Engine/Source/lua-5.1/src/lstate.h"
#include "Engine/Source/Os/System.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <iterator>

static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "Lua check failed: %s\n", message); std::exit(1); }
}
static void Run(lua_State* state, const char* source) {
    const auto status = luaL_loadbuffer(state, source, std::strlen(source), "@authored-lua-fixture");
    Require(status == 0, status ? lua_tostring(state, -1) : "load");
    const auto result = lua_pcall(state, 0, 0, 0);
    Require(result == 0, result ? lua_tostring(state, -1) : "execute");
    Require(lua_gettop(state) == 0, "balanced execution stack");
}
static int Add(lua_State* state) {
    lua_pushnumber(state, luaL_checknumber(state, 1) + luaL_checknumber(state, 2));
    return 1;
}
static int Dump(lua_State*, const void* bytes, size_t size, void* context) {
    auto& output = *static_cast<std::vector<char>*>(context);
    output.insert(output.end(), static_cast<const char*>(bytes), static_cast<const char*>(bytes) + size);
    return 0;
}
static int hookCalls;
static void Hook(lua_State* state, lua_Debug*) {
    if (++hookCalls > 10) luaL_error(state, "authored instruction limit");
}
static void* FailAllocation(void*, void*, size_t, size_t) { return nullptr; }

static void PoolCheck(void* pool) {
    // Cross every size-class boundary in both directions, checking retained bytes.
    const size_t sizes[] = {1,16,17,24,25,32,33,40,41,64,65,80,81,128,129,160,161,256,257,4096};
    void* allocation = nullptr;
    size_t old = 0;
    auto resize = [&](size_t size) {
        allocation = luaM_reallocPool(pool, allocation, old, size);
        Require(allocation != nullptr, "pool reallocation");
        Require(reinterpret_cast<uintptr_t>(allocation) % alignof(void*) == 0, "pool alignment");
        for (size_t i = 0; i < std::min(old, size); ++i)
            Require(static_cast<unsigned char*>(allocation)[i] == 0xA7, "pool retained bytes");
        std::memset(allocation, 0xA7, size); old = size;
    };
    for (auto size : sizes) resize(size);
    for (auto i = std::size(sizes); i; --i) resize(sizes[i-1]);
    Require(luaM_reallocPool(pool, allocation, old, 0) == nullptr, "pool release");
    std::vector<void*> blocks;
    for (size_t i = 0; i < 1100; ++i) {
        auto* block = luaM_reallocPool(pool, nullptr, 0, 24);
        Require(block != nullptr, "multiple pool chunks");
        blocks.push_back(block);
    }
    for (auto* block : blocks) luaM_reallocPool(pool, block, 24, 0);
}

int main(int argc, char** argv) {
    static int data;
    if (argc == 2) {
        void* heap = std::malloc(16);
        const void* pointer = !std::strcmp(argv[1], "--reject-null") ? nullptr :
            !std::strcmp(argv[1], "--reject-data") ? &data : heap;
        OsValidateFunctionPointer(pointer);
        std::free(heap);
        return 0;
    }
    OsValidateFunctionPointer(reinterpret_cast<const void*>(&Add));
    Require(lua_newstate(FailAllocation, nullptr, 0) == nullptr, "failed initial allocation");
    for (int cycle = 0; cycle < 3; ++cycle) for (int profile = 0; profile <= 1; ++profile) {
        lua_tainted = nullptr; lua_taintexpected = 0; lua_taintedclosure = 0;
        lua_closuretaint = nullptr; lua_gctag = nullptr;
        void* pool = luaM_initPool(); PoolCheck(pool);
        auto* state = lua_newstate(luaM_reallocPool, pool, profile);
        Require(state != nullptr, "product pool-backed state");
        // The same five libraries and stack cleanup as FrameScript_Initialize.
        luaopen_base(state); lua_pop(state, 2);
        luaopen_string(state); lua_pop(state, 1);
        luaopen_table(state); lua_pop(state, 1);
        luaopen_math(state); lua_pop(state, 1);
        luaopen_bit(state); lua_pop(state, 1);
        lua_register(state, "native_add", Add);
        Run(state, R"lua(
            assert(native_add(19,23)==42)
            local function factory(n) return function(x) return n+x end end
            assert(factory(7)(5)==12)
            local t=setmetatable({}, {__index=function(_,k) return k..'!' end})
            assert(t.test=='test!')
            local values={3,1,2}; table.sort(values)
            assert(table.concat(values,',')=='1,2,3')
            assert(string.gsub('abc123','(%d+)','[%1]')=='abc[123]')
            assert(math.floor(3.9)==3 and bit.band(7,3)==3)
            assert(bit.bnot(0)==4294967295 and bit.lshift(1,30)==1073741824)
            assert(bit.rshift(-1,1)==2147483647 and bit.arshift(-8,1)==-4)
            local co=coroutine.create(function(x) coroutine.yield(x+1); return 9 end)
            local ok,n=coroutine.resume(co,4); assert(ok and n==5)
            ok,n=coroutine.resume(co); assert(ok and n==9)
            local ok,message=pcall(function() error('expected failure') end)
            assert(not ok and string.find(message,'expected failure'))
            assert(io==nil and os==nil and package==nil and debug==nil)
            function measured(n) local sum=0; for i=1,n do sum=sum+i end; return sum end
            assert(measured(100)==5050)
            local objects={}; for i=1,5000 do objects[i]={i,tostring(i)} end
            objects=nil; collectgarbage('collect')
        )lua");
        lua_getglobal(state, "measured");
        double elapsed = -1, total = -1; int calls = -1;
        lua_gcgetfunctiontime(state, &elapsed, &total, &calls);
        Require(std::isfinite(elapsed) && elapsed >= 0 && total >= 0, "profile monotonic clock units");
        Require(calls == (profile ? 1 : 0), "product profile call count");
        std::vector<char> bytecode;
        Require(lua_dump(state, Dump, &bytecode) == 0 && !bytecode.empty(), "native bytecode dump");
        lua_pop(state, 1);
        Require(luaL_loadbuffer(state, bytecode.data(), bytecode.size(), "@native-roundtrip") == 0, "native bytecode reload");
        lua_pushnumber(state, 10);
        Require(lua_pcall(state, 1, 1, 0) == 0 && lua_tonumber(state, -1) == 55, "native bytecode execution");
        lua_pop(state, 1);
        bytecode[6] ^= 1; // foreign endianness in the Lua 5.1 header
        Require(luaL_loadbuffer(state, bytecode.data(), bytecode.size(), "@foreign-header") == LUA_ERRSYNTAX, "foreign bytecode rejection");
        lua_pop(state, 1);
        Require(luaL_loadstring(state, "function broken(") == LUA_ERRSYNTAX, "syntax error"); lua_pop(state, 1);
        hookCalls = 0; lua_sethook(state, Hook, LUA_MASKCOUNT, 100);
        Require(luaL_loadstring(state, "while true do end") == 0, "hook source");
        Require(lua_pcall(state, 0, 0, 0) == LUA_ERRRUN && hookCalls > 10, "protected instruction hook abort");
        lua_sethook(state, nullptr, 0, 0); lua_pop(state, 1);
        // Exercise product value taint, raw storage, read propagation and chunk tagging.
        static const char tag[] = "AuthoredAddon";
        lua_pushnumber(state, 123); lua_settaint(state, -1, tag);
        lua_setglobal(state, "tainted_value");
        lua_taintexpected = 1; lua_tainted = nullptr;
        lua_getglobal(state, "tainted_value");
        Require(lua_tainted == tag, "product global taint propagation"); lua_pop(state, 1);
        lua_tainted = tag; lua_gctag = tag;
        Require(luaL_loadstring(state, "return function() return 7 end") == 0, "tagged chunk");
        lua_gcupdatesizes(state);
        Require(lua_gcgetsize(state, tag) > 0, "product per-addon allocation accounting");
        lua_tainted = nullptr; lua_taintexpected = 0; lua_gctag = nullptr; lua_pop(state, 1);
        lua_gcupdatetimes(state); lua_gcresettimes(state);
        Run(state, "assert(native_add(2,3)==5); collectgarbage('collect')");
        lua_close(state); luaM_freePool(pool);
        std::printf("Lua cycle %d profile %d: pool/libraries/coroutines/errors/taint/profile/GC passed\n", cycle, profile);
    }
}
