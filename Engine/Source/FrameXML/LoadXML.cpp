#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <typeinfo>
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Frame/CSimpleAnimGroup.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/lua-5.1/src/lua.h"





// STATIC: 0x00A44068
static const SMOOTHWEIGHTS s_smoothWeights[] = {
    { 0.0f, 0.0f, "NONE" },
    { 1.0f, 0.0f, "IN" },
    { 0.0f, 1.0f, "OUT" },
    { 1.0f, 1.0f, "IN_OUT" },
    { 1.0f, 1.0f, "OUT_IN" },
};

// STATIC: 0x00A440A4
static const XMLENUMVALUE s_loopTypes[] = {
    { ANIM_LOOPTYPE_NONE, "NONE" },
    { ANIM_LOOPTYPE_REPEAT, "REPEAT" },
    { ANIM_LOOPTYPE_BOUNCE, "BOUNCE" },
};

// STATIC: 0x00A440BC
static const XMLENUMVALUE s_loopStates[] = {
    { ANIM_LOOPSTATE_NONE, "NONE" },
    { ANIM_LOOPSTATE_FORWARD, "FORWARD" },
    { ANIM_LOOPSTATE_REVERSE, "REVERSE" },
};

// STATIC: 0x00A440D4
static const XMLENUMVALUE s_curveTypes[] = {
    { ANIM_CURVE_NONE, "NONE" },
    { ANIM_CURVE_SMOOTH, "SMOOTH" },
};

// STATIC: 0x00A4421C
static const uint8_t s_signaturePublicExponent[4] = { 0x01, 0x00, 0x01, 0x00 };







// VERIFY: 37b9ac8c auto
// OFFSET: 0x00815160
const char* OrientationToString(ORIENTATION orientation) {
    for (uint32_t i = 0; i < sizeof(s_orientations) / sizeof(s_orientations[0]); i++) {
        if (orientation == s_orientations[i].m_value) {
            return s_orientations[i].m_name;
        }
    }

    return "UNKNOWN";
}

// VERIFY: 580b9450 manual
// OFFSET: 0x00815240
int32_t StringToSmoothWeights(const char* string, float& in, float& out) {
    for (uint32_t i = 0; i < sizeof(s_smoothWeights) / sizeof(s_smoothWeights[0]); i++) {
        if (!SStrCmpI(s_smoothWeights[i].m_name, string, STORM_MAX_STR)) {
            in = s_smoothWeights[i].m_in;
            out = s_smoothWeights[i].m_out;
            return 1;
        }
    }

    return 0;
}

// VERIFY: bd8cb02c auto
// OFFSET: 0x008152A0
int32_t StringToLoopType(const char* string, SimpleAnimLoopType& loop) {
    for (uint32_t i = 0; i < sizeof(s_loopTypes) / sizeof(s_loopTypes[0]); i++) {
        if (!SStrCmpI(s_loopTypes[i].m_name, string, STORM_MAX_STR)) {
            loop = static_cast<SimpleAnimLoopType>(s_loopTypes[i].m_value);
            return 1;
        }
    }

    return 0;
}

// VERIFY: bb276483 auto
// OFFSET: 0x008152F0
const char* LoopTypeToString(SimpleAnimLoopType loop) {
    for (uint32_t i = 0; i < sizeof(s_loopTypes) / sizeof(s_loopTypes[0]); i++) {
        if (loop == s_loopTypes[i].m_value) {
            return s_loopTypes[i].m_name;
        }
    }

    return "UNKNOWN";
}

// VERIFY: 0e8c8acf auto
// OFFSET: 0x00815330
const char* LoopStateToString(SimpleAnimLoopState state) {
    for (uint32_t i = 0; i < sizeof(s_loopStates) / sizeof(s_loopStates[0]); i++) {
        if (state == s_loopStates[i].m_value) {
            return s_loopStates[i].m_name;
        }
    }

    return "UNKNOWN";
}

// VERIFY: 5a457d8a auto
// OFFSET: 0x00815370
int32_t StringToCurveType(const char* string, SimpleAnimCurveType& curve) {
    for (uint32_t i = 0; i < sizeof(s_curveTypes) / sizeof(s_curveTypes[0]); i++) {
        if (!SStrCmpI(s_curveTypes[i].m_name, string, STORM_MAX_STR)) {
            curve = static_cast<SimpleAnimCurveType>(s_curveTypes[i].m_value);
            return 1;
        }
    }

    return 0;
}

// VERIFY: 27176859 auto
// OFFSET: 0x008153C0
const char* CurveTypeToString(SimpleAnimCurveType curve) {
    for (uint32_t i = 0; i < sizeof(s_curveTypes) / sizeof(s_curveTypes[0]); i++) {
        if (curve == s_curveTypes[i].m_value) {
            return s_curveTypes[i].m_name;
        }
    }

    return "UNKNOWN";
}

