// Native ownership/admission around original 12340 backdrop Lua fields. No new
// address verification. Raw table access prevents assignment hooks reentering
// partially constructed region ownership; Windows retains its original scripts.
#include "Backdrops.h"
#include "Scripts.h"
#include "Engine/Source/Frame/CBackdropGenerator.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Base/Coordinate.h"
#include <memory>
#include <cstring>
#include "Engine/Source/FrameXML/LoadXML.h"

namespace Frame { namespace Native {
namespace {
CSimpleFrame* This(lua_State* L) {
    return static_cast<CSimpleFrame*>(FrameScript_GetObjectThis(L, CSimpleFrame::GetObjectType()));
}
void Field(lua_State* L, int table, const char* key) {
    lua_pushstring(L, key); lua_rawget(L, table);
}
float Dimension(lua_State* L, int table, const char* key, float fallback) {
    Field(L, table, key);
    float value = fallback;
    if (lua_isnumber(L, -1)) {
        const double number = lua_tonumber(L, -1);
        const double scaled = number / (CoordinateGetAspectCompensation() * 1024.0f);
        if (!std::isfinite(number) || std::fabs(number) > FLT_MAX ||
            !std::isfinite(scaled) || std::fabs(scaled) > FLT_MAX)
            throw std::invalid_argument("Invalid native backdrop dimension");
        value = NDCToDDCWidth(float(scaled));
        if (!std::isfinite(value)) throw std::invalid_argument("Invalid native backdrop dimension");
    }
    lua_pop(L, 1);
    return value;
}
const char* Filename(lua_State* L, const char* key) {
    Field(L, 2, key);
    if (!lua_isnil(L, -1) && lua_type(L, -1) != LUA_TSTRING)
        throw std::invalid_argument("Backdrop image must be a filename");
    size_t length = 0;
    const char* value = lua_tolstring(L, -1, &length);
    if (value && (length >= 260 || std::strlen(value) != length))
        throw std::invalid_argument("Invalid native backdrop filename");
    lua_pop(L, 1);
    return value; // Still rooted in the raw source table until this call returns.
}
void Number(lua_State* L, int table, const char* key, float value) {
    lua_pushstring(L, key); lua_pushnumber(L, value); lua_rawset(L, table);
}
void String(lua_State* L, int table, const char* key, const char* value) {
    lua_pushstring(L, key); lua_pushstring(L, value ? value : ""); lua_rawset(L, table);
}
float Pixels(float value) { return DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * value); }
int Color(lua_State* L, bool border) {
    auto* backdrop = This(L)->m_backdropGenerator;
    if (!backdrop) return 0;
    const auto color = border ? backdrop->m_borderColor : backdrop->m_backgroundColor;
    lua_pushnumber(L, color.r * 0.00392156f); lua_pushnumber(L, color.g * 0.00392156f);
    lua_pushnumber(L, color.b * 0.00392156f); lua_pushnumber(L, color.a * 0.00392156f);
    return 4;
}
int SetColor(lua_State* L, bool border) {
    auto* backdrop = This(L)->m_backdropGenerator;
    CImVector color;
    FrameScript_GetColor(L, 2, color);
    if (backdrop) {
        if (border) backdrop->SetBorderVertexColor(color);
        else backdrop->SetVertexColor(color);
    }
    return 0;
}
}
int GetBackdrop(lua_State* L) {
    auto* backdrop = This(L)->m_backdropGenerator;
    if (!backdrop) return 0;
    lua_settop(L, 2);
    if (!lua_istable(L, 2)) { lua_pop(L, 1); lua_newtable(L); }
    String(L, 2, "bgFile", backdrop->m_bgFile.m_str);
    String(L, 2, "edgeFile", backdrop->m_edgeFile.m_str);
    lua_pushstring(L, "tile");
    if (backdrop->m_tile) lua_pushnumber(L, 1); else lua_pushnil(L);
    lua_rawset(L, 2);
    Number(L, 2, "tileSize", Pixels(backdrop->m_tileSize));
    Number(L, 2, "edgeSize", Pixels(backdrop->m_edgeSize));
    Field(L, 2, "insets");
    if (!lua_istable(L, 3)) { lua_pop(L, 1); lua_newtable(L); }
    Number(L, 3, "left", Pixels(backdrop->m_insetLeft));
    Number(L, 3, "right", Pixels(backdrop->m_insetRight));
    Number(L, 3, "top", Pixels(backdrop->m_insetTop));
    Number(L, 3, "bottom", Pixels(backdrop->m_insetBottom));
    lua_pushstring(L, "insets"); lua_pushvalue(L, 3); lua_rawset(L, 2);
    lua_pop(L, 1);
    return 1;
}
int SetBackdrop(lua_State* L) {
    auto* frame = This(L);
    if (lua_isnil(L, 2)) { frame->SetBackdrop(nullptr); return 0; }
    if (!lua_istable(L, 2)) throw std::invalid_argument("Backdrop requires nil or a table");
    const char* bg = Filename(L, "bgFile");
    const char* edge = Filename(L, "edgeFile");
    Field(L, 2, "tile"); const bool tile = StringToBOOL(L, -1, 0); lua_pop(L, 1);
    const float tileSize = Dimension(L, 2, "tileSize", 0);
    const float edgeSize = Dimension(L, 2, "edgeSize", 0.025f);
    float left = 0, right = 0, top = 0, bottom = 0;
    Field(L, 2, "insets");
    const int insets = lua_gettop(L);
    if (lua_istable(L, insets)) {
        left = Dimension(L, insets, "left", 0); right = Dimension(L, insets, "right", 0);
        top = Dimension(L, insets, "top", 0); bottom = Dimension(L, insets, "bottom", 0);
    }
    lua_pop(L, 1);
    auto backdrop = std::make_unique<CBackdropGenerator>();
    backdrop->m_bgFile.Copy(bg); backdrop->m_edgeFile.Copy(edge);
    backdrop->m_tile = tile; backdrop->m_pieces = edge && *edge ? CBackdropGenerator::THEWORKS : 0;
    backdrop->m_tileSize = tileSize; backdrop->m_edgeSize = edgeSize;
    backdrop->m_insetLeft = left; backdrop->m_insetRight = right;
    backdrop->m_insetTop = top; backdrop->m_insetBottom = bottom;
    frame->SetBackdrop(backdrop.get());
    backdrop.release();
    return 0;
}
int GetBackdropColor(lua_State* L) { return Color(L, false); }
int SetBackdropColor(lua_State* L) { return SetColor(L, false); }
int GetBackdropBorderColor(lua_State* L) { return Color(L, true); }
int SetBackdropBorderColor(lua_State* L) { return SetColor(L, true); }
}}
