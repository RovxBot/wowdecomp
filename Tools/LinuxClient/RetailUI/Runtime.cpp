#include "Engine/Source/Frame/RetailUI/Runtime.h"
#include "Engine/Source/FrameScript/RetailUI/LuaCompat.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <algorithm>

namespace RovRetailUI {
void BootstrapRuntime(bool gameplay) {
    const char* root = std::getenv("WOWDECOMP_FOREVER_ASSETS");
    if (!Enabled() || !root || !*root) return;
    const char* stage = std::getenv("WOWDECOMP_FOREVER_STAGE");
    if ((stage && !std::strcmp(stage, "glue")) == gameplay) return;
    std::ifstream plan(std::string(root) + "/../forever-runtime-plan.txt");
    std::string line;
    if (!std::getline(plan, line) || line != "# forever-70862 207a69936fcc6186deceaf63be07066c45b278339b2a4e266b1f1a1985cbeb74") {
        std::fprintf(stderr, "forever-runtime: missing verified ../forever-runtime-plan.txt; run audit_reference.py --runtime-plan beside the external bundle\n");
        return;
    }
    lua_State* L = FrameScript_GetContext();
    const int top = lua_gettop(L);
    // Retain the working client's handler. A partially loaded modern bootstrap
    // must not leave the legacy session using its unqualified error handler.
    lua_rawgeti(L, LUA_REGISTRYINDEX, s_errorHandlerRef);
    const int savedHandler = luaL_ref(L, LUA_REGISTRYINDEX);
    bool loaded = true;
    unsigned count = 0;
    while (std::getline(plan, line)) {
        if (line.size() > 230 || line.rfind("Interface/AddOns/", 0) != 0 || line.find("..") != std::string::npos || line.find('\\') != std::string::npos) {
            loaded = false; std::fprintf(stderr, "forever-runtime: invalid declared plan entry\n"); break;
        }
        std::string path = "RovForeverUI/" + line;
        std::replace(path.begin(), path.end(), '/', '\\');
        CStatus status;
        const bool ok = FrameXML_ProcessFileNoHash(path.c_str(), nullptr, &status) && status.m_highestSeverity < STATUS_WARNING;
        // Keep native logging intact while the modern error handler is pending
        // #217 qualification; no successful error service is substituted.
        luaL_unref(L, LUA_REGISTRYINDEX, s_errorHandlerRef);
        lua_rawgeti(L, LUA_REGISTRYINDEX, savedHandler);
        s_errorHandlerRef = luaL_ref(L, LUA_REGISTRYINDEX);
        if (!ok) {
            char errors[8192];
            std::fprintf(stderr, "forever-runtime-stop: %s: %s\n", line.c_str(), status.GetErrorStr(errors, sizeof(errors), STATUS_WARNING, "\n"));
            loaded = false; break;
        }
        ++count;
        std::fprintf(stderr, "forever-runtime-loaded: %s\n", line.c_str());
    }
    luaL_unref(L, LUA_REGISTRYINDEX, savedHandler);
    std::fprintf(stderr, "forever-runtime: stage=%s completed-files=%u complete=%d (Edit Mode gameplay acceptance remains open)\n", gameplay ? "game" : "glue", count, loaded);
    lua_settop(L, top);
    // Explicit developer visual evidence: real FontStrings consume the genuine
    // selected resource on the existing UI root. This is a font specimen, never
    // advertised as a genuine Edit Mode control or action-bar adapter.
    const char* preview = std::getenv("WOWDECOMP_FOREVER_PREVIEW");
    if (preview && !std::strcmp(preview, "1")) {
        const char* source = R"lua(
            local root = UIParent or GlueParent
            if root and SystemFont_Outline_Slug_Large2 then
                for i, scale in ipairs({0.75, 1, 2}) do
                    local label = root:CreateFontString(nil, "OVERLAY")
                    label:SetFontObject(SystemFont_Outline_Slug_Large2)
                    label:SetPoint("TOP", root, "TOP", 0, -20 - (i - 1) * 45)
                    label:SetScale(scale)
                    label:SetText("Forever 70862: native Slug AaBb 012345 @&")
                end
            end
        )lua";
        if (luaL_loadstring(L, source) || lua_pcall(L, 0, 0, 0)) {
            std::fprintf(stderr, "forever-runtime-preview: %s\n", lua_tostring(L, -1)); lua_pop(L, 1);
        }
    }
}
}
