#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Regions.h"
#include "Engine/Source/Frame/Portable/Storage.h"
#define FRAME_REGION_CHECK() Frame::Native::RequireRegions()
#else
#define FRAME_REGION_CHECK() ((void)0)
#endif
#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleRender.h"
#include <malloc.h>
#include <cctype>
#include <cstring>
#include "Engine/Source/Base/ConvertUTF.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Frame/CSimpleEmbeddedTexture.h"
#include "Engine/Source/Gxu/GxuFontString.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDevice/CGxShader.h"
#include "Engine/Source/Gx/CGxVertexPCT.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Services/TextBlock.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CScriptRegion.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include <cmath>
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/Frame/CSimpleFontStringAttributes.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/FrameScript/Types.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Tempest/CMath.h"
#include "Engine/Source/Frame/CFramePoint.h"
#include "Engine/Source/Frame/CSimpleRegion.h"
#include "Engine/Source/Services/SysMessage.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/Tempest/CRect.h"


#ifdef WOWDECOMP_PORTABLE_RUNTIME
namespace {
void QueueRegionText(CRenderBatch* batch, CSimpleFontString* string) {
    Frame::Native::RequireScript(string);
    if (!string->IsA("FontString")) throw std::invalid_argument("Frame batch requires a font-string region");
    CRect rect;
    if (!string->GetRect(&rect) || rect.maxY < 0 || rect.minY > 1 || rect.maxX < 0 || rect.minX > 1 || !string->m_string)
        return;
    batch->NativeQueueText(string->m_fontHandle, string->m_string);
}
}
#endif

// Original font-string consumers shared with the Windows client.
// STATIC: 0x00AC0F14
static TSExplicitList<CSimpleEmbeddedTexture, offsetof(CSimpleEmbeddedTexture, m_link)> s_embeddedTextureStorage;



// STATIC: 0x00B47978
static char s_displayText[0x2000];

// STATIC: 0x00AC1114
int32_t CSimpleFontString::s_metatable = LUA_NOREF;

// STATIC: 0x00B4792C
int32_t CSimpleFontString::s_objectType;

// STATIC: 0x00AC0F10
const char* CSimpleFontString::s_objectTypeName = "FontString";

// STATIC: 0x00B47960
CDataAllocator CSimpleFontString::s_allocator(sizeof(CSimpleFontString), 809);

// STATIC: 0x00B47928
static int32_t s_fontStringCount;

// VERIFY: 0e98fd61 auto
// OFFSET: 0x004826D0
static int32_t IsRectOffscreen(const CRect& rect) {
    return rect.maxY < 0.0f || rect.minY > 1.0f || rect.maxX < 0.0f || rect.minX > 1.0f;
}

// INLINED
int32_t CSimpleFontString::GetObjectType() {
    FRAME_REGION_CHECK();
    if (!CSimpleFontString::s_objectType) {
        CSimpleFontString::s_objectType = ++FrameScript_Object::s_objectTypes;
    }

    return CSimpleFontString::s_objectType;
}

// VERIFY: a08564ec auto
// OFFSET: 0x00485390
bool CSimpleFontString::IsA(int32_t type) {
    FRAME_REGION_CHECK();
    return type == CSimpleFontString::s_objectType || type == CScriptRegion::s_objectType || type == CScriptObject::s_objectType;
}

// VERIFY: 9da7e913 auto
// OFFSET: 0x004853C0
bool CSimpleFontString::IsA(const char* typeName) {
    FRAME_REGION_CHECK();
    return !SStrCmpI(typeName, CSimpleFontString::s_objectTypeName, STORM_MAX_STR) || !SStrCmpI(typeName, CScriptRegion::s_objectTypeName, STORM_MAX_STR) || !SStrCmpI(typeName, CScriptObject::s_objectTypeName, STORM_MAX_STR);
}

// VERIFY: 3a930695 auto
// OFFSET: 0x00485420
const char* CSimpleFontString::GetObjectTypeName() {
    FRAME_REGION_CHECK();
    return CSimpleFontString::s_objectTypeName;
}

// VERIFY: 210fa960 auto
// OFFSET: 0x00485430
int32_t CSimpleFontString::GetScriptMetaTable() {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::UnavailableRegionFeature();
#else

    return CSimpleFontString::s_metatable;

#endif
}

// VERIFY: c3b240f1 auto
// OFFSET: 0x00482270
const char* CSimpleFontString::GetFontName() {
    FRAME_REGION_CHECK();
    return this->m_fontHandle ? TextBlockGetFontName(this->m_fontHandle) : nullptr;
}

// VERIFY: b8a061b3 auto
// OFFSET: 0x00482300
uint32_t CSimpleFontString::GetFontFlags() {
    FRAME_REGION_CHECK();
    return this->m_fontHandle ? TextBlockGetFontFlags(this->m_fontHandle) : 0;
}





// VERIFY: e5b220d7 auto
// OFFSET: 0x00482200
void CSimpleFontString::RemoveShadow() {
    FRAME_REGION_CHECK();
    if (this->m_styleFlags & 0x100) {
        this->m_styleFlags &= ~0x100u;

        if (this->m_string) {
            TextBlockRemoveShadow(this->m_string);
        }
    }
}

// VERIFY: e7bf854c auto
// OFFSET: 0x00482290
float CSimpleFontString::GetFontHeight(bool recalculate) {
    FRAME_REGION_CHECK();
    float fontHeight = this->m_fontHeight;

    if (recalculate && this->m_fontHandle && (this->m_styleFlags & 0x200)) {
        float ndcHeight = GxuFontGetOneToOneHeight(TextBlockGetFontPtr(this->m_fontHandle)) / this->m_layoutScale;
        NDCToDDC(0.0f, ndcHeight, nullptr, &fontHeight);
    }

    return fontHeight;
}

