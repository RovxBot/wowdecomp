// Native adaptation of original 12340 FrameXML registry/template creation and
// SimpleFrameScript_Scripts.cpp::Script_CreateFrame. No new address/VERIFY claim.
#include "Factories.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Frame/Portable/Scripts.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Storm/H/storm.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

namespace {
bool installed = false;
size_t templateCount = 0;
TSHashTable<HashedNode, HASHKEY_STRI> templates;
TSHashTable<FrameFactoryNode, HASHKEY_STRI> factories;
size_t creationDepth = 0;
std::vector<CSimpleFrame*> created;
struct CreationScope {
    bool complete = false;
    CreationScope() { ++creationDepth; }
    ~CreationScope() {
        if (--creationDepth) return;
        if (!complete) {
            // Explicit XML parents can put newly created descendants outside
            // the root being unwound. Retire every live allocation in reverse.
            for (auto i = created.rbegin(); i != created.rend(); ++i) {
                bool live = true;
                try { Frame::Native::RequireSimpleFrame(*i); }
                catch (const std::invalid_argument&) { live = false; }
                if (live) delete *i;
            }
        }
        std::vector<CSimpleFrame*>().swap(created);
    }
};
bool Is(const char* a, const char* b) { return !SStrCmpI(a, b, STORM_MAX_STR); }
void Name(const char* value) { Frame::Native::RequireFontName(value); }
std::vector<std::string> Names(const char* value) {
    std::vector<std::string> result;
    if (!value || !*value) return result;
    if (std::strlen(value) > 4095) throw std::length_error("Native template list limit");
    while (*value) {
        char name[1024];
        const char* start = value;
        while (*start == ' ' || *start == ',') ++start;
        const char* end = start;
        while (*end && *end != ' ' && *end != ',') ++end;
        if (end - start >= 1024) throw std::length_error("Native template name limit");
        SStrTokenize(&value, name, sizeof(name), " ,", nullptr);
        if (!*name) break;
        result.emplace_back(name);
        if (result.size() > 64) throw std::length_error("Native template count limit");
    }
    return result;
}
std::vector<std::string> InheritedNames(XMLNode* node) {
    const char* inherits = node->GetAttributeByName("inherits");
    // Original Frame lists tokenize; FontString/Texture inheritance is one exact key.
    if (Is(node->m_name.m_str, "FontString") || Is(node->m_name.m_str, "Texture")) {
        if (!inherits || !*inherits) return {};
        Name(inherits);
        return {inherits};
    }
    return Names(inherits);
}
struct TemplateScope {
    const char* name;
    XMLNode* node;
    const char* savedTaint = lua_tainted;
    const char* savedTag = lua_gctag;
    bool applyTaint;
    TemplateScope(const char* value, bool tainted) : name(value), applyTaint(tainted) {
        const char* taint = nullptr;
        bool locked = false;
        node = FrameXML_AcquireHashNode(name, taint, locked);
        if (!node || locked) throw std::invalid_argument("Missing or recursively inherited XML template");
        if (applyTaint) {
            ++lua_taintexpected;
            if (!lua_taintedclosure) lua_tainted = taint;
            lua_gctag = taint;
        }
    }
    ~TemplateScope() {
        if (applyTaint) {
            lua_gctag = savedTag;
            if (lua_taintexpected && !lua_taintedclosure) lua_tainted = savedTaint;
            if (--lua_taintexpected < 0) lua_taintexpected = 0;
        }
        FrameXML_ReleaseHashNode(name);
    }
};
void Validate(XMLNode* node, std::vector<XMLNode*>& path, size_t& visits) {
    if (!node || path.size() >= 64 || ++visits > 4096 ||
        std::find(path.begin(), path.end(), node) != path.end())
        throw std::invalid_argument("Cyclic or excessive native XML creation graph");
    path.push_back(node);
    const char* tag = node->m_name.m_str;
    // Fail before allocation for consumers with unselected ownership/services.
    const char* supported[] = {"Frame", "FontString", "TitleRegion", "Size", "Anchors", "Anchor", "Offset",
        "AbsDimension", "RelDimension", "Layers", "Layer", "Frames", "FontHeight", "AbsValue", "RelValue",
        "Color", "Shadow", "ResizeBounds", "minResize", "maxResize", "Texture", "TexCoords", "Rect", "Gradient", "MinColor", "MaxColor"};
    if (std::none_of(std::begin(supported), std::end(supported), [&](const char* s) { return Is(tag, s); }))
        throw std::invalid_argument("Native XML widget or child service is unavailable");
    if (Is(tag, "Texture")) {
        for (auto* key : {"horizTile", "vertTile"})
            if (auto* value = node->GetAttributeByName(key); value && StringToBOOL(value))
                throw std::invalid_argument("Native texture tiling needs sampler integration");
    }
    if (auto* name = node->GetAttributeByName("name"); name && *name) Name(name);
    for (uint32_t i = 0; i < node->m_attributes.Count(); ++i) {
        auto& attr = node->m_attributes[i];
        const char* key = attr.m_name.m_str;
        const char* value = attr.m_value.m_str;
        if (!value || !*value) continue;
        const char* numeric[] = {"x", "y", "val", "r", "g", "b", "a", "alpha", "depth", "spacing", "left", "right", "top", "bottom", "ULx", "ULy", "LLx", "LLy", "URx", "URy", "LRx", "LRy"};
        const char* integer[] = {"id", "bytes", "maxLines", "frameLevel"};
        bool integral = std::any_of(std::begin(integer), std::end(integer), [&](const char* s) { return Is(key, s); });
        if (integral || std::any_of(std::begin(numeric), std::end(numeric), [&](const char* s) { return Is(key, s); })) {
            char* end = nullptr;
            double number = std::strtod(value, &end);
            if (*end || end == value || !std::isfinite(number) || std::fabs(number) > FLT_MAX ||
                (integral && (number < INT32_MIN || number > INT32_MAX || std::trunc(number) != number)))
                throw std::invalid_argument("Invalid native XML numeric attribute");
            if ((Is(tag, "TexCoords") || Is(tag, "Rect")) && std::fabs(number) > 10000)
                throw std::invalid_argument("Native texture coordinate exceeds original range");
        }
    }
    for (auto& name : InheritedNames(node)) {
        if (Is(tag, "FontString") && CSimpleFont::GetFont(name.c_str(), 0)) continue;
        auto* item = templates.Ptr(name.c_str());
        if (!item || item->m_locked || !Is(item->m_node->m_name.m_str, tag))
            throw std::invalid_argument("Missing, locked or incompatible XML template");
        Validate(item->m_node, path, visits);
    }
    for (auto* child = node->m_child; child; child = child->m_next) Validate(child, path, visits);
    path.pop_back();
}
const char* ParentName(XMLNode* node, size_t depth = 0) {
    if (depth >= 64) throw std::invalid_argument("Excessive inherited parent lookup");
    if (auto* value = node->GetAttributeByName("parent")) return value;
    // Original lookup overwrites parentName for each sibling template. Continue
    // down the last inherited template's chain when it has no explicit parent.
    XMLNode* last = nullptr;
    const char* parent = nullptr;
    for (auto& name : Names(node->GetAttributeByName("inherits"))) {
        auto* item = templates.Ptr(name.c_str());
        if (!item) throw std::invalid_argument("Missing parent template");
        last = item->m_node;
        parent = last->GetAttributeByName("parent");
    }
    return parent ? parent : last ? ParentName(last, depth + 1) : nullptr;
}
int CreateFrameLua(lua_State* L) {
    // No Lua longjmp surrounds XMLNode/C++ owners. Validate and unwind first.
    bool failed = false;
    try {
        FrameXML::Native::RequireFactories();
        Frame::Native::RequireRoot(); Frame::Native::RequireTreeMutation();
        if (!lua_isstring(L, 1) || (!lua_isnoneornil(L, 3) && !lua_istable(L, 3)))
            throw std::invalid_argument("Invalid CreateFrame arguments");
        CSimpleFrame* parent = nullptr;
        if (lua_istable(L, 3)) {
            parent = dynamic_cast<CSimpleFrame*>(FrameScript::Native::GetObject(L, 3));
            if (!parent) throw std::invalid_argument("Invalid canonical Frame parent");
            Frame::Native::RequireSimpleFrame(parent);
        }
        XMLNode node(nullptr, lua_tostring(L, 1));
        if (const char* name = lua_tostring(L, 2)) node.SetAttribute("name", name);
        if (parent) node.SetAttribute("parent", "");
        if (const char* inherits = lua_tostring(L, 4)) node.SetAttribute("inherits", inherits);
        if (const char* id = lua_tostring(L, 5)) node.SetAttribute("id", id);
        CStatus status;
        std::unique_ptr<CSimpleFrame> frame(FrameXML_CreateFrame(&node, parent, &status));
        if (!frame) throw std::invalid_argument("Unknown native Frame type");
        lua_rawgeti(L, LUA_REGISTRYINDEX, frame->GetLuaObjectRef());
        frame.release(); // Parent/root owns the object, matching original CreateFrame.
        return 1;
    } catch (const std::exception&) { failed = true; }
    if (failed) return luaL_error(L, "CreateFrame rejected unavailable services, invalid state or XML arguments");
    return 0;
}
}
namespace FrameXML { namespace Native {
void RequireFactories() {
    Frame::Native::RequireRegionScripts();
    if (!installed) throw std::logic_error("Native FrameXML factories are not installed");
}
void InstallFactories() {
    Frame::Native::RequireRegionScripts();
    if (installed) throw std::logic_error("Native FrameXML factories already installed");
    installed = true;
    try {
        FrameXML_RegisterDefault();
        RequireTableWrite(FrameScript_GetContext(), LUA_GLOBALSINDEX, "CreateFrame");
        FrameScript_RegisterFunction("CreateFrame", &CreateFrameLua);
    } catch (...) {
        factories.InternalClear(0);
        factories.Destroy();
        installed = false;
        throw;
    }
}
void CloseFactories() {
    RequireFactories();
    FrameXML_FreeHashNodes(); FrameXML_ClearFactories();
    templates.Destroy(); factories.Destroy();
    FrameScript_UnregisterFunction("CreateFrame");
    installed = false;
}
void ValidateCreation(XMLNode* node) {
    RequireFactories(); Frame::Native::RequireTreeMutation();
    std::vector<XMLNode*> path;
    size_t visits = 0;
    Validate(node, path, visits);
}
void RequireAvailableName(CScriptObject* object, const char* name) {
    RequireFactories();
    if (!name || !*name) return;
    Name(name);
    char fullName[1024];
    object->CreateName(name, fullName, sizeof(fullName));
    lua_State* L = FrameScript_GetContext();
    lua_pushstring(L, fullName); lua_rawget(L, LUA_GLOBALSINDEX);
    const bool occupied = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (occupied) throw std::invalid_argument("Native XML object name already exists");
    RequireTableWrite(L, LUA_GLOBALSINDEX, fullName);
}
void RequireTableWrite(lua_State* L, int index, const char* key) {
    RequireFactories();
    // A user __newindex can longjmp through XML owners or reenter construction.
    // Admit ordinary writes and existing raw keys, before any assignment runs.
    const int top = lua_gettop(L);
    if (index < 0 && index > LUA_REGISTRYINDEX) index = top + index + 1;
    lua_pushstring(L, key); lua_rawget(L, index);
    bool callback = false;
    if (lua_isnil(L, -1) && lua_getmetatable(L, index)) {
        lua_pushliteral(L, "__newindex"); lua_rawget(L, -2);
        callback = !lua_isnil(L, -1);
    }
    lua_settop(L, top);
    if (callback) throw std::logic_error("Native XML assignment metamethods are unavailable");
}
void ApplyTemplates(XMLNode* node, const std::function<void(XMLNode*)>& apply, bool applyTaint) {
    RequireFactories();
    for (auto& name : InheritedNames(node)) {
        TemplateScope scope(name.c_str(), applyTaint);
        apply(scope.node);
    }
}
CSimpleFontString* CreateFontString(CSimpleFrame* parent, const char* name, unsigned layer, const char* inherits) {
    RequireFactories();
    XMLNode node(nullptr, "FontString");
    if (name && *name) node.SetAttribute("name", name);
    if (inherits && *inherits) node.SetAttribute("inherits", inherits);
    ValidateCreation(&node);
    std::unique_ptr<CSimpleFontString> string(Frame::Native::CreateFontString(parent, layer, true));
    RequireAvailableName(string.get(), name);
    if (name && *name) string->SetName(name);
    if (inherits && *inherits) {
        if (auto* font = CSimpleFont::GetFont(inherits, 0)) string->SetFontObject(font);
        else ApplyTemplates(&node, [&](XMLNode* inherited) {
            CStatus status;
            string->LoadXML(inherited, &status);
            string->PostLoadXML(inherited, &status);
        }, false);
    }
    string->GetLuaObjectRef();
    return string.release();
}
CSimpleTexture* CreateTexture(CSimpleFrame* parent, const char* name, unsigned layer, const char* inherits) {
    RequireFactories();
    XMLNode node(nullptr, "Texture");
    if (name && *name) node.SetAttribute("name", name);
    if (inherits && *inherits) node.SetAttribute("inherits", inherits);
    ValidateCreation(&node);
    std::unique_ptr<CSimpleTexture> texture(Frame::Native::CreateTexture(parent, layer, true));
    RequireAvailableName(texture.get(), name);
    if (name && *name) texture->SetName(name);
    if (inherits && *inherits) {
        ApplyTemplates(&node, [&](XMLNode* inherited) {
            CStatus status;
            texture->LoadXML(inherited, &status);
            texture->PostLoadXML(inherited, &status);
        }, false);
    }
    texture->GetLuaObjectRef();
    return texture.release();
}
}}
int32_t FrameXML_RegisterFactory(const char* name, CSimpleFrame* (*factory)(CSimpleFrame*), bool unique) {
    FrameXML::Native::RequireFactories(); Name(name);
    if (!Is(name, "Frame") || factory != Create_SimpleFrame || unique)
        throw std::invalid_argument("Native widget factory is not qualified");
    if (factories.Ptr(name)) return 0;
    auto* item = factories.New(name, 0, 0);
    item->m_factory = factory; item->m_unique = unique;
    return 1;
}
int32_t FrameXML_RegisterDefault() { return FrameXML_RegisterFactory("Frame", Create_SimpleFrame, false); }
void FrameXML_ClearFactories() { FrameXML::Native::RequireFactories(); factories.InternalClear(0); }
XMLNode* FrameXML_AcquireHashNode(const char* name, const char*& taint, bool& locked) {
    FrameXML::Native::RequireFactories(); Name(name);
    taint = nullptr; locked = false;
    auto* item = templates.Ptr(name);
    if (!item) return nullptr;
    taint = item->m_taint; locked = item->m_locked; item->m_locked = true;
    return item->m_node;
}
void FrameXML_ReleaseHashNode(const char* name) {
    FrameXML::Native::RequireFactories();
    auto* item = templates.Ptr(name);
    if (!item || !item->m_locked) throw std::logic_error("Native XML template release without acquisition");
    item->m_locked = false;
}
void FrameXML_StoreHashNode(XMLNode* node, const char* name, const char* taint, CStatus* status) {
    FrameXML::Native::RequireFactories(); Frame::Native::RequireTreeMutation(); Name(name);
    if (!node || !node->m_parent || !status)
        throw std::invalid_argument("Transfer a tree-owned native template with status");
    auto* item = templates.Ptr(name);
    if (item) {
        if (item->m_locked) throw std::logic_error("Cannot replace a borrowed XML template");
        if (!item->m_taint || taint) { status->Add(STATUS_WARNING, "Virtual object named %s already exists", name); return; }
        delete item->m_node;
    } else {
        if (templateCount >= 4096) throw std::length_error("Native XML template registry limit");
        item = templates.New(name, 0, 0);
        ++templateCount;
    }
    item->m_node = node; item->m_taint = taint; item->m_locked = false;
    node->Detach();
}
void FrameXML_FreeHashNodes() {
    FrameXML::Native::RequireFactories();
    for (auto* item = templates.Head(); item; item = templates.Next(item))
        if (item->m_locked) throw std::logic_error("Release XML template borrows before cleanup");
    for (auto* item = templates.Head(); item; item = templates.Next(item)) delete item->m_node;
    templates.InternalClear(0);
    templateCount = 0;
}
CSimpleFrame* FrameXML_CreateFrame(XMLNode* node, CSimpleFrame* parent, CStatus* status) {
    FrameXML::Native::ValidateCreation(node);
    Frame::Native::RequireRoot();
    if (!status) throw std::invalid_argument("Native XML creation requires status");
    auto* factory = factories.Ptr(node->m_name.m_str);
    if (!factory) { status->Add(STATUS_WARNING, "Unknown frame type: %s", node->m_name.m_str); return nullptr; }
    if (auto* name = ParentName(node); name && *name) {
        parent = static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName(name, CSimpleFrame::GetObjectType()));
        if (!parent) throw std::invalid_argument("Named native Frame parent was not found");
    }
    if (parent) Frame::Native::RequireSimpleFrame(parent);
    CreationScope creation;
    std::unique_ptr<CSimpleFrame> frame(factory->m_factory(parent));
    if (!frame) throw std::bad_alloc();
    created.push_back(frame.get());
    frame->PreLoadXML(node, status);
    frame->LoadXML(node, status);
    frame->PostLoadXML(node, status);
    creation.complete = true;
    return frame.release();
}
