// Original 12340 font/XML scalar helpers, shared by Windows and native builds.
// Historical OFFSET/VERIFY comments do not claim new binary verification.
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Frame/Types.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include <cmath>
#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Runtime.h"
#define FONT_XML_CHECK() Frame::Native::RequireOwner()
#else
#define FONT_XML_CHECK() ((void)0)
#endif

// STATIC: 0x00A44010
static const XMLENUMVALUE s_justify[] = {
    { 0x1, "LEFT" },
    { 0x2, "CENTER" },
    { 0x4, "RIGHT" },
    { 0x8, "TOP" },
    { 0x10, "MIDDLE" },
    { 0x20, "BOTTOM" },
};


// STATIC: 0x00A44050
static const XMLENUMVALUE s_fontFlags[] = {
    { 0x1, "OUTLINE" },
    { 0x4, "THICKOUTLINE" },
    { 0x2, "MONOCHROME" },
};


// STATIC: 0x00D3F730
static char s_fontFlagsString[64];


// VERIFY: 987b1717 auto
// OFFSET: 0x00815080
int32_t StringToJustify(const char* string, uint32_t& justify) {
    FONT_XML_CHECK();
    for (uint32_t i = 0; i < sizeof(s_justify) / sizeof(s_justify[0]); i++) {
        if (!SStrCmpI(s_justify[i].m_name, string, STORM_MAX_STR)) {
            justify = s_justify[i].m_value;
            return 1;
        }
    }

    return 0;
}

// VERIFY: f9e9e7b3 auto
// OFFSET: 0x008150D0
const char* JustifyToString(uint32_t justify) {
    FONT_XML_CHECK();
    for (uint32_t i = 0; i < sizeof(s_justify) / sizeof(s_justify[0]); i++) {
        if (justify & s_justify[i].m_value) {
            return s_justify[i].m_name;
        }
    }

    return "UNKNOWN";
}



// VERIFY: 45fda266 auto
// OFFSET: 0x008151A0
uint32_t StringToFontFlags(const char* string) {
    FONT_XML_CHECK();
    uint32_t flags = 0;

    for (uint32_t i = 0; i < sizeof(s_fontFlags) / sizeof(s_fontFlags[0]); i++) {
        if (SStrStrI(string, s_fontFlags[i].m_name)) {
            flags |= s_fontFlags[i].m_value;
        }
    }

    return flags;
}


// VERIFY: 9e674fdc auto
// OFFSET: 0x008151E0
const char* FontFlagsToString(uint32_t flags) {
    FONT_XML_CHECK();
    s_fontFlagsString[0] = '\0';

    for (uint32_t i = 0; i < sizeof(s_fontFlags) / sizeof(s_fontFlags[0]); i++) {
        if (flags & s_fontFlags[i].m_value) {
            if (s_fontFlagsString[0]) {
                SStrPack(s_fontFlagsString, ", ", sizeof(s_fontFlagsString));
            }

            SStrPack(s_fontFlagsString, s_fontFlags[i].m_name, sizeof(s_fontFlagsString));
        }
    }

    return s_fontFlagsString;
}


// VERIFY: 89581c53 auto
// OFFSET: 0x00815400
static int32_t StringToBOOL(const char* string, int32_t defaultValue) {
    if (!string) {
        return defaultValue;
    }

    switch (*string) {
        case '0':
        case 'F':
        case 'N':
        case 'f':
        case 'n':
            return 0;

        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 'T':
        case 'Y':
        case 't':
        case 'y':
            return 1;
    }

    if (!SStrCmpI(string, "off", STORM_MAX_STR) || !SStrCmpI(string, "disabled", STORM_MAX_STR)) {
        return 0;
    }

    if (!SStrCmpI(string, "on", STORM_MAX_STR) || !SStrCmpI(string, "enabled", STORM_MAX_STR)) {
        return 1;
    }

    return defaultValue;
}

// VERIFY: e0b63df6 auto
// OFFSET: 0x008154E0
int32_t StringToBOOL(const char* string) {
    FONT_XML_CHECK();
    return StringToBOOL(string, 0);
}

// VERIFY: ed82a3ea auto
// OFFSET: 0x00815500
int32_t StringToBOOL(lua_State* L, int32_t index, int32_t defaultValue) {
    FONT_XML_CHECK();
    switch (lua_type(L, index)) {
        case LUA_TNIL:
            return 0;

        case LUA_TBOOLEAN:
            return lua_toboolean(L, index);

        case LUA_TNUMBER:
            #ifdef WOWDECOMP_PORTABLE_RUNTIME
            if (!std::isfinite(lua_tonumber(L, index)) || lua_tonumber(L, index) < -2147483648.0 || lua_tonumber(L, index) >= 2147483648.0)
                return luaL_error(L, "Boolean number outside original integer bounds");
#endif
            return static_cast<int32_t>(lua_tonumber(L, index)) != 0;

        case LUA_TSTRING:
            return StringToBOOL(lua_tolstring(L, index, nullptr), defaultValue);

        default:
            return defaultValue;
    }
}