// VERIFY: 496d4923 auto
// OFFSET: 0x00483A40
const char* CSimpleFontString::GetDisplayText(float width, float height) {
    FRAME_REGION_CHECK();
    const char* text = this->m_text && *this->m_text ? this->m_text : nullptr;

    if (!this->m_fontHandle || !text || !*text || width <= 0.0f) {
        return text;
    }

    if (!(this->m_maxLines & 0xFFFFFF) && height <= 0.0f) {
        return text;
    }

    float fontHeight = this->GetFontHeight(true);
    float lineHeight = (CSimpleTop::RoundToPixelHeight(this->m_spacing) + fontHeight) * this->m_layoutScale;

    if (this->m_maxLines & 0xFFFFFF) {
        height = lineHeight * (this->m_maxLines & 0xFFFFFF);
    } else if (lineHeight >= height) {
        height = lineHeight;
    }

    uint32_t numBytes = SStrLen(text);

    if (TextBlockGetMaxCharsWithinWidthAndHeight(this->m_fontHandle, text, this->m_layoutScale * fontHeight, width, height, numBytes, this->m_layoutScale * this->m_shadowOffset.x, this->m_layoutScale, this->m_layoutScale * this->m_spacing, this->m_styleFlags) >= numBytes) {
        return text;
    }

    if (numBytes >= sizeof(s_displayText) - 4) {
        numBytes = sizeof(s_displayText) - 4;
    }

    SStrCopy(s_displayText, text, sizeof(s_displayText));

    uint32_t count;

    do {
        if (numBytes) {
            char ch;

            do {
                ch = s_displayText[numBytes - 1];
                numBytes--;
            } while ((ch & 0xC0) == 0x80 && numBytes);
        }

        SStrCopy(&s_displayText[numBytes], "...", STORM_MAX_STR);
        count = TextBlockGetMaxCharsWithinWidthAndHeight(this->m_fontHandle, s_displayText, this->m_layoutScale * fontHeight, width, height, numBytes + 3, this->m_layoutScale * this->m_shadowOffset.x, this->m_layoutScale, this->m_layoutScale * this->m_spacing, this->m_styleFlags);
    } while (numBytes && count < numBytes + 3);

    return s_displayText;
}

// VERIFY: 586014e2 auto
// OFFSET: 0x004840F0
bool CSimpleFontString::SetLayoutScale(float scale, bool force) {
    FRAME_REGION_CHECK();
    if (!this->CLayoutFrame::SetLayoutScale(scale, force)) {
        return false;
    }

    if (this->m_fontHandle) {
        char fontName[1024];
        SStrCopy(fontName, this->GetFontName(), sizeof(fontName));

        uint32_t fontFlags = this->GetFontFlags();
        float fontHeight = this->m_fontHeight;
        HTEXTFONT font = nullptr;

        if (fontHeight == 0.0f || (font = TextBlockGenerateFont(fontName, fontFlags, fontHeight * this->m_layoutScale)) != nullptr) {
            this->m_fontHeight = fontHeight;

            if (this->m_string) {
                HandleClose(reinterpret_cast<HOBJECT>(this->m_string));
                this->m_string = nullptr;
            }

            if (this->m_fontHandle) {
                HandleClose(reinterpret_cast<HOBJECT>(this->m_fontHandle));
            }

            this->m_fontHandle = font;

            if (font) {
                this->m_textWidth = 0.0f;
                this->m_textHeight = 0.0f;

                if (this->m_string) {
                    HandleClose(reinterpret_cast<HOBJECT>(this->m_string));
                    this->m_string = nullptr;
                }

                this->m_layoutFlags &= ~0x100u;
                this->Resize(0);
            }
        }
    }

    return true;
}

// VERIFY: 45a4d11b auto
// OFFSET: 0x00485850
void CSimpleFontString::QueueTextures(CRenderBatch* batch) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (this->m_textures.Head()) Frame::Native::UnavailableRegionFeature();
#else

    for (CSimpleEmbeddedTexture* embedded = this->m_textures.Head(); !TSList<CSimpleEmbeddedTexture, TSGetExplicitLink<CSimpleEmbeddedTexture> >::IsEnd(embedded); embedded = this->m_textures.RawNext(embedded)) {
        if (embedded->m_texture && embedded->m_texture->m_texture) {
            batch->QueueTexture(embedded->m_texture);
        }
    }

#endif
}

// VERIFY: 502e17cc auto
// OFFSET: 0x00485D60
void CSimpleFontString::Draw(CRenderBatch* batch) {
    FRAME_REGION_CHECK();
    if (this->m_fontHandle && this->m_text && *this->m_text) {
        batch->QueueFontString(this);
        this->QueueTextures(batch);
    }
}

// VERIFY: 70fe81b7 auto
// OFFSET: 0x00485440
void CSimpleFontString::FreeEmbeddedTextures() {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (this->m_textures.Head()) Frame::Native::UnavailableRegionFeature();
#else

    CSimpleEmbeddedTexture* embedded = this->m_textures.Head();

    while (!TSList<CSimpleEmbeddedTexture, TSGetExplicitLink<CSimpleEmbeddedTexture> >::IsEnd(embedded)) {
        CSimpleEmbeddedTexture* next = this->m_textures.RawNext(embedded);
        this->m_textures.UnlinkNode(embedded);
        embedded->m_texture->m_shown = 0;
        embedded->m_texture->HideThis();
        s_embeddedTextureStorage.LinkToTail(embedded);
        embedded = next;
    }

#endif
}

// VERIFY: 23bee593 manual
// OFFSET: 0x004858B0
void CSimpleFontString::FreeEmbededTextureStorage() {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (s_embeddedTextureStorage.Head()) Frame::Native::UnavailableRegionFeature();
#else

    CSimpleEmbeddedTexture* embedded = s_embeddedTextureStorage.Head();

    while (!TSList<CSimpleEmbeddedTexture, TSGetExplicitLink<CSimpleEmbeddedTexture> >::IsEnd(embedded)) {
        embedded = s_embeddedTextureStorage.DeleteNode(embedded);
    }

#endif
}

// INLINED
CSimpleEmbeddedTexture* CSimpleFontString::GetEmbeddedTextureFromStorage() {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::UnavailableRegionFeature();
#else

    CSimpleEmbeddedTexture* embedded = s_embeddedTextureStorage.Head();

    if (embedded) {
        s_embeddedTextureStorage.UnlinkNode(embedded);
        return embedded;
    }

    return new (SMemAlloc(sizeof(CSimpleEmbeddedTexture), __FILE__, __LINE__, 0)) CSimpleEmbeddedTexture;

#endif
}

// VERIFY: 947663f7 manual
// OFFSET: 0x00485DA0
void CSimpleFontString::RefreshEmbeddedTextures() {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    const GXUEMBEDDEDTEXTUREINFO* info;
    if (GxuFontStringEmbeddedTextureInfo(this->m_string ? TextBlockGetStringPtr(this->m_string) : nullptr, info))
        Frame::Native::UnavailableRegionFeature();
#else

    const GXUEMBEDDEDTEXTUREINFO* info;
    uint32_t count = GxuFontStringEmbeddedTextureInfo(this->m_string ? TextBlockGetStringPtr(this->m_string) : nullptr, info);

    for (uint32_t i = 0; i < count; i++) {
        CSimpleEmbeddedTexture* embedded = CSimpleFontString::GetEmbeddedTextureFromStorage();
        this->m_textures.LinkToTail(embedded);
        embedded->InitEmbeddedTexture(this, &info[i]);
    }

#endif
}

