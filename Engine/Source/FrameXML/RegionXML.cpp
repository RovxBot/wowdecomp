// Original 12340 Frame/region XML helpers, separated for explicit native selection.
// Retained historical tags do not claim new binary verification.
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Storm/H/storm.h"
#include <new>
#include <typeinfo>
#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Hierarchy.h"
#include "Engine/Source/FrameXML/Portable/Factories.h"
#include <memory>
#endif

// STATIC: 0x00A43F38
static const XMLENUMVALUE s_frameStrata[] = {
    { FRAME_STRATA_BACKGROUND, "BACKGROUND" },
    { FRAME_STRATA_LOW, "LOW" },
    { FRAME_STRATA_MEDIUM, "MEDIUM" },
    { FRAME_STRATA_HIGH, "HIGH" },
    { FRAME_STRATA_DIALOG, "DIALOG" },
    { FRAME_STRATA_FULLSCREEN, "FULLSCREEN" },
    { FRAME_STRATA_FULLSCREEN_DIALOG, "FULLSCREEN_DIALOG" },
    { FRAME_STRATA_TOOLTIP, "TOOLTIP" },
};


// VERIFY: d37bc6e0 auto
// OFFSET: 0x00814E40
int32_t StringToFrameStrata(const char* string, FRAME_STRATA& strata) {
    for (uint32_t i = 0; i < sizeof(s_frameStrata) / sizeof(s_frameStrata[0]); i++) {
        if (!SStrCmpI(s_frameStrata[i].m_name, string, STORM_MAX_STR)) {
            strata = static_cast<FRAME_STRATA>(s_frameStrata[i].m_value);
            return 1;
        }
    }

    return 0;
}

// VERIFY: c7635fb9 auto
// OFFSET: 0x00814E90
const char* FrameStrataToString(FRAME_STRATA strata) {
    for (uint32_t i = 0; i < sizeof(s_frameStrata) / sizeof(s_frameStrata[0]); i++) {
        if (strata == s_frameStrata[i].m_value) {
            return s_frameStrata[i].m_name;
        }
    }

    return "UNKNOWN";
}


// VERIFY: 60497307 auto
// OFFSET: 0x00815910
int32_t LoadXML_Insets(XMLNode* node, float& left, float& right, float& top, float& bottom, CStatus* status) {
    left = 0.0f;
    right = 0.0f;
    top = 0.0f;
    bottom = 0.0f;

    const char* leftString = node->GetAttributeByName("left");

    if (leftString && *leftString) {
        float ndc = SStrToFloat(leftString) / (CoordinateGetAspectCompensation() * 1024.0f);
        left = NDCToDDCWidth(ndc);
    }

    const char* rightString = node->GetAttributeByName("right");

    if (rightString && *rightString) {
        float ndc = SStrToFloat(rightString) / (CoordinateGetAspectCompensation() * 1024.0f);
        right = NDCToDDCWidth(ndc);
    }

    const char* topString = node->GetAttributeByName("top");

    if (topString && *topString) {
        float ndc = SStrToFloat(topString) / (CoordinateGetAspectCompensation() * 1024.0f);
        top = NDCToDDCWidth(ndc);
    }

    const char* bottomString = node->GetAttributeByName("bottom");

    if (bottomString && *bottomString) {
        float ndc = SStrToFloat(bottomString) / (CoordinateGetAspectCompensation() * 1024.0f);
        bottom = NDCToDDCWidth(ndc);
    }

    XMLNode* child = node->m_child;

    if (!child) {
        if (!bottomString) {
            status->Add(STATUS_WARNING, "No \"right\", \"left\", \"top\", or \"bottom\" attributes in element: %s", node->m_name.m_str);
            return 0;
        }

        return 1;
    }

    if (!SStrCmpI(child->m_name.m_str, "AbsInset", STORM_MAX_STR)) {
        const char* absRight = child->GetAttributeByName("right");

        if (absRight && *absRight) {
            float ndc = SStrToFloat(absRight) / (CoordinateGetAspectCompensation() * 1024.0f);
            right = NDCToDDCWidth(ndc);
        }

        const char* absLeft = child->GetAttributeByName("left");

        if (absLeft && *absLeft) {
            float ndc = SStrToFloat(absLeft) / (CoordinateGetAspectCompensation() * 1024.0f);
            left = NDCToDDCWidth(ndc);
        }

        const char* absTop = child->GetAttributeByName("top");

        if (absTop && *absTop) {
            float ndc = SStrToFloat(absTop) / (CoordinateGetAspectCompensation() * 1024.0f);
            top = NDCToDDCWidth(ndc);
        }

        const char* absBottom = child->GetAttributeByName("bottom");

        if (absBottom && *absBottom) {
            float ndc = SStrToFloat(absBottom) / (CoordinateGetAspectCompensation() * 1024.0f);
            bottom = NDCToDDCWidth(ndc);
        }

        return 1;
    }

    if (!SStrCmpI(child->m_name.m_str, "RelInset", STORM_MAX_STR)) {
        const char* relRight = child->GetAttributeByName("right");

        if (relRight && *relRight) {
            right = SStrToFloat(relRight);
        }

        const char* relLeft = child->GetAttributeByName("left");

        if (relLeft && *relLeft) {
            left = SStrToFloat(relLeft);
        }

        const char* relTop = child->GetAttributeByName("top");

        if (relTop && *relTop) {
            top = SStrToFloat(relTop);
        }

        const char* relBottom = child->GetAttributeByName("bottom");

        if (relBottom && *relBottom) {
            bottom = SStrToFloat(relBottom);
        }

        return 1;
    }

    status->Add(STATUS_WARNING, "Unknown child node in %s element: %s", node->m_name.m_str, child->m_name.m_str);

    return 0;
}


// VERIFY: 437bdf6e auto
// OFFSET: 0x00815FA0
CSimpleFontString* LoadXML_String(XMLNode* node, CSimpleFrame* frame, CStatus* status) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    FrameXML::Native::ValidateCreation(node);
    std::unique_ptr<CSimpleFontString> owner(Frame::Native::CreateFontString(frame, 2, true));
    CSimpleFontString* string = owner.get();
#else
    void* data = CSimpleFontString::s_allocator.GetData(0, typeid(CSimpleFontString).raw_name(), SLOG_OBJECT);
    CSimpleFontString* string = data ? new (data) CSimpleFontString(frame, 2, true) : nullptr;

#endif

    string->PreLoadXML(node, status);
    string->LoadXML(node, status);
    string->PostLoadXML(node, status);

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    return owner.release();
#else
    return string;
#endif
}