// VERIFY: 41e6d008 auto
// OFFSET: 0x00815590
const char* SmoothWeightsToString(float in, float out) {
    for (uint32_t i = 0; i < sizeof(s_smoothWeights) / sizeof(s_smoothWeights[0]); i++) {
        if (fabs(in - s_smoothWeights[i].m_in) < 0.001f && fabs(out - s_smoothWeights[i].m_out) < 0.001f) {
            return s_smoothWeights[i].m_name;
        }
    }

    return "UNKNOWN";
}



// VERIFY: 4bd552ef manual
// OFFSET: 0x00815EB0
int32_t LoadXML_AnimOrigin(XMLNode* node, FRAMEPOINT& point, C2Vector& offset, CStatus* status) {
    point = FRAMEPOINT_CENTER;
    offset.x = 0.0f;
    offset.y = 0.0f;

    const char* pointString = node->GetAttributeByName("point");

    if (pointString && *pointString) {
        if (!StringToFramePoint(pointString, point)) {
            status->Add(STATUS_WARNING, "Invalid origin point %s in element %s", pointString, node->m_name.m_str);
        }

        XMLNode* offsetNode = node->GetChildByName("Offset");

        if (offsetNode) {
            LoadXML_Dimensions(offsetNode, offset.x, offset.y, status);
        }
    }

    return 1;
}



// VERIFY: 8282815a manual
// OFFSET: 0x00816000
CSimpleAnimGroup* LoadXML_AnimGroup(XMLNode* node, CScriptRegion* region, CStatus* status) {
    CSimpleAnimGroup* group = new CSimpleAnimGroup(region);

    group->PreLoadXML(node, status);
    group->LoadXML(node, status);
    group->PostLoadXML(node, status);

    return group;
}

// VERIFY: 644cec26 auto
// OFFSET: 0x00816060
static void ProcessTocForMakeSign(MD5_CTX* md5, const char* text, const char* basePath) {
    const char* current = text;
    char line[1024];

    do {
        SStrTokenize(&current, line, sizeof(line), "\r\n", nullptr);

        if (!line[0]) {
            break;
        }

        if (line[0] != '#') {
            ProcessFileForMakeSign(md5, line, basePath);
        }
    } while (line[0] && *current);
}

// VERIFY: 0877ed9a manual
// OFFSET: 0x008160D0
static void ParseManifestTag(char* tag, int32_t length, MD5_CTX* md5, const char* basePath) {
    char* end = tag + length;
    char* name = tag;

    while ((*name == '\r' || *name == '\n' || *name == ' ' || *name == '\t') && name < end) {
        name++;
    }

    if (name == end) {
        return;
    }

    char* nameEnd = name;

    while (*nameEnd != '\r' && *nameEnd != '\n' && *nameEnd != ' ' && *nameEnd != '\t' && nameEnd < end) {
        nameEnd++;
    }

    if (nameEnd == end) {
        return;
    }

    *nameEnd = '\0';

    if (SStrCmpI(name, "Script", STORM_MAX_STR) && SStrCmpI(name, "Include", STORM_MAX_STR)) {
        return;
    }

    char* attribute = name;
    char* attributeEnd = nameEnd;

    while (attribute < end) {
        attribute = attributeEnd + 1;

        while ((*attribute == '\r' || *attribute == '\n' || *attribute == ' ' || *attribute == '\t') && attribute < end) {
            attribute++;
        }

        if (attribute == end) {
            return;
        }

        attributeEnd = attribute;

        while (*attributeEnd != '\r' && *attributeEnd != '\n' && *attributeEnd != ' ' && *attributeEnd != '\t' && *attributeEnd != '=' && attributeEnd < end) {
            attributeEnd++;
        }

        if (attributeEnd == end) {
            return;
        }

        *attributeEnd = '\0';

        if (!SStrCmpI(attribute, "file", STORM_MAX_STR)) {
            break;
        }
    }

    if (attribute == end) {
        return;
    }

    char* value = attributeEnd + 1;

    while ((*value == '\r' || *value == '\n' || *value == ' ' || *value == '\t' || *value == '=' || *value == '"') && value < end) {
        value++;
    }

    if (value == end) {
        return;
    }

    char* valueEnd = value;

    while (*valueEnd != '\r' && *valueEnd != '\n' && *valueEnd != '\t' && *valueEnd != '"' && *valueEnd != '/' && valueEnd < end) {
        valueEnd++;
    }

    *valueEnd = '\0';

    ProcessFileForMakeSign(md5, value, basePath);
}

// VERIFY: 65239f1e auto
// OFFSET: 0x00816240
static void ProcessXMLForMakeSign(char* text, uint32_t size, MD5_CTX* md5, const char* basePath) {
    char* end = text + size;
    char* current = text;

    while (*current != '<' && current < end) {
        current++;
    }

    char* tag = current + 1;
    char* cursor = tag;

    while (cursor < end) {
        if (*cursor == '>') {
            *cursor = '\0';
            ParseManifestTag(tag, cursor - tag, md5, basePath);

            char* next = cursor + 1;

            while (*next != '<' && next < end) {
                next++;
            }

            tag = next + 1;
            cursor = tag;
        } else {
            cursor++;
        }
    }
}