// INLINED
void CSimpleFontString::operator delete(void* ptr) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::FreeRegionStorage(CSimpleFontString::s_allocator, ptr);
#else

    if (ptr) {
        CSimpleFontString::s_allocator.PutData(ptr, nullptr, 0);
    }

#endif
}

// VERIFY: ce8bf3cc manual
// OFFSET: 0x00485240
CSimpleFontString::CSimpleFontString(CSimpleFrame* frame, uint32_t drawLayer, bool show) : CSimpleRegion(frame, drawLayer, show) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    this->m_fontHeight = 0.0f;
    this->m_maxLines = 0;
    this->m_shadowColor = CImVector(0, 0, 0, 255);
    this->m_shadowOffset = this->m_justificationOffset = C2Vector(0, 0);
    this->m_alphaGradientLength = 0;
    this->m_animPosition = C3Vector(0, 0, 0);
    CSimpleFontString::GetObjectType();
#endif
    this->m_spacing = 0.0f;
    this->m_textWidth = 0.0f;
    this->m_textHeight = 0.0f;
    this->m_fontHandle = nullptr;
    this->m_textMaxSize = 0;
    this->m_textCurSize = 0;
    this->m_text = nullptr;
    this->m_string = nullptr;
    this->m_shadowColor.a = 0xFF;
    this->m_alphaGradientStart = -1;
    this->m_styleFlags = 0x212;
    this->m_settableStyleFlags = 0x2107F;
    this->m_maxLines = (this->m_maxLines & 0xFE000000) | 0x1000000;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::SetRegionTextQueue(&QueueRegionText);
#endif
    s_fontStringCount++;
}

// VERIFY: fcafba60 auto
// OFFSET: [0x00485370, 0x00485380, 0x00485600, 0x00485D30]
CSimpleFontString::~CSimpleFontString() {
    FRAME_REGION_CHECK();
    this->FreeEmbeddedTextures();

    if (this->m_text) {
        SMemFree(this->m_text, __FILE__, __LINE__, 0);
        this->m_text = nullptr;
    }

    this->SetFont(nullptr, 0.0f, 0, false);

    s_fontStringCount--;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (!s_fontStringCount) Frame::Native::SetRegionTextQueue(nullptr);
#endif
}

// VERIFY: f646e84a manual
// OFFSET: 0x004818F0
void CSimpleFontString::SetTextLength(uint32_t length) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (length > 65534) throw std::length_error("Native FontString text limit");
#endif
    if (length == this->m_textMaxSize) {
        return;
    }

    if (length) {
        char* text = static_cast<char*>(SMemAlloc(length + 1, __FILE__, __LINE__, 0));
        *text = 0;

        if (this->m_text) {
            SMemFree(this->m_text, __FILE__, __LINE__, 0);
        }

        this->m_text = text;
        this->m_textCurSize = length;
    }

    this->m_textMaxSize = length;
}

// VERIFY: df02ba6a manual
// OFFSET: 0x00482230
bool CSimpleFontString::SetAlphaGradient(int32_t start, int32_t length) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (start < -1 || start > INT16_MAX || length < 0 || length > INT16_MAX)
        throw std::invalid_argument("Invalid FontString alpha gradient");
#endif
    this->m_alphaGradientStart = start;
    this->m_alphaGradientLength = length;

    return !this->m_string || TextBlockSetGradient(this->m_string, start, length) < 2;
}

// VERIFY: 8dc263f9 auto
// OFFSET: 0x00482320
void CSimpleFontString::SetWidth(float width) {
    FRAME_REGION_CHECK();
    this->CLayoutFrame::SetWidth(width);
    this->m_textWidth = 0.0f;
    this->m_textHeight = 0.0f;
}

// VERIFY: 0b7750c4 auto
// OFFSET: 0x00482350
void CSimpleFontString::SetHeight(float height) {
    FRAME_REGION_CHECK();
    this->CLayoutFrame::SetHeight(height);
    this->m_textWidth = 0.0f;
    this->m_textHeight = 0.0f;
}

// VERIFY: fd665b85 auto
// OFFSET: 0x00482380
float CSimpleFontString::GetTextWidth(const char* text, uint32_t numBytes) {
    FRAME_REGION_CHECK();
    float extent = 0.0f;

    if (!this->m_fontHandle) {
        return extent;
    }

    if (!numBytes) {
        numBytes = SStrLen(text);
    }

    TextBlockGetTextExtent(this->m_fontHandle, text, numBytes, this->m_layoutScale * this->GetFontHeight(true), &extent, this->m_shadowOffset.x * this->m_layoutScale, this->m_layoutScale, 0.0f, this->m_styleFlags);

    return extent / this->m_layoutScale;
}

// VERIFY: 7b82c1df manual
// OFFSET: 0x00482450
uint32_t CSimpleFontString::WrapText(const char* text, float width, uint32_t* lineStarts, uint32_t maxLines) {
    FRAME_REGION_CHECK();
    if (!this->m_fontHandle) {
        return 0;
    }

    return TextBlockWrapText(this->m_fontHandle, text, this->GetFontHeight(true) * this->m_layoutScale, width * this->m_layoutScale, lineStarts, maxLines, this->m_shadowOffset.x * this->m_layoutScale, this->m_layoutScale, 0.0f, this->m_styleFlags);
}

// VERIFY: 92162b43 manual
// OFFSET: 0x00482510
uint32_t CSimpleFontString::GetNumCharsWithinWidth(const char* text, uint32_t numBytes, float maxWidth) {
    FRAME_REGION_CHECK();
    if (!this->m_fontHandle) {
        return 0;
    }

    if (!numBytes) {
        numBytes = SStrLen(text);
    }

    float extent;

    return TextBlockGetMaxCharsWithinWidth(this->m_fontHandle, text, this->m_layoutScale * this->GetFontHeight(true), this->m_layoutScale * maxWidth, numBytes, &extent, this->m_shadowOffset.x * this->m_layoutScale, this->m_layoutScale, 0.0f, this->m_styleFlags);
}

