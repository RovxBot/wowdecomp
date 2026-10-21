// Native lifetime boundary for the original 12340 Frame font factories.
#include "Fonts.h"
#include "Engine/Source/Frame/SimpleFrameScript.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/Gxu/Portable/LegacyFonts.h"
#include <stdexcept>
#include <unordered_set>
namespace Frame { namespace Native {
namespace {
bool installed = false;
std::unordered_set<CSimpleFont*> fonts;
std::unordered_set<CSimpleFontable*> fontables;
std::unordered_set<CScriptObject*> scripts;
constexpr size_t MaxFonts = 512, MaxDepth = 64;
size_t updates = 0;
}
Fonts::Fonts() {
    Gxu::Native::RequireFontOwner();
    if (installed) throw std::logic_error("Only one Frame font factory owner may be installed");
    installed = true;
    try { CSimpleFont::CreateScriptMetaTable(); FrameScript_RegisterFunction("CreateFont", &SimpleFrameScript_CreateFont); }
    catch (...) { installed = false; throw; }
}
void RequireFontFactories() {
    RequireOwner();
    Gxu::Native::RequireFontOwner();
    if (!installed) throw std::logic_error("Native Frame font factories are not installed");
}
void Fonts::CheckCanClose() const {
    RequireFontFactories();
    if (!scripts.empty() || fontables.size() != fonts.size() || updates)
        throw std::logic_error("Close external Frame fontables/script objects before font factories");
    for (auto* font : fonts)
        if (!font->m_name) throw std::logic_error("Close standalone Frame fonts before the factory owner");
}
Fonts::~Fonts() {
    try {
        CheckCanClose();
        CSimpleFont::Destroy();
        if (!fonts.empty() || !fontables.empty())
            throw std::logic_error("Close standalone Frame fonts before the factory owner");
        FrameScript_UnregisterFunction("CreateFont");
        CSimpleFont::DestroyScriptMetaTable();
        std::unordered_set<CSimpleFont*>().swap(fonts);
        std::unordered_set<CSimpleFontable*>().swap(fontables);
        std::unordered_set<CScriptObject*>().swap(scripts);
        installed = false;
    } catch (...) { std::terminate(); }
}
void RequireFontName(const char* name) {
    RequireFontFactories();
    if (!name || !*name) throw std::invalid_argument("Empty Frame font name");
    size_t length = 0;
    while (name[length] && length < 1024) ++length;
    if (length == 1024) throw std::length_error("Frame font name exceeds 1023 bytes");
}
void RequireFontCapacity() {
    RequireFontFactories();
    if (fonts.size() >= MaxFonts) throw std::length_error("Native Frame font limit");
}
void FontCreated(CSimpleFont* font) {
    RequireFontFactories();
    if (fonts.size() >= MaxFonts) throw std::length_error("Native Frame font limit");
    fonts.insert(font);
}
void FontDestroyed(CSimpleFont* font) { RequireFontFactories(); fonts.erase(font); }
void FontableCreated(CSimpleFontable* object) { RequireFontFactories(); fontables.insert(object); }
void FontableDestroyed(CSimpleFontable* object) { RequireFontFactories(); fontables.erase(object); }
void ScriptCreated(CScriptObject* object) { RequireFontFactories(); scripts.insert(object); }
void ScriptDestroyed(CScriptObject* object) { RequireFontFactories(); scripts.erase(object); }
void RequireScript(CScriptObject* object) {
    RequireFontFactories();
    if (!scripts.count(object)) throw std::invalid_argument("Script parent is not live");
}
FontUpdate::FontUpdate() { RequireFontFactories(); ++updates; }
FontUpdate::~FontUpdate() { --updates; }
void RequireFont(CSimpleFont* font) {
    RequireFontFactories();
    if (!fonts.count(font)) throw std::invalid_argument("Frame font is not live");
}
void RequireFontParent(CSimpleFontable* object, CSimpleFont* parent) {
    RequireFontFactories();
    if (!fontables.count(object)) throw std::invalid_argument("Frame fontable is not live");
    if (updates && object->m_fontObject != parent)
        throw std::logic_error("Finish font updates before mutating fontable lists");
    // Validate the proposed edge over all descendants before mutating any list.
    for (auto* node : fonts) {
        size_t depth = 0;
        for (auto* current = node; current;) {
            RequireFont(current);
            if (++depth > MaxDepth) throw std::invalid_argument("Cyclic or excessive font inheritance");
            current = static_cast<CSimpleFontable*>(current) == object ? parent : current->m_fontObject;
        }
    }
    if (parent) RequireFont(parent);
}
}}