// VERIFY: 5341ffcf manual
// OFFSET: 0x008162B0
int32_t ProcessFileForMakeSign(MD5_CTX* md5, const char* filename, const char* basePath) {
    char path[260];
    SStrPrintf(path, sizeof(path), "%s%s", basePath, filename);

    char* end = &path[SStrLen(path)];

    while (end[-1] == ' ') {
        if (end <= path) {
            break;
        }

        end--;
    }

    *end = '\0';

    if (SStrStr(path, "..")) {
        for (char* dots = SStrStr(path, ".."); dots; dots = SStrStr(path, "..")) {
            char before = dots[-1];

            if (before != '\\' && before != '/') {
                break;
            }

            char after = dots[2];

            if (after != '\\' && after != '/') {
                break;
            }

            char* dir = dots - 2;

            while (dir >= path && *dir != '\\' && *dir != '/') {
                dir--;
            }

            SStrCopy(dir + 1, dots + 3, STORM_MAX_STR);
        }
    }

    char directory[260];
    memset(directory, 0, sizeof(directory));

    char* backslash = SStrChrR(path, '\\');
    char* slash = SStrChrR(path, '/');

    if (backslash > slash) {
        slash = backslash;
    }

    if (slash) {
        uint32_t length = slash - path + 2;

        if (length < sizeof(directory)) {
            SStrCopy(directory, path, slash - path + 2);
            directory[length] = '\0';
        }
    }

    void* data;
    uint32_t size;
    int32_t result = SFile::ReadFile(nullptr, path, &data, &size, 1, 1, nullptr);

    if (!result) {
        return result;
    }

    MD5Update(md5, static_cast<const uint8_t*>(data), size);

    const char* extension = SStrChrR(const_cast<char*>(filename), '.');

    if (extension) {
        if (!SStrCmpI(extension, ".toc", STORM_MAX_STR)) {
            ProcessTocForMakeSign(md5, static_cast<const char*>(data), directory);
        } else if (!SStrCmpI(extension, ".xml", STORM_MAX_STR)) {
            ProcessXMLForMakeSign(static_cast<char*>(data), size, md5, directory);
        }
    }

    SFile::Unload(data);

    return 1;
}

// VERIFY: e9b24e51 auto
// OFFSET: 0x008164D0
static void CalculateSignatureMD5(const char* tocFile, const char* extraFile, uint8_t* digest) {
    MD5_CTX context;
    MD5Init(&context);
    ProcessFileForMakeSign(&context, tocFile, "");

    void* data;
    uint32_t size;

    if (extraFile && SFile::ReadFile(nullptr, extraFile, &data, &size, 1, 1, nullptr)) {
        MD5Update(&context, static_cast<const uint8_t*>(data), size);
        SFile::Unload(data);
    }

    MD5Final(digest, &context);
}

// VERIFY: 65235038 auto
// OFFSET: 0x00816550
static int32_t VerifySignature(const uint8_t* data, uint32_t size, const uint8_t* modulus, const char* name) {
    if (!modulus) {
        return 0;
    }

    SSignatureData* token;
    SSignatureVerifyStream_Begin(&token, 256, 4);

    uint32_t signatureLength = SSignatureVerifyStream_GetSignatureLength(token);
    SSignatureVerifyStream_ProvideData(token, data, size - signatureLength);
    SSignatureVerifyStream_ProvideData(token, reinterpret_cast<const uint8_t*>(name), SStrLen(name));
    SSignatureVerifyStream_ProvideData(token, &data[size - signatureLength], signatureLength);

    return SSignatureVerifyStream_Finish(token, modulus, s_signaturePublicExponent);
}

// VERIFY: 29422f35 manual
// OFFSET: 0x008165E0
int32_t FrameXML_CheckSignature(const char* tocFile, const char* extraFile, const uint8_t* modulus, uint8_t* digest) {
#ifdef DISABLE_SIGNATURE_CHECKS
    CalculateSignatureMD5(tocFile, extraFile, digest);
    return 3;
#endif

    char sigFile[260];
    SStrPrintf(sigFile, sizeof(sigFile), "%s.sig", tocFile);

    void* data;
    uint32_t size;

    if (!SFile::ReadFile(nullptr, sigFile, &data, &size, 0, 1, nullptr)) {
        return 0;
    }

    if (size != 276) {
        SFile::Unload(data);
        return 1;
    }

    char* name = SStrChrR(sigFile, '\\');
    name = name ? name + 1 : sigFile;
    SStrUpper(name);

    if (!VerifySignature(static_cast<const uint8_t*>(data), size, modulus, name)) {
        SFile::Unload(data);
        return 1;
    }

    uint8_t signedDigest[16];
    memcpy(signedDigest, data, sizeof(signedDigest));
    SFile::Unload(data);

    CalculateSignatureMD5(tocFile, extraFile, digest);

    return memcmp(signedDigest, digest, sizeof(signedDigest)) ? 2 : 3;
}