// VERIFY: 211ff357 manual
// OFFSET: 0x004825E0
uint32_t CSimpleFontString::GetNumCharsWithinWidthFromEnd(const char* text, uint32_t numBytes, float maxWidth) {
    FRAME_REGION_CHECK();
    if (!this->m_fontHandle) {
        return 0;
    }

    if (!numBytes) {
        numBytes = SStrLen(text);
    }

    float extent;

    return TextBlockGetMaxCharsWithinWidthFromEnd(this->m_fontHandle, text, this->m_layoutScale * this->GetFontHeight(true), this->m_layoutScale * maxWidth, numBytes, &extent, this->m_shadowOffset.x * this->m_layoutScale, this->m_layoutScale, 0.0f, this->m_styleFlags);
}

// VERIFY: 5d78882a auto
// OFFSET: 0x004826B0
void CSimpleFontString::OnScreenSizeChanged() {
    FRAME_REGION_CHECK();
    this->m_textWidth = 0.0f;
    this->m_textHeight = 0.0f;
    this->Resize(0);
}

// VERIFY: 5cc3b167 auto
// OFFSET: 0x00482720
void CSimpleFontString::AddAnimTranslation(const C2Vector& translation) {
    FRAME_REGION_CHECK();
    if (this->m_string) {
        C3Vector position = TextBlockGetStringPos(this->m_string);
        ApplyTranslationAnim(position, translation);
        TextBlockSetStringPos(this->m_string, position);
    }
}

// VERIFY: ebf55995 auto
// OFFSET: 0x00482E10
void CSimpleFontString::ClearString() {
    FRAME_REGION_CHECK();
    this->m_textWidth = 0.0f;
    this->m_textHeight = 0.0f;

    if (this->m_string) {
        HandleClose(reinterpret_cast<HOBJECT>(this->m_string));
        this->m_string = nullptr;
    }

    this->m_layoutFlags &= ~0x100u;
    this->Resize(0);
}

// VERIFY: 45b31c86 manual
// OFFSET: 0x004836D0
void CSimpleFontString::PostLoadXML(XMLNode* node, CStatus* status) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::UnavailableRegionFeature();
#else

    if (!this->m_parent || this->HasAnyFramepoints()) {
        return;
    }

    if (this->m_styleFlags & 0x1) {
        this->SetPoint(FRAMEPOINT_LEFT, this->m_parent, FRAMEPOINT_LEFT, 0.0f, 0.0f, 1);
    } else if (this->m_styleFlags & 0x4) {
        this->SetPoint(FRAMEPOINT_RIGHT, this->m_parent, FRAMEPOINT_RIGHT, 0.0f, 0.0f, 1);
    } else {
        this->SetPoint(FRAMEPOINT_CENTER, this->m_parent, FRAMEPOINT_CENTER, 0.0f, 0.0f, 1);
    }

#endif
}

// VERIFY: 96073ff2 auto
// OFFSET: 0x00483750
int32_t CSimpleFontString::SetFont(const char* fontName, float fontHeight, uint32_t fontFlags, bool force) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireDimension(fontHeight);
#endif
    if (!force) {
        const char* currentName = this->GetFontName();

        if (fontName && currentName && !SStrCmpI(fontName, currentName, STORM_MAX_STR) && fequal(fontHeight, this->m_fontHeight) && fontFlags == this->GetFontFlags()) {
            return 1;
        }
    }

    HTEXTFONT font = nullptr;

    if (fontName && fontHeight != 0.0f) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
        try { font = TextBlockGenerateFont(fontName, fontFlags, fontHeight * this->m_layoutScale); }
        catch (const std::exception&) { return 0; }
#else
        font = TextBlockGenerateFont(fontName, fontFlags, fontHeight * this->m_layoutScale);
#endif

        if (!font) {
            return 0;
        }
    }

    this->m_fontHeight = fontHeight;

    if (this->m_string) {
        HandleClose(reinterpret_cast<HOBJECT>(this->m_string));
        this->m_string = nullptr;
    }

    if (this->m_fontHandle) {
        HandleClose(reinterpret_cast<HOBJECT>(this->m_fontHandle));
    }

    this->m_fontHandle = font;

    if (font) {
        this->ClearString();
    }

    return 1;
}

// VERIFY: 3d8d8078 auto
// OFFSET: 0x00483890
void CSimpleFontString::SetTextHeight(float height) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireDimension(height);
#endif
    if (fabsf(height - this->m_fontHeight) >= 0.00000023841858f) {
        this->m_fontHeight = height;
        this->m_styleFlags &= ~0x200u;
        this->ClearString();
    }
}

// VERIFY: 5aa8ddc3 manual
// OFFSET: 0x00483910
void CSimpleFontString::SetText(const char* text, int32_t process) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (text && SStrLen(text) > 65534) throw std::length_error("Native FontString text limit");
    if (text && process) Frame::Native::UnavailableRegionFeature();
    if (text && std::strstr(text, "|T")) Frame::Native::UnavailableRegionFeature();
#endif
    if (text && process) {
#ifndef WOWDECOMP_PORTABLE_RUNTIME
        text = LanguageProcess(text);
#endif
    }

    if ((!text || !*text) && (!this->m_text || !*this->m_text)) {
        return;
    }

    if (text) {
        if (this->m_text && !SStrCmp(text, this->m_text, STORM_MAX_STR)) {
            return;
        }

        if (*text) {
            if (this->m_textMaxSize) {
                SStrCopy(this->m_text, text, this->m_textMaxSize);
            } else {
                uint32_t length = SStrLen(text);

                if (length > this->m_textCurSize) {
                    if (this->m_text) {
                        SMemFree(this->m_text, __FILE__, __LINE__, 0);
                    }

                    this->m_text = SStrDupA(text, __FILE__, __LINE__);
                    this->m_textCurSize = length;
                } else {
                    SStrCopy(this->m_text, text, STORM_MAX_STR);
                }
            }

            this->ClearString();
            return;
        }
    }

    if (this->m_text) {
        *this->m_text = 0;
    }

    this->ClearString();
}

// VERIFY: 63874679 auto
// OFFSET: 0x00483C40
void CSimpleFontString::SetJustificationOffset(float x, float y) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireFinite(x); Frame::Native::RequireFinite(y);
#endif
    if (x == this->m_justificationOffset.x && this->m_justificationOffset.y == y) {
        return;
    }

    this->m_justificationOffset.x = x;
    this->m_justificationOffset.y = y;

    if (this->m_string) {
        C3Vector position;
        position.x = this->m_layoutScale * this->m_justificationOffset.x + this->m_rect.minX;
        position.y = this->m_justificationOffset.y * this->m_layoutScale + this->m_rect.minY;
        position.z = this->m_layoutDepth;
        TextBlockSetStringPos(this->m_string, position);
    }
}

