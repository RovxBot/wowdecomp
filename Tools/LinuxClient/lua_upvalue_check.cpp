// Exercise the full client's actual Lua runtime, including lfunc.c and GC.
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lfunc.h"
#include "Engine/Source/lua-5.1/src/lgc.h"
#include "Engine/Source/lua-5.1/src/lstate.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static void check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Lua upvalue check failed at line %d\n", line);
        std::abort(); // Active in release/NDEBUG, too.
    }
}
#define require(condition) check((condition), __LINE__)

struct Allocations { size_t live = 0, calls = 0, frees = 0; };
static void* allocate(void* context, void* pointer, size_t, size_t size) {
    auto& counts = *static_cast<Allocations*>(context);
    if (!size) {
        if (pointer) { --counts.live; ++counts.frees; }
        std::free(pointer);
        return nullptr;
    }
    const bool fresh = !pointer;
    void* result = std::realloc(pointer, size);
    if (result) { counts.live += fresh; ++counts.calls; }
    return result;
}

static void lists(lua_State* L, std::initializer_list<UpVal*> expected) {
    GCObject* node = L->openupval;
    for (UpVal* uv : expected) {
        require(node == obj2gco(uv));
        require(uv->v != &uv->u.value);
        node = uv->next;
    }
    require(node == nullptr);
    UpVal* head = &G(L)->uvhead;
    UpVal* previous = head;
    size_t count = 0;
    for (UpVal* uv = head->u.l.next; uv != head; uv = uv->u.l.next) {
        require(++count <= expected.size());
        require(uv->u.l.prev == previous);
        bool found = false;
        for (UpVal* candidate : expected) found |= uv == candidate;
        require(found);
        previous = uv;
    }
    require(count == expected.size() && head->u.l.prev == previous);
}

static bool rooted(lua_State* L, UpVal* uv) {
    for (GCObject* node = G(L)->rootgc; node; node = node->gch.next)
        if (node == obj2gco(uv)) return true;
    return false;
}

int main() {
    for (int profile : {0, 1}) {
        Allocations counts;
        lua_State* L = lua_newstate(allocate, &counts, profile);
        require(L != nullptr);
        lua_gc(L, LUA_GCSTOP, 0); // Keep unreferenced manual test upvalues alive.
        lua_settop(L, 8);
        for (int i = 1; i <= 8; ++i) {
            lua_pushnumber(L, 100 + i);
            lua_replace(L, i);
        }
        StkId base = L->base; // All comparisons use this one live stack array.
        GCObject* empty = nullptr;
        require(ngcotouv(empty) == nullptr); // The exact formerly-UB conversion.
        lists(L, {});
        luaF_close(L, base); // Empty close also uses the nullable conversion.
        UpVal* middle = luaF_findupval(L, base + 3); // Empty lookup boundary.
        lists(L, {middle});
        UpVal* high = luaF_findupval(L, base + 6); // Insert before first.
        UpVal* between = luaF_findupval(L, base + 5); // Insert between nodes.
        UpVal* low = luaF_findupval(L, base + 1); // Traverse to null tail.
        lists(L, {high, between, middle, low});
        const size_t calls = counts.calls;
        for (UpVal* uv : {high, between, middle, low})
            require(luaF_findupval(L, uv->v) == uv);
        require(counts.calls == calls); // Reuse at head, middle and tail.

        // Reuse must resurrect a dead open upvalue, without allocation/relink.
        middle->marked = cast_byte(otherwhite(G(L)) & WHITEBITS);
        require(isdead(G(L), obj2gco(middle)));
        require(luaF_findupval(L, base + 3) == middle);
        require(!isdead(G(L), obj2gco(middle)) && counts.calls == calls);
        lists(L, {high, between, middle, low});

        static const char tag[] = "authored-upvalue-tag";
        lua_tainted = tag;
        lua_gctag = tag;
        UpVal* tagged = luaF_findupval(L, base + 4);
        require(tagged->taint == tag);
        lua_tainted = lua_gctag = nullptr;
        lists(L, {high, between, tagged, middle, low});
        luaF_close(L, base + 7); // Nonempty list, nothing in closing range.
        lists(L, {high, between, tagged, middle, low});
        luaF_close(L, base + 4); // Inclusive close boundary, retained lower nodes.
        lists(L, {middle, low});
        for (UpVal* uv : {high, between, tagged}) {
            require(uv->v == &uv->u.value && rooted(L, uv));
        }
        require(nvalue(high->v) == 107 && nvalue(between->v) == 106);
        require(nvalue(tagged->v) == 105 && tagged->taint == tag);
        setnvalue(base + 6, -1);
        require(nvalue(high->v) == 107); // Closed storage no longer aliases stack.
        low->marked = cast_byte(otherwhite(G(L)) & WHITEBITS);
        const size_t frees = counts.frees;
        luaF_close(L, base);
        require(counts.frees == frees + 1); // Dead open tail is freed, not closed.
        require(middle->v == &middle->u.value && nvalue(middle->v) == 104);
        lists(L, {});
        luaF_close(L, base); // Empty after consuming a nonempty list.
        lua_settop(L, 0);
        lua_gc(L, LUA_GCCOLLECT, 0);

        // VM-created closures share one upvalue, retain closed values through
        // collection, and release them through the real GC/state shutdown.
        const char* source =
            "local x = 40; return function() x = x + 1; return x end, "
            "function() return x end";
        require(luaL_loadbuffer(L, source, std::strlen(source), "@upvalue-fixture") == 0);
        require(lua_pcall(L, 0, 2, 0) == 0);
        Closure* first = clvalue(L->base);
        Closure* second = clvalue(L->base + 1);
        require(first->l.upvals[0] == second->l.upvals[0]);
        UpVal* shared = first->l.upvals[0];
        require(shared->v == &shared->u.value && nvalue(shared->v) == 40);
        lists(L, {});
        lua_gc(L, LUA_GCCOLLECT, 0);
        for (int index : {1, 2}) {
            lua_pushvalue(L, index);
            require(lua_pcall(L, 0, 1, 0) == 0 && lua_tonumber(L, -1) == 41);
            lua_pop(L, 1);
        }
        lua_close(L);
        require(counts.live == 0);
    }
    std::puts("lua-upvalue=passed: empty/tail boundaries, ordering/reuse, resurrection, taint, close, closures/GC/shutdown");
}