// VERIFY: 785739a1 auto
// OFFSET: 0x00815600
int32_t LoadXML_Value(XMLNode* node, float& value, CStatus* status) {
    FONT_XML_CHECK();
    value = 0.0f;

    const char* valueString = node->GetAttributeByName("val");

    if (valueString && *valueString) {
        float ndc = SStrToFloat(valueString) / (CoordinateGetAspectCompensation() * 1024.0f);
        value = NDCToDDCWidth(ndc);
    }

    XMLNode* child = node->m_child;

    if (!child) {
        if (!valueString) {
            status->Add(STATUS_WARNING, "No \"val\" attribute in element: %s", node->m_name.m_str);
            return 0;
        }

        return 1;
    }

    if (!SStrCmpI(child->m_name.m_str, "AbsValue", STORM_MAX_STR)) {
        const char* absString = child->GetAttributeByName("val");

        if (absString && *absString) {
            float ndc = SStrToFloat(absString) / (CoordinateGetAspectCompensation() * 1024.0f);
            value = NDCToDDCWidth(ndc);
        }

        return 1;
    }

    if (!SStrCmpI(child->m_name.m_str, "RelValue", STORM_MAX_STR)) {
        const char* relString = child->GetAttributeByName("val");

        if (relString && *relString) {
            value = SStrToFloat(relString);
        }

        return 1;
    }

    status->Add(STATUS_WARNING, "Unknown child node in %s element: %s", node->m_name.m_str, child->m_name.m_str);

    return 0;
}


// VERIFY: e06fe5f5 auto
// OFFSET: 0x00815C30
int32_t LoadXML_Color(XMLNode* node, CImVector& color, CStatus* status) {
    FONT_XML_CHECK();
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    const char* rString = node->GetAttributeByName("r");

    if (rString && *rString) {
        r = (SStrToFloat(rString) < 1.0f && !(SStrToFloat(rString) > 0.0f)) ? 0.0f : (SStrToFloat(rString) >= 1.0f ? 1.0f : SStrToFloat(rString));
    }

    const char* gString = node->GetAttributeByName("g");

    if (gString && *gString) {
        g = (SStrToFloat(gString) < 1.0f && !(SStrToFloat(gString) > 0.0f)) ? 0.0f : (SStrToFloat(gString) >= 1.0f ? 1.0f : SStrToFloat(gString));
    }

    const char* bString = node->GetAttributeByName("b");

    if (bString && *bString) {
        b = (SStrToFloat(bString) < 1.0f && !(SStrToFloat(bString) > 0.0f)) ? 0.0f : (SStrToFloat(bString) >= 1.0f ? 1.0f : SStrToFloat(bString));
    }

    const char* aString = node->GetAttributeByName("a");

    if (aString && *aString) {
        a = (SStrToFloat(aString) < 1.0f && !(SStrToFloat(aString) > 0.0f)) ? 0.0f : (SStrToFloat(aString) >= 1.0f ? 1.0f : SStrToFloat(aString));
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireFinite(a); Frame::Native::RequireFinite(r);
    Frame::Native::RequireFinite(g); Frame::Native::RequireFinite(b);
#endif
    color.Set(a, r, g, b);

    return 1;
}


// STATIC: 0x00A43FC0
static const XMLENUMVALUE s_drawLayers[] = {
    { DRAWLAYER_BACKGROUND, "BACKGROUND" },
    { DRAWLAYER_BACKGROUND_BORDER, "BORDER" },
    { DRAWLAYER_ARTWORK, "ARTWORK" },
    { DRAWLAYER_ARTWORK_OVERLAY, "OVERLAY" },
    { DRAWLAYER_HIGHLIGHT, "HIGHLIGHT" },
};

// VERIFY: 8a25e9ea auto
// OFFSET: 0x00814F60
int32_t StringToDrawLayer(const char* string, uint32_t& layer) {
    for (uint32_t i = 0; i < sizeof(s_drawLayers) / sizeof(s_drawLayers[0]); i++) {
        if (!SStrCmpI(s_drawLayers[i].m_name, string, STORM_MAX_STR)) {
            layer = s_drawLayers[i].m_value;
            return 1;
        }
    }

    return 0;
}

// VERIFY: ab06cece auto
// OFFSET: 0x00814FB0
const char* DrawLayerToString(uint32_t layer) {
    for (uint32_t i = 0; i < sizeof(s_drawLayers) / sizeof(s_drawLayers[0]); i++) {
        if (layer == s_drawLayers[i].m_value) {
            return s_drawLayers[i].m_name;
        }
    }

    return "UNKNOWN";
}