// VERIFY: 273d1f7a manual
// OFFSET: 0x00483CD0
void CSimpleFontString::AddShadow(const CImVector& color, const C2Vector& offset) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireFinite(offset.x); Frame::Native::RequireFinite(offset.y);
#endif
    if ((this->m_styleFlags & 0x100) && *reinterpret_cast<const uint32_t*>(&color) == *reinterpret_cast<uint32_t*>(&this->m_shadowColor) && this->m_shadowOffset.x == offset.x && this->m_shadowOffset.y == offset.y) {
        return;
    }

    this->m_styleFlags |= 0x100;
    this->m_shadowColor = color;
    this->m_shadowOffset = offset;

    if (this->m_string) {
        CImVector shadowColor = this->m_shadowColor;
        C2Vector shadowOffset;
        shadowOffset.x = this->m_shadowOffset.x * this->m_layoutScale;
        shadowOffset.y = this->m_shadowOffset.y * this->m_layoutScale;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
            uint8_t alpha = this->m_parent ? this->m_parent->GetEffectiveAlpha() : 255;
#else
            uint8_t alpha = this->m_parent->m_alpha * this->m_parent->m_parentAlpha / 255;
#endif
        shadowColor.a = this->m_shadowColor.a * alpha / 255;
        TextBlockAddShadow(this->m_string, shadowColor, shadowOffset);
    }
}

// VERIFY: e9c58d61 manual
// OFFSET: 0x00483DD0
void CSimpleFontString::SetShadowOffset(const C2Vector& offset) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireFinite(offset.x); Frame::Native::RequireFinite(offset.y);
#endif
    if (this->m_shadowOffset.x == offset.x && this->m_shadowOffset.y == offset.y) {
        return;
    }

    this->m_shadowOffset = offset;

    if (this->m_shadowOffset.x == 0.0f && this->m_shadowOffset.y == 0.0f) {
        this->RemoveShadow();
    } else {
        this->AddShadow(this->m_shadowColor, this->m_shadowOffset);
    }

    this->ClearString();
}

// VERIFY: 230de876 auto
// OFFSET: 0x00483E80
float CSimpleFontString::GetStringWidth() {
    FRAME_REGION_CHECK();
    if (this->m_textWidth == 0.0f && this->m_fontHandle) {
        const char* text = this->GetDisplayText(this->CLayoutFrame::GetWidth() * this->m_layoutScale, this->CLayoutFrame::GetHeight() * this->m_layoutScale);

        if (text && *text) {
            TextBlockGetTextExtent(this->m_fontHandle, text, SStrLen(text), this->GetFontHeight(true) * this->m_layoutScale, &this->m_textWidth, this->m_shadowOffset.x * this->m_layoutScale, this->m_layoutScale, 0.0f, this->m_styleFlags);
        }
    }

    return this->m_textWidth / this->m_layoutScale;
}

// VERIFY: 9a3e23aa auto
// OFFSET: 0x00483F30
float CSimpleFontString::GetWidth() {
    FRAME_REGION_CHECK();
    float width = this->CLayoutFrame::GetWidth();

    if (width == 0.0f) {
        width = this->GetStringWidth();
    }

    return width > NDCToDDCWidth(1.0f / (CoordinateGetAspectCompensation() * 1024.0f)) ? width : NDCToDDCWidth(1.0f / (CoordinateGetAspectCompensation() * 1024.0f));
}

// VERIFY: 5c920bfb auto
// OFFSET: 0x00483FA0
float CSimpleFontString::GetStringHeight() {
    FRAME_REGION_CHECK();
    if (this->m_textHeight == 0.0f && this->m_fontHandle) {
        const char* text = this->GetDisplayText(this->CLayoutFrame::GetWidth() * this->m_layoutScale, this->CLayoutFrame::GetHeight() * this->m_layoutScale);

        if (text && *text) {
            C2Vector shadowOffset;
            shadowOffset.x = this->m_shadowOffset.x * this->m_layoutScale;
            shadowOffset.y = this->m_layoutScale * this->m_shadowOffset.y;
            this->m_textHeight = TextBlockGetWrappedTextHeight(this->m_fontHandle, text, this->GetFontHeight(true) * this->m_layoutScale, this->GetWidth() * this->m_layoutScale, shadowOffset, this->m_layoutScale, this->m_spacing * this->m_layoutScale, this->m_styleFlags);
        }
    }

    return this->m_textHeight / this->m_layoutScale;
}

// VERIFY: 34e78a2e auto
// OFFSET: 0x00484080
float CSimpleFontString::GetHeight() {
    FRAME_REGION_CHECK();
    float height = this->CLayoutFrame::GetHeight();

    if (height == 0.0f) {
        height = this->GetStringHeight();
    }

    return height > NDCToDDCWidth(1.0f / (CoordinateGetAspectCompensation() * 1024.0f)) ? height : NDCToDDCWidth(1.0f / (CoordinateGetAspectCompensation() * 1024.0f));
}

// VERIFY: 4a6b69f4 auto
// OFFSET: 0x00484230
bool CSimpleFontString::SetLayoutDepth(float depth, bool force) {
    FRAME_REGION_CHECK();
    if (!this->CLayoutFrame::SetLayoutDepth(depth, force)) {
        return false;
    }

    if (this->m_string) {
        C3Vector position;
        position.x = this->m_justificationOffset.x * this->m_layoutScale + this->m_rect.minX;
        position.y = this->m_justificationOffset.y * this->m_layoutScale + this->m_rect.minY;
        position.z = this->m_layoutDepth;
        TextBlockSetStringPos(this->m_string, position);
    }

    return true;
}

// VERIFY: 026ce15c manual
// OFFSET: 0x004842A0
void CSimpleFontString::PreOnAnimUpdate() {
    FRAME_REGION_CHECK();
    if (!this->m_animating) {
        return;
    }

    if (this->m_string) {
        TextBlockSetStringPos(this->m_string, this->m_animPosition);
    }

    if (this->m_animAlpha) {
        uint8_t alpha = this->m_animSavedAlpha;
        CImVector color;
        this->GetVertexColor(color);
        color.a = alpha;
        this->SetVertexColor(color);
    }
}

