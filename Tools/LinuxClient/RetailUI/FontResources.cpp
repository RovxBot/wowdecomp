// Product-authored 70862 FontFamily subset; no exported implementation/assets.
#include "Engine/Source/Frame/RetailUI/FontResources.h"
#include "Engine/Source/FrameScript/RetailUI/LuaCompat.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>

namespace RovRetailUI {
static bool named(XMLNode* node, const char* name) { return !std::strcmp(node->m_name.m_str, name); }
static bool attributes(XMLNode* node, std::initializer_list<const char*> allowed) {
    if (node->m_body) for (const char* p = node->m_body; *p; ++p)
        if (*p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') return false;
    for (uint32_t i = 0; i < node->m_attributes.Count(); ++i) {
        bool found = false;
        for (const char* name : allowed) found |= !std::strcmp(node->m_attributes[i].m_name.m_str, name);
        if (!found) return false;
    }
    return true;
}
static bool number(const char* text, double low, double high) {
    if (!text || !*text) return false;
    char* end;
    const double value = std::strtod(text, &end);
    return end != text && !*end && std::isfinite(value) && value >= low && value <= high;
}
static bool color(XMLNode* node) {
    if (!attributes(node, {"r", "g", "b", "a"}) || node->m_child) return false;
    for (const char* key : {"r", "g", "b", "a"})
        if (const char* value = node->GetAttributeByName(key)) if (!number(value, 0, 1)) return false;
    return true;
}
static XMLNode* append(XMLNode* parent, const char* name) {
    auto* node = new XMLNode(parent, name);
    XMLNode** tail = &parent->m_child;
    while (*tail) tail = &(*tail)->m_next;
    *tail = node;
    return node;
}
static XMLNode* clone(XMLNode* source, XMLNode* parent) {
    XMLNode* result = append(parent, source->m_name.m_str);
    for (uint32_t i = 0; i < source->m_attributes.Count(); ++i)
        result->SetAttribute(source->m_attributes[i].m_name.m_str, source->m_attributes[i].m_value.m_str);
    for (XMLNode* child = source->m_child; child; child = child->m_next) clone(child, result);
    return result;
}
bool LoadFontFamily(XMLNode* node, uint32_t locale, CStatus* status) {
    auto fail = [&](const char* reason) {
        status->Add(STATUS_ERROR, "Forever FontFamily %s: %s", node->GetAttributeByName("name") ? node->GetAttributeByName("name") : "<unnamed>", reason);
        return false;
    };
    if (!Enabled()) return fail("compatibility version is not enabled");
    if (!named(node, "FontFamily") || !attributes(node, {"name", "virtual"})) return fail("unsupported family schema");
    const char* name = node->GetAttributeByName("name");
    if (!name || !*name || std::strlen(name) > 255) return fail("invalid resource name");
    const char* virtualValue = node->GetAttributeByName("virtual");
    if (virtualValue && std::strcmp(virtualValue, "true") && std::strcmp(virtualValue, "false")) return fail("invalid virtual value");
    const char* alphabet = nullptr;
    // Existing client locale tags, independent of host byte order.
    switch (locale) {
    case 0x656e5553: case 0x656e4742: case 0x66724652: case 0x64654445: case 0x65734553: case 0x65734d58: alphabet = "roman"; break;
    case 0x6b6f4b52: alphabet = "korean"; break;
    case 0x7a68434e: alphabet = "simplifiedchinese"; break;
    case 0x7a685457: alphabet = "traditionalchinese"; break;
    case 0x72755255: alphabet = "russian"; break;
    default: return fail("unsupported locale");
    }
    XMLNode* selected = nullptr;
    const char* known[] = {"roman", "korean", "simplifiedchinese", "traditionalchinese", "russian"};
    bool seen[5] = {};
    for (XMLNode* member = node->m_child; member; member = member->m_next) {
        if (!named(member, "Member") || !attributes(member, {"alphabet"})) return fail("unsupported member schema");
        const char* value = member->GetAttributeByName("alphabet");
        int index = -1;
        for (int i = 0; i < 5; ++i) if (value && !std::strcmp(value, known[i])) index = i;
        if (index < 0 || seen[index]) return fail("unknown or duplicate alphabet");
        seen[index] = true;
        if (!member->m_child || member->m_child->m_next || !named(member->m_child, "Font")) return fail("Member requires exactly one Font");
        if (!std::strcmp(value, alphabet)) selected = member->m_child;
    }
    if (!selected) return fail("locale has no member; no fallback is inferred");
    if (!attributes(selected, {"font", "height", "outline", "monochrome", "spacing", "inherits", "slug"})) return fail("unsupported selected Font attributes");
    const char* file = selected->GetAttributeByName("font");
    const char* height = selected->GetAttributeByName("height");
    const char* inherits = selected->GetAttributeByName("inherits");
    if ((!file || !*file) && (!inherits || !*inherits)) return fail("font file or inheritance required");
    if (file && (!*file || std::strlen(file) > 255 || !number(height, 0.01, 256))) return fail("invalid font path or height");
    if (!file && (height || selected->GetAttributeByName("outline") || selected->GetAttributeByName("monochrome") || selected->GetAttributeByName("slug")))
        return fail("inherited height/outline overrides require a selected font file");
    if (file && CSimpleFont::GetFont(file, 0)) return fail("font path conflicts with a named font; explicit inheritance required");
    if (const char* outline = selected->GetAttributeByName("outline"))
        if (std::strcmp(outline, "NONE") && std::strcmp(outline, "NORMAL") && std::strcmp(outline, "THICK")) return fail("unsupported outline");
    if (const char* mono = selected->GetAttributeByName("monochrome"))
        if (std::strcmp(mono, "true") && std::strcmp(mono, "false")) return fail("invalid monochrome");
    if (const char* slug = selected->GetAttributeByName("slug")) {
        if (std::strcmp(slug, "true") && std::strcmp(slug, "false")) return fail("invalid slug value");
        const char* mono = selected->GetAttributeByName("monochrome");
        if (!std::strcmp(slug, "true") && mono && !std::strcmp(mono, "true")) return fail("slug monochrome combination is unqualified");
    }
    if (const char* spacing = selected->GetAttributeByName("spacing"))
        if (!number(spacing, -256, 256)) return fail("invalid spacing");
    CSimpleFont* existing = CSimpleFont::GetFont(name, 0);
    lua_State* L = FrameScript_GetContext();
    lua_pushstring(L, name); lua_rawget(L, LUA_GLOBALSINDEX);
    bool owned = lua_isnil(L, -1) && !existing;
    if (existing) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, existing->GetLuaObjectRef());
        owned = lua_rawequal(L, -1, -2);
        lua_rawgeti(L, -1, 0);
        owned = owned && lua_islightuserdata(L, -1) && lua_touserdata(L, -1) == existing;
        lua_pop(L, 2);
    }
    lua_pop(L, 1);
    if (!owned) return fail("global resource name belongs to another object");
    if (inherits) {
        CSimpleFont* parent = CSimpleFont::GetFont(inherits, 0);
        if (!parent) return fail("missing inherited font");
        unsigned depth = 0;
        for (; parent; parent = parent->m_fontObject) if (++depth > 64 || parent == existing) return fail("cyclic or excessive inheritance");
    }
    bool shadowSeen = false, colorSeen = false;
    for (XMLNode* child = selected->m_child; child; child = child->m_next) {
        if (named(child, "Color")) {
            if (colorSeen || !color(child)) return fail("invalid Color");
            colorSeen = true;
        } else if (named(child, "Shadow")) {
            if (shadowSeen || !attributes(child, {"x", "y"})) return fail("invalid Shadow");
            shadowSeen = true;
            const char* x = child->GetAttributeByName("x");
            const char* y = child->GetAttributeByName("y");
            if ((x || y) && (!number(x, -256, 256) || !number(y, -256, 256))) return fail("invalid shadow coordinates");
            bool offset = false, tint = false;
            for (XMLNode* item = child->m_child; item; item = item->m_next) {
                if (named(item, "Color") && !tint && color(item)) tint = true;
                else if (named(item, "Offset") && !offset && !x && !y && attributes(item, {"x", "y"}) && !item->m_child &&
                         number(item->GetAttributeByName("x"), -256, 256) && number(item->GetAttributeByName("y"), -256, 256)) offset = true;
                else return fail("unsupported Shadow child");
            }
            if (!tint) return fail("Shadow requires an explicit Color in this subset");
        } else return fail("unsupported selected Font child");
    }
    // Normalize documented scalar syntax into the real 12340 loader, preserving
    // its dimensions, font handles, inheritance, globals and consumer updates.
    XMLNode normalized(nullptr, "Font");
    for (uint32_t i = 0; i < selected->m_attributes.Count(); ++i) {
        const char* key = selected->m_attributes[i].m_name.m_str;
        if (std::strcmp(key, "height")) normalized.SetAttribute(key, selected->m_attributes[i].m_value.m_str);
    }
    // Explicit private namespace resolves the supplied installed font bytes.
    // It cannot override a signed original font/archive lookup.
    if (file && std::getenv("WOWDECOMP_FOREVER_ASSETS")) {
        if (std::strncmp(file, "Fonts\\", 6) && std::strncmp(file, "Fonts/", 6)) return fail("external resource requires a Fonts path");
        normalized.SetAttribute("font", (std::string("RovForeverFonts\\") + file).c_str());
    }
    if (height) { auto* h = append(&normalized, "FontHeight"); h->SetAttribute("val", height); }
    for (XMLNode* child = selected->m_child; child; child = child->m_next) {
        auto* copy = clone(child, &normalized);
        if (named(copy, "Shadow") && child->GetAttributeByName("x")) {
            auto* offset = append(copy, "Offset");
            offset->SetAttribute("x", child->GetAttributeByName("x")); offset->SetAttribute("y", child->GetAttributeByName("y"));
        }
    }
    CStatus stagedStatus;
    CSimpleFont staged;
    staged.LoadXML(&normalized, &stagedStatus);
    if (stagedStatus.m_highestSeverity >= STATUS_WARNING || !staged.m_attributes.m_font.m_str) {
        status->Add(stagedStatus);
        return fail("font construction failed; resource was not published");
    }
    CSimpleFont* font = CSimpleFont::GetFont(name, 1);
    // Assignment alone does not notify fontables; use the original XML path for
    // replacement too. Preflight has already validated every required resource.
    font->LoadXML(&normalized, status);
    return true;
}
}
