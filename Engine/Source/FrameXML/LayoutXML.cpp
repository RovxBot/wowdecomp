// Original 12340 LoadXML.cpp bodies, separated for explicit native selection.
// Historical OFFSET/VERIFY tags are retained; this is not a new binary claim.
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Storm/H/storm.h"
#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Runtime.h"
#endif

// STATIC: 0x00A43F78
static const XMLENUMVALUE s_framePoints[] = {
    { FRAMEPOINT_BOTTOM, "BOTTOM" },
    { FRAMEPOINT_BOTTOMLEFT, "BOTTOMLEFT" },
    { FRAMEPOINT_BOTTOMRIGHT, "BOTTOMRIGHT" },
    { FRAMEPOINT_CENTER, "CENTER" },
    { FRAMEPOINT_TOP, "TOP" },
    { FRAMEPOINT_TOPRIGHT, "TOPRIGHT" },
    { FRAMEPOINT_TOPLEFT, "TOPLEFT" },
    { FRAMEPOINT_LEFT, "LEFT" },
    { FRAMEPOINT_RIGHT, "RIGHT" },
};

// VERIFY: 96ee6114 auto
// OFFSET: 0x00814ED0
int32_t StringToFramePoint(const char* string, FRAMEPOINT& point) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireOwner();
#endif
    for (uint32_t i = 0; i < sizeof(s_framePoints) / sizeof(s_framePoints[0]); i++) {
        if (!SStrCmpI(s_framePoints[i].m_name, string, STORM_MAX_STR)) {
            point = static_cast<FRAMEPOINT>(s_framePoints[i].m_value);
            return 1;
        }
    }

    return 0;
}

// VERIFY: 305511b6 auto
// OFFSET: 0x00814F20
const char* FramePointToString(FRAMEPOINT point) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireOwner();
#endif
    for (uint32_t i = 0; i < sizeof(s_framePoints) / sizeof(s_framePoints[0]); i++) {
        if (point == s_framePoints[i].m_value) {
            return s_framePoints[i].m_name;
        }
    }

    return "UNKNOWN";
}

// VERIFY: 4b656605 auto
// OFFSET: 0x00815740
int32_t LoadXML_Dimensions(XMLNode* node, float& x, float& y, CStatus* status) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireOwner();
#endif
    x = 0.0f;
    y = 0.0f;

    const char* xString = node->GetAttributeByName("x");

    if (xString && *xString) {
        float ndc = SStrToFloat(xString) / (CoordinateGetAspectCompensation() * 1024.0f);
        x = NDCToDDCWidth(ndc);
    }

    const char* yString = node->GetAttributeByName("y");

    if (yString && *yString) {
        float ndc = SStrToFloat(yString) / (CoordinateGetAspectCompensation() * 1024.0f);
        y = NDCToDDCWidth(ndc);
    }

    XMLNode* child = node->m_child;

    if (!child) {
        return yString != nullptr;
    }

    if (!SStrCmpI(child->m_name.m_str, "AbsDimension", STORM_MAX_STR)) {
        const char* absX = child->GetAttributeByName("x");

        if (absX && *absX) {
            float ndc = SStrToFloat(absX) / (CoordinateGetAspectCompensation() * 1024.0f);
            x = NDCToDDCWidth(ndc);
        }

        const char* absY = child->GetAttributeByName("y");

        if (absY && *absY) {
            float ndc = SStrToFloat(absY) / (CoordinateGetAspectCompensation() * 1024.0f);
            y = NDCToDDCWidth(ndc);
        }

        return 1;
    }

    if (!SStrCmpI(child->m_name.m_str, "RelDimension", STORM_MAX_STR)) {
        const char* relX = child->GetAttributeByName("x");

        if (relX && *relX) {
            x = SStrToFloat(relX);
        }

        const char* relY = child->GetAttributeByName("y");

        if (relY && *relY) {
            y = SStrToFloat(relY);
        }

        return 1;
    }

    status->Add(STATUS_WARNING, "Unknown child node in %s element: %s", node->m_name.m_str, child->m_name.m_str);

    return 0;
}