// VERIFY: db02a088 manual
// OFFSET: 0x00484300
void CSimpleFontString::StoreAnimPosition() {
    FRAME_REGION_CHECK();
    this->CSimpleRegion::StoreAnimPosition();
    this->m_animPosition.x = this->m_justificationOffset.x * this->m_layoutScale + this->m_rect.minX;
    this->m_animPosition.y = this->m_justificationOffset.y * this->m_layoutScale + this->m_rect.minY;
    this->m_animPosition.z = this->m_layoutDepth;

    if (this->m_string) {
        TextBlockSetStringPos(this->m_string, this->m_animPosition);
    }
}

// VERIFY: c02c2d45 manual
// OFFSET: 0x00484350
void CSimpleFontString::ResetAnimPosition() {
    FRAME_REGION_CHECK();
    this->CSimpleRegion::ResetAnimPosition();
    this->m_animPosition.x = this->m_justificationOffset.x * this->m_layoutScale + this->m_rect.minX;
    this->m_animPosition.y = this->m_justificationOffset.y * this->m_layoutScale + this->m_rect.minY;
    this->m_animPosition.z = this->m_layoutDepth;

    if (this->m_string) {
        TextBlockSetStringPos(this->m_string, this->m_animPosition);
    }
}

// VERIFY: 50c246ae manual
// OFFSET: 0x004856F0
void CSimpleFontString::OnColorChanged(bool changed) {
    FRAME_REGION_CHECK();
    this->CSimpleRegion::OnColorChanged(changed);

    if (this->m_string) {
        CImVector color;
        color.b = 0xFF;
        color.g = 0xFF;
        color.r = 0xFF;
        color.a = 0xFF;

        if (this->m_colorCount == 1) {
            color = this->m_colors[0];
        }

        TextBlockUpdateColor(this->m_string, color);

        if (this->m_styleFlags & 0x100) {
            CImVector shadowColor = this->m_shadowColor;
            C2Vector shadowOffset;
            shadowOffset.x = this->m_shadowOffset.x * this->m_layoutScale;
            shadowOffset.y = this->m_shadowOffset.y * this->m_layoutScale;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
            uint8_t alpha = this->m_parent ? this->m_parent->GetEffectiveAlpha() : 255;
#else
            uint8_t alpha = this->m_parent->m_alpha * this->m_parent->m_parentAlpha / 255;
#endif
            shadowColor.a = this->m_shadowColor.a * alpha / 255;
            TextBlockAddShadow(this->m_string, shadowColor, shadowOffset);
        }
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (this->m_textures.Head()) Frame::Native::UnavailableRegionFeature();
#else
    for (CSimpleEmbeddedTexture* embedded = this->m_textures.Head(); !TSList<CSimpleEmbeddedTexture, TSGetExplicitLink<CSimpleEmbeddedTexture> >::IsEnd(embedded); embedded = this->m_textures.RawNext(embedded)) {
        if (embedded->m_texture && embedded->m_texture->m_texture) {
            CImVector color;
            this->GetVertexColor(color);
            CSimpleTexture* texture = embedded->m_texture;
            uint8_t alpha = color.a;
            CImVector textureColor;
            texture->GetVertexColor(textureColor);
            textureColor.a = alpha;
            texture->SetVertexColor(textureColor);
        }
    }
#endif
}

// VERIFY: 1148256f manual
// OFFSET: 0x00486B20
void CSimpleFontString::UpdateString() {
    FRAME_REGION_CHECK();
    if (!(this->m_layoutFlags & 0x100)) {
        return;
    }

    if (this->m_string) {
        HandleClose(reinterpret_cast<HOBJECT>(this->m_string));
        this->m_string = nullptr;
    }

    this->FreeEmbeddedTextures();

    if (!(this->m_rect.maxY > this->m_rect.minY) || !(this->m_rect.maxX > this->m_rect.minX)) {
        return;
    }

    if (this->m_fontHandle && this->m_text && *this->m_text) {
        C3Vector position;
        position.x = this->m_justificationOffset.x * this->m_layoutScale + this->m_rect.minX;
        position.y = this->m_justificationOffset.y * this->m_layoutScale + this->m_rect.minY;
        position.z = this->m_layoutDepth;

        float width = this->CLayoutFrame::GetWidth();
        width = width != 0.0f ? width * this->m_layoutScale : this->m_textWidth;

        if (fnotequalz(width, this->m_rect.maxX - this->m_rect.minX, 0.00001f)) {
            width = this->m_rect.maxX - this->m_rect.minX;
        }

        float height = this->CLayoutFrame::GetHeight();
        height = height != 0.0f ? height * this->m_layoutScale : this->m_textHeight;

        if (fnotequalz(height, this->m_rect.maxY - this->m_rect.minY, 0.00001f)) {
            height = this->m_rect.maxY - this->m_rect.minY;
        }

        const char* text = this->GetDisplayText(width, height);
        uint32_t styleFlags = this->m_styleFlags;

        CImVector color;
        color.b = 0xFF;
        color.g = 0xFF;
        color.r = 0xFF;
        color.a = 0xFF;

        if (!(styleFlags & 0x400)) {
            bool hasColorCode = false;

            for (int32_t i = 0; text[i]; i++) {
                if (text[i] == '|') {
                    if (text[i + 1] == '|') {
                        i++;
                    } else if (text[i + 1] == 'C' || text[i + 1] == 'c') {
                        hasColorCode = true;
                        break;
                    }
                }
            }

            if (!hasColorCode) {
                styleFlags |= 0x400;
            }
        }

        if (this->m_colorCount) {
            color = this->m_colors[0];
        }

        this->m_string = TextBlockCreate(this->m_fontHandle, text, color, position, this->GetFontHeight(true) * this->m_layoutScale, width, height, styleFlags, 0.0f, this->m_spacing * this->m_layoutScale, this->m_layoutScale);

        if (this->m_styleFlags & 0x100) {
            CImVector shadowColor = this->m_shadowColor;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
            uint8_t alpha = this->m_parent ? this->m_parent->GetEffectiveAlpha() : 255;
#else
            uint8_t alpha = this->m_parent->GetEffectiveAlpha();
#endif
            C2Vector shadowOffset;
            shadowOffset.x = this->m_shadowOffset.x * this->m_layoutScale;
            shadowOffset.y = this->m_shadowOffset.y * this->m_layoutScale;
            shadowColor.a = this->m_shadowColor.a * alpha / 255;
            TextBlockAddShadow(this->m_string, shadowColor, shadowOffset);
        }

        if (this->m_alphaGradientStart >= 0) {
            TextBlockSetGradient(this->m_string, this->m_alphaGradientStart, this->m_alphaGradientLength);
        }

        this->RefreshEmbeddedTextures();

        if (this->m_animating) {
            this->m_animPosition = position;
            TextBlockSetStringPos(this->m_string, position);
        }
    }

    this->OnRegionChanged();
}

// VERIFY: 39fc54f9 manual
// OFFSET: 0x00486E30
void CSimpleFontString::OnFrameSizeChanged(const CRect& rect) {
    FRAME_REGION_CHECK();
    this->CLayoutFrame::OnFrameSizeChanged(rect);

    if (this->m_string && fabsf((rect.maxX - rect.minX) - (this->m_rect.maxX - this->m_rect.minX)) < 0.00001f && fabsf((rect.maxY - rect.minY) - (this->m_rect.maxY - this->m_rect.minY)) < 0.00001f) {
        C3Vector position;
        position.x = this->m_justificationOffset.x * this->m_layoutScale + this->m_rect.minX;
        position.y = this->m_justificationOffset.y * this->m_layoutScale + this->m_rect.minY;
        position.z = this->m_layoutDepth;
        TextBlockSetStringPos(this->m_string, position);

        if (IsRectOffscreen(this->m_rect) != IsRectOffscreen(rect)) {
            this->OnRegionChanged();
        }
    } else {
        this->UpdateString();
    }

    if (this->m_animating) {
        C3Vector previous = this->m_animPosition;
        this->m_animPosition.x = this->m_justificationOffset.x * this->m_layoutScale + this->m_rect.minX;
        this->m_animPosition.y = this->m_justificationOffset.y * this->m_layoutScale + this->m_rect.minY;
        this->m_animPosition.z = this->m_layoutDepth;

        if (this->m_string) {
            C3Vector delta = this->m_animPosition - previous;
            C3Vector position = TextBlockGetStringPos(this->m_string) + delta;
            TextBlockSetStringPos(this->m_string, position);
        }
    }

    if (this->m_parent) {
        this->m_parent->NotifyScrollParent();
    }
}

// VERIFY: 86a4fd93 auto
// OFFSET: 0x00487090
void CSimpleFontString::SetStyleFlags(uint32_t styleFlags) {
    FRAME_REGION_CHECK();
    uint32_t flags = (styleFlags & this->m_settableStyleFlags) | (this->m_styleFlags & ~this->m_settableStyleFlags);

    if (flags != this->m_styleFlags) {
        this->m_styleFlags = flags;

        if (this->m_string) {
            this->UpdateString();
        }
    }
}

// VERIFY: ee7180a8 auto
// OFFSET: 0x004870D0
void CSimpleFontString::SetSpacing(float spacing) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireFinite(spacing);
#endif
    if (spacing < 0.0f) {
        spacing = 0.0f;
    }

    if (fabsf(spacing - this->m_spacing) >= 0.00000023841858f) {
        this->m_spacing = spacing;
        this->m_textHeight = 0.0f;

        if (this->m_string) {
            this->UpdateString();
        }
    }
}

