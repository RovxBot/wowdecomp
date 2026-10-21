#include "Runtime.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include <atomic>
#include <unordered_set>
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include <stdexcept>
#include <utility>

namespace FrameScript { namespace Native {
namespace {
std::atomic<Runtime*> active{nullptr};
std::unordered_set<FrameScript_Object*> objects;
size_t executions = 0;
bool Normalize(const char* name, std::string& normalized) {
    normalized.clear();
    if (!name || !*name || name[0] == '/' || name[0] == '\\') return false;
    std::vector<std::string> parts;
    std::string part;
    for (const char* p = name;; ++p) {
        if (p - name >= 260 || *p == ':') return false;
        if (*p && *p != '/' && *p != '\\') { part += *p; continue; }
        if (part == "..") {
            if (parts.empty()) return false;
            parts.pop_back();
        } else if (!part.empty() && part != ".") parts.push_back(part);
        part.clear();
        if (!*p) break;
    }
    for (const auto& component : parts) {
        if (!normalized.empty()) normalized += '\\';
        normalized += component;
    }
    return !normalized.empty();
}
}
Runtime::Runtime(SourceReader source, bool profile)
    : owner(std::this_thread::get_id()), reader(std::move(source)) {
    if (!reader) throw std::invalid_argument("FrameScript requires a completed-read provider");
    Runtime* expected = nullptr;
    if (!active.compare_exchange_strong(expected, this))
        throw std::logic_error("Only one native FrameScript runtime may be installed");
    try {
        if (!FrameScript_Initialize(profile)) throw std::runtime_error("FrameScript initialization failed");
    } catch (...) {
        if (s_context) FrameScript_Destroy();
        active.store(nullptr);
        throw;
    }
}
Runtime::~Runtime() {
    try {
        RequireIdle();
        if (s_context) FrameScript_Destroy();
        active.store(nullptr);
    } catch (...) { std::terminate(); }
}
void Runtime::Check() const {
    if (std::this_thread::get_id() != owner || active.load() != this)
        throw std::logic_error("FrameScript requires its owner thread and installed runtime");
}
bool Runtime::Read(SourceKind kind, const char* name, std::string& path, std::vector<uint8_t>& bytes) const {
    Check();
    bytes.clear();
    if (!Normalize(name, path)) return false;
    // Keep arbitrary provider exceptions outside the VM and report a failed read.
    try {
        if (!reader(kind, path, bytes) || bytes.size() > 8 * 1024 * 1024u) {
            bytes.clear(); return false;
        }
    } catch (...) { bytes.clear(); return false; }
    return true;
}
void RequireOwner() {
    auto* runtime = active.load();
    if (!runtime) throw std::logic_error("Native FrameScript runtime is not installed");
    runtime->Check();
}
void RequireIdle() {
    RequireOwner();
    if (!objects.empty() || executions) throw std::logic_error("Close FrameScript objects and finish execution before VM reset");
}
void ObjectCreated(FrameScript_Object* object) {
    RequireOwner();
    if (!s_context) throw std::logic_error("FrameScript objects require an initialized VM");
    objects.insert(object);
}
void ObjectDestroyed(FrameScript_Object* object) {
    RequireOwner(); objects.erase(object);
    if (objects.empty()) std::unordered_set<FrameScript_Object*>().swap(objects);
}
FrameScript_Object* GetObject(lua_State* L, int index) {
    RequireOwner();
    if (L != s_context || lua_type(L, index) != LUA_TTABLE) return nullptr;
    lua_rawgeti(L, index, 0);
    auto* object = static_cast<FrameScript_Object*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!objects.count(object) || !object->m_luaRegistered) return nullptr;
    lua_rawgeti(L, LUA_REGISTRYINDEX, object->m_luaObjectRef);
    // All callers supply a positive stack index.
    bool same = lua_rawequal(L, index, -1);
    lua_pop(L, 1);
    return same ? object : nullptr;
}
bool ReadSource(SourceKind kind, const char* name, std::string& path, std::vector<uint8_t>& bytes) {
    RequireOwner(); return active.load()->Read(kind, name, path, bytes);
}
Execution::Execution() { RequireOwner(); ++executions; }
Execution::~Execution() { --executions; }
}}