// VERIFY: adb869c7 auto
// OFFSET: 0x00487130
void CSimpleFontString::SetShadowColor(const CImVector& color) {
    FRAME_REGION_CHECK();
    if (*reinterpret_cast<const uint32_t*>(&color) != *reinterpret_cast<uint32_t*>(&this->m_shadowColor)) {
        this->m_shadowColor = color;

        if (this->m_string) {
            this->UpdateString();
        }
    }
}

// VERIFY: 283d01b1 auto
// OFFSET: 0x00487190
void CSimpleFontString::SetJustifyH(uint8_t justify) {
    FRAME_REGION_CHECK();
    this->m_settableStyleFlags &= ~0x7u;
    uint32_t styleFlags = (this->m_styleFlags & ~0x7u) | (justify & 0x7);

    if (styleFlags != this->m_styleFlags) {
        this->m_styleFlags = styleFlags;

        if (this->m_string) {
            this->UpdateString();
        }
    }
}

// VERIFY: d976f70d auto
// OFFSET: 0x004871D0
void CSimpleFontString::SetJustifyV(uint8_t justify) {
    FRAME_REGION_CHECK();
    this->m_settableStyleFlags &= ~0x38u;
    uint32_t styleFlags = (this->m_styleFlags & ~0x38u) | (justify & 0x38);

    if (styleFlags != this->m_styleFlags) {
        this->m_styleFlags = styleFlags;

        if (this->m_string) {
            this->UpdateString();
        }
    }
}

// VERIFY: e9a340b1 auto
// OFFSET: 0x00487210
void CSimpleFontString::SetNonWordWrap(int32_t noWrap) {
    FRAME_REGION_CHECK();
    this->m_settableStyleFlags &= ~0x40u;
    uint32_t styleFlags = noWrap ? this->m_styleFlags | 0x40 : this->m_styleFlags & ~0x40u;

    if (styleFlags != this->m_styleFlags) {
        this->m_styleFlags = styleFlags;

        if (this->m_string) {
            this->UpdateString();
        }
    }
}

// VERIFY: eb8e3134 auto
// OFFSET: 0x00487250
void CSimpleFontString::SetNonSpaceWrap(int32_t wrap) {
    FRAME_REGION_CHECK();
    this->m_settableStyleFlags &= ~0x1000u;
    uint32_t styleFlags = wrap ? this->m_styleFlags | 0x1000 : this->m_styleFlags & ~0x1000u;

    if (styleFlags != this->m_styleFlags) {
        this->m_styleFlags = styleFlags;

        if (this->m_string) {
            this->UpdateString();
        }
    }
}

// VERIFY: 8f4f99c8 auto
// OFFSET: 0x004872A0
void CSimpleFontString::SetIndented(int32_t indented) {
    FRAME_REGION_CHECK();
    this->m_settableStyleFlags &= ~0x20000u;
    uint32_t styleFlags = indented ? this->m_styleFlags | 0x20000 : this->m_styleFlags & ~0x20000u;

    if (styleFlags != this->m_styleFlags) {
        this->m_styleFlags = styleFlags;

        if (this->m_string) {
            this->UpdateString();
        }
    }
}

// VERIFY: 83e312ff manual
// OFFSET: 0x004873E0
void CSimpleFontString::LoadXML(XMLNode* node, CStatus* status) {
    FRAME_REGION_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::UnavailableRegionFeature();
#else

    const char* inherits = node->GetAttributeByName("inherits");

    if (inherits && *inherits) {
        CSimpleFont* font = CSimpleFont::GetFont(inherits, 0);

        if (font) {
            if (this->m_maxLines & 0x1000000) {
                this->SetFontObject(font);
            }
        } else {
            const char* taint;
            bool locked;
            XMLNode* inheritNode = FrameXML_AcquireHashNode(inherits, taint, locked);

            if (!inheritNode) {
                status->Add(STATUS_WARNING, "Couldn't find inherited node: %s", inherits);
            } else if (locked) {
                status->Add(STATUS_WARNING, "Recursively inherited node: %s", inherits);
            } else {
                this->LoadXML(inheritNode, status);
                FrameXML_ReleaseHashNode(inherits);
            }
        }
    }

    this->CScriptRegion::LoadXML(node, status);

    const char* hidden = node->GetAttributeByName("hidden");

    if (hidden && *hidden) {
        if (StringToBOOL(hidden)) {
            this->m_shown = 0;
            this->HideThis();
        } else {
            this->m_shown = 1;
            this->ShowThis();
        }
    }

    const char* textAttr = node->GetAttributeByName("text");

    if (textAttr && *textAttr) {
        const char* text = FrameScript_GetText(textAttr, -1, GENDER_NOT_APPLICABLE);

        if (!text || !*text) {
            text = textAttr;
        }

        this->SetText(text, 1);
    }

    const char* nonSpaceWrap = node->GetAttributeByName("nonspacewrap");

    if (nonSpaceWrap && *nonSpaceWrap) {
        this->SetNonSpaceWrap(StringToBOOL(nonSpaceWrap));
    }

    const char* wordWrap = node->GetAttributeByName("wordwrap");

    if (wordWrap && *wordWrap) {
        this->SetNonWordWrap(StringToBOOL(wordWrap) == 0);
    }

    const char* bytes = node->GetAttributeByName("bytes");

    if (bytes && *bytes) {
        this->SetTextLength(SStrToInt(bytes));
    }

    const char* maxLines = node->GetAttributeByName("maxLines");

    if (maxLines && *maxLines) {
        this->m_maxLines = (this->m_maxLines & ~0xFFFFFFu) | (SStrToUnsigned(maxLines) & 0xFFFFFF);
    }

    if (!(this->m_maxLines & 0x1000000)) {
        return;
    }

    const char* fontName = node->GetAttributeByName("font");

    if (fontName && *fontName) {
        CSimpleFont* font = CSimpleFont::GetFont(fontName, 0);

        if (font) {
            this->SetFontObject(font);
        } else {
            float fontHeight = 0.0f;
            XMLNode* heightNode = node->GetChildByName("FontHeight");

            if (heightNode) {
                LoadXML_Value(heightNode, fontHeight, status);
            }

            if (fontHeight <= 0.00000011920929f) {
                status->Add(STATUS_WARNING, "FontString %s: invalid font height %f in %s element", this->GetDisplayName(), fontHeight, node->m_name.m_str);
                return;
            }

            uint32_t fontFlags = 0;
            const char* outline = node->GetAttributeByName("outline");

            if (outline && *outline) {
                if (!SStrCmpI(outline, "NORMAL", STORM_MAX_STR)) {
                    fontFlags = 1;
                } else if (!SStrCmpI(outline, "THICK", STORM_MAX_STR)) {
                    fontFlags = 5;
                }
            }

            const char* monochrome = node->GetAttributeByName("monochrome");

            if (monochrome && *monochrome && StringToBOOL(monochrome)) {
                fontFlags |= 2;
            }

            if (this->SetFont(fontName, fontHeight, fontFlags, false)) {
                this->m_fontableFlags &= ~0x1u;
            } else {
                status->Add(STATUS_WARNING, "FontString %s: Unable to load font file %s", this->GetDisplayName(), fontName);
            }
        }
    }

    const char* spacing = node->GetAttributeByName("spacing");

    if (spacing && *spacing) {
        float value = SStrToFloat(spacing);
        this->SetSpacing(NDCToDDCWidth(value / (CoordinateGetAspectCompensation() * 1024.0f)));
        this->m_fontableFlags &= ~0x10u;
    }

    uint32_t justify;
    const char* justifyV = node->GetAttributeByName("justifyV");

    if (justifyV && *justifyV && StringToJustify(justifyV, justify)) {
        this->SetJustifyV(justify);
        this->m_fontableFlags &= ~0x2u;
    }

    const char* justifyH = node->GetAttributeByName("justifyH");

    if (justifyH && *justifyH && StringToJustify(justifyH, justify)) {
        this->SetJustifyH(justify);
        this->m_fontableFlags &= ~0x2u;
    }

    const char* indented = node->GetAttributeByName("indented");

    if (indented && *indented) {
        this->SetIndented(StringToBOOL(indented));
    }

    const char* alphaAttr = node->GetAttributeByName("alpha");

    if (alphaAttr && *alphaAttr) {
        float alpha = SStrToFloat(alphaAttr);

        if (alpha < 0.0f) {
            alpha = 0.0f;
        } else if (alpha >= 1.0f) {
            alpha = 1.0f;
        }

        this->SetVertexAlpha(static_cast<uint8_t>(alpha * 255.0f));
    }

    for (XMLNode* child = node->m_child; child; child = child->m_next) {
        if (!SStrCmpI(child->m_name.m_str, "Color", STORM_MAX_STR)) {
            CImVector color;
            LoadXML_Color(child, color, status);
            this->SetVertexColor(color);
            this->m_fontableFlags &= ~0x4u;
        } else if (!SStrCmpI(child->m_name.m_str, "Shadow", STORM_MAX_STR)) {
            float offsetX = 0.001f;
            float offsetY = -0.001f;
            CImVector color;
            color.a = 0xFF;

            XMLNode* colorNode = child->GetChildByName("Color");

            if (colorNode) {
                LoadXML_Color(colorNode, color, status);
            }

            XMLNode* offsetNode = child->GetChildByName("Offset");

            if (offsetNode) {
                LoadXML_Dimensions(offsetNode, offsetX, offsetY, status);
            }

            C2Vector offset;
            offset.x = offsetX;
            offset.y = offsetY;
            this->AddShadow(color, offset);
            this->m_fontableFlags &= ~0x8u;
        }
    }

#endif
}

// VERIFY: 744f3b3c auto
// OFFSET: 0x00487990
void CSimpleFontString::FontObjectUpdated(CSimpleFontStringAttributes& attributes) {
    FRAME_REGION_CHECK();
    attributes.Update(this, this->m_fontableFlags);
}

