// Original 12340 texture regions, extracted from CSimpleRender.cpp for explicit
// native source selection. Historical tags do not claim new verification.
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Frame/CFramePoint.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Services/SysMessage.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDevice/CGxShader.h"
#include "Engine/Source/Tempest/CRect.h"
#include <cmath>
#include <cstring>
#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Hierarchy.h"
#include "Engine/Source/Frame/Portable/Storage.h"
#include "Engine/Source/FrameXML/Portable/Factories.h"
#endif

#ifdef WOWDECOMP_PORTABLE_RUNTIME
namespace {
static const uint16_t s_quadIndices[6] = {0, 1, 2, 2, 1, 3};
void QueueTextureRegion(CRenderBatch* batch, CSimpleTexture* texture) {
    Frame::Native::RequireScript(texture);
    CStatus status;
    CGxTex* gxTex = TextureGetGxTex(texture->m_texture, (texture->m_textureFlags & 1) ? 0 : 2, &status);
    if (status.m_highestSeverity >= STATUS_ERROR) {
        char error[4096];
        throw std::runtime_error(status.GetErrorStr(error, sizeof(error), STATUS_ERROR, "; "));
    }

    if (!gxTex) {
        texture->NotifyDelayedLoad();
        return;
    }

    if (texture->IsResizePending()) {
        texture->Resize(1);
    }

    uint32_t flags = texture->m_textureFlags;

    if (flags & 2) {
        C2Vector texCoords[4];
        texture->GetTexCoord(texCoords);

        float left = texture->m_positions[0].x;
        float right = texture->m_positions[2].x;
        float bottom = texture->m_positions[1].y;
        float top = texture->m_positions[0].y;

        if (flags & 4) {
            uint32_t width = gxTex->m_width;
            float ndcWidth = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * (right - left));

            if (width && ndcWidth > 0.0f) {
                if (texCoords[0].x != 0.0f) {
                    texCoords[0].x = ndcWidth / width;
                }

                if (texCoords[1].x != 0.0f) {
                    texCoords[1].x = ndcWidth / width;
                }

                if (texCoords[2].x != 0.0f) {
                    texCoords[2].x = ndcWidth / width;
                }

                if (texCoords[3].x != 0.0f) {
                    texCoords[3].x = ndcWidth / width;
                }
            }
        }

        if (texture->m_textureFlags & 8) {
            uint32_t height = gxTex->m_height;
            float ndcHeight = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * (top - bottom));

            if (height && ndcHeight > 0.0f) {
                if (texCoords[0].y != 0.0f) {
                    texCoords[0].y = ndcHeight / height;
                }

                if (texCoords[1].y != 0.0f) {
                    texCoords[1].y = ndcHeight / height;
                }

                if (texCoords[2].y != 0.0f) {
                    texCoords[2].y = ndcHeight / height;
                }

                if (texCoords[3].y != 0.0f) {
                    texCoords[3].y = ndcHeight / height;
                }
            }
        }

        texture->m_textureFlags &= ~2u;
        texture->m_texCoords[0] = texCoords[0];
        texture->m_texCoords[1] = texCoords[1];
        texture->m_texCoords[2] = texCoords[2];
        texture->m_texCoords[3] = texCoords[3];
    }

    uint32_t colorCount = texture->m_colorCount;
    const CImVector* colors = colorCount ? texture->m_colors : nullptr;

    batch->Queue(texture->m_texture, texture->m_alphaMode, 4, texture->m_positions, texture->m_texCoords, colorCount > 1 ? 4 : 0, colors, 6, s_quadIndices, texture->m_shader);
}
}
#endif

// STATIC: 0x00AC0EF4
const char* CSimpleTexture::s_objectTypeName = "Texture";

// STATIC: 0x00AC0EF8
const char* CSimpleTexture::s_imageModePixelShaderNames[2] = { "UI", "Desaturate" };

// STATIC: 0x00AC0F00
EGxTexFilter CSimpleTexture::s_textureFilterMode = GxTex_Linear;

// STATIC: 0x00B47930
int32_t CSimpleTexture::s_count;

// STATIC: 0x00B47934
CGxShader* CSimpleTexture::s_imageModePixelShaders[2];

// STATIC: 0x00B4793C
int32_t CSimpleTexture::s_objectType;

// STATIC: 0x00B4794C
CDataAllocator CSimpleTexture::s_allocator(sizeof(CSimpleTexture), 0x2E0);

// VERIFY: 323fc4f3 auto
// OFFSET: 0x004834C0
void CSimpleTexture::StoreAnimPosition() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    this->CSimpleRegion::StoreAnimPosition();
    this->SetPosition(this->m_rect, this->m_storedPositions);
    memcpy(this->m_positions, this->m_storedPositions, sizeof(this->m_positions));
}

// VERIFY: c6bab0dc auto
// OFFSET: 0x004834F0
void CSimpleTexture::ResetAnimPosition() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    this->CSimpleRegion::ResetAnimPosition();
    this->SetPosition(this->m_rect, this->m_positions);
}

// VERIFY: 88bc13dc auto
// OFFSET: 0x00485140
void CSimpleTexture::Draw(CRenderBatch* batch) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    if (this->m_texture) {
        batch->QueueTexture(this);
    }
}

// INLINED
int32_t CSimpleTexture::GetObjectType() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    if (!CSimpleTexture::s_objectType) {
        CSimpleTexture::s_objectType = ++FrameScript_Object::s_objectTypes;
    }

    return CSimpleTexture::s_objectType;
}

// INLINED
void CSimpleTexture::operator delete(void* ptr) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::FreeRegionStorage(s_allocator, ptr);
#else
    if (ptr) {
        CSimpleTexture::s_allocator.PutData(ptr, nullptr, 0);
    }
#endif
}

// VERIFY: 0a77af56 auto
// OFFSET: 0x00481520
int32_t CSimpleTexture::SetTexture(CTexture* texture) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    if (!this->m_texture || !texture || HandleCompare(reinterpret_cast<HOBJECT>(this->m_texture), reinterpret_cast<HOBJECT>(texture))) {
        CTexture* newTexture = nullptr;

        if (texture) {
            newTexture = reinterpret_cast<CTexture*>(HandleDuplicate(reinterpret_cast<HOBJECT>(texture)));
        }

        if (this->m_texture) {
            HandleClose(reinterpret_cast<HOBJECT>(this->m_texture));
        }

        this->m_texture = newTexture;
        this->OnRegionChanged();
    }

    return 1;
}

// VERIFY: 66dd3b98 auto
// OFFSET: 0x00481590
int32_t CSimpleTexture::SetTexture(const CImVector& color) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    CTexture* texture = TextureCreateSolid(color);

    if (this->m_texture) {
        HandleClose(reinterpret_cast<HOBJECT>(this->m_texture));
    }

    this->m_texture = texture;
    this->OnRegionChanged();

    return 1;
}

// VERIFY: a001c950 auto
// OFFSET: 0x004815E0
int32_t CSimpleTexture::GetTexture(char* buffer, uint32_t bufferLen) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    if (!this->m_texture) {
        return 0;
    }

    const char* filename = TextureGetFilename(this->m_texture);

    if (!filename || !*filename) {
        return 0;
    }

    SStrCopy(buffer, filename, bufferLen);
    return 1;
}

// VERIFY: 289faf79 auto
// OFFSET: 0x00481620
void CSimpleTexture::SetBlendMode(EGxBlend blend) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    uint32_t raw; std::memcpy(&raw, &blend, sizeof(raw));
    if (raw >= GxBlend_ConstantAlpha) throw std::invalid_argument("Invalid native texture blend mode");
#endif
    if (blend != this->m_alphaMode) {
        this->m_alphaMode = blend;
        this->OnRegionChanged();
    }
}

// VERIFY: ced9d67c auto
// OFFSET: 0x00481640
void CSimpleTexture::SetTexCoord(const C2Vector* texCoords) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    this->m_texCoords[0] = texCoords[0];
    this->m_texCoords[1] = texCoords[1];
    this->m_texCoords[2] = texCoords[2];
    this->m_texCoords[3] = texCoords[3];
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    this->OnRegionChanged();
#endif
}

// VERIFY: d3659815 auto
// OFFSET: 0x004816A0
void CSimpleTexture::GetTexCoord(C2Vector* texCoords) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    texCoords[0] = this->m_texCoords[0];
    texCoords[1] = this->m_texCoords[1];
    texCoords[2] = this->m_texCoords[2];
    texCoords[3] = this->m_texCoords[3];
}

// VERIFY: 7b313c73 auto
// OFFSET: 0x00481700
void CSimpleTexture::SetShader(CGxShader* shader) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    if (shader != this->m_shader) {
        this->m_shader = shader;
        this->OnRegionChanged();
    }
}

// VERIFY: 1db02c26 auto
// OFFSET: 0x00481720
CGxTex* CSimpleTexture::GetGxTex() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    return this->m_texture ? TextureGetGxTex(this->m_texture, 1, nullptr) : nullptr;
}

// VERIFY: 09cfc6ac auto
// OFFSET: 0x00481740
void CSimpleTexture::AddAnimTranslation(const C2Vector& translation) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::UnavailableRegionFeature();
#else
    ApplyTranslationAnim(this->m_positions, translation);
#endif
    this->OnRegionChanged();
}

// VERIFY: 0283fe37 auto
// OFFSET: 0x00481770
void CSimpleTexture::AddAnimRotation(FRAMEPOINT origin, const C2Vector& offset, float radians) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::UnavailableRegionFeature();
#else
    ApplyRotationAnim(this->m_positions, origin, offset, radians);
#endif
    this->OnRegionChanged();
}

// VERIFY: 2b060cdb auto
// OFFSET: 0x004817A0
void CSimpleTexture::AddAnimScale(FRAMEPOINT origin, const C2Vector& offset, const C2Vector& scale) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::UnavailableRegionFeature();
#else
    ApplyScaleAnim(this->m_positions, origin, offset, scale);
#endif
    this->OnRegionChanged();
}

// VERIFY: efe353c3 auto
// OFFSET: 0x004817D0
float CSimpleTexture::GetWidth() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    float width = this->CLayoutFrame::GetWidth();

    if (width == 0.0f && this->m_texture) {
        uint32_t textureWidth;

        if (TextureGetDimensions(this->m_texture, &textureWidth, nullptr, 0)) {
            return NDCToDDCWidth(static_cast<float>(textureWidth) / (CoordinateGetAspectCompensation() * 1024.0f));
        }
    }

    return width;
}

// VERIFY: 9aa1bf92 auto
// OFFSET: 0x00481850
float CSimpleTexture::GetHeight() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    float height = this->CLayoutFrame::GetHeight();

    if (height == 0.0f && this->m_texture) {
        uint32_t textureHeight;

        if (TextureGetDimensions(this->m_texture, nullptr, &textureHeight, 0)) {
            return NDCToDDCWidth(static_cast<float>(textureHeight) / (CoordinateGetAspectCompensation() * 1024.0f));
        }
    }

    return height;
}

// VERIFY: d5f393df auto
// OFFSET: 0x004818D0
int32_t CSimpleTexture::IsObjectLoaded() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    if (this->m_texture && !TextureIsLoaded(this->m_texture)) {
        return 0;
    }

    return 1;
}

// VERIFY: bced6855 auto
// OFFSET: 0x00482C60
bool CSimpleTexture::IsA(int32_t type) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    return type == CSimpleTexture::s_objectType || type == CScriptRegion::s_objectType || type == CScriptObject::s_objectType;
}

// VERIFY: 3165fe90 auto
// OFFSET: 0x00482C90
bool CSimpleTexture::IsA(const char* typeName) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    return !SStrCmpI(typeName, CSimpleTexture::s_objectTypeName, STORM_MAX_STR) || !SStrCmpI(typeName, CScriptRegion::s_objectTypeName, STORM_MAX_STR) || !SStrCmpI(typeName, CScriptObject::s_objectTypeName, STORM_MAX_STR);
}

// VERIFY: 465f6c7c auto
// OFFSET: 0x00482CF0
const char* CSimpleTexture::GetObjectTypeName() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    return CSimpleTexture::s_objectTypeName;
}

// VERIFY: 80d103be auto
// OFFSET: 0x00482D00
int32_t CSimpleTexture::GetScriptMetaTable() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    return CSimpleTexture::s_metatable;
}

// VERIFY: ec4a9c07 auto
// OFFSET: [0x00482860, 0x00483010]
CSimpleTexture::~CSimpleTexture() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireDestroyable();
#endif
    if (this->m_texture) {
        HandleClose(reinterpret_cast<HOBJECT>(this->m_texture));
    }

    CSimpleTexture::s_count--;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (!s_count) Frame::Native::SetRegionTextureQueue(nullptr);
#endif
}

// VERIFY: d7f68080 auto
// OFFSET: 0x00483060
void CSimpleTexture::Init() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    for (uint32_t i = 0; i < 2; i++) {
        if (CSimpleTexture::s_imageModePixelShaderNames[i]) {
            g_theGxDevicePtr->ShaderCreate(&CSimpleTexture::s_imageModePixelShaders[i], GxSh_Pixel, "Shaders\\Pixel", CSimpleTexture::s_imageModePixelShaderNames[i], 1);
#ifdef WOWDECOMP_PORTABLE_RUNTIME
            if (!s_imageModePixelShaders[i] || !s_imageModePixelShaders[i]->Valid())
                throw std::runtime_error("Native Frame texture image-mode shader failed");
#endif
        }
    }
}

// VERIFY: 61684332 manual
// OFFSET: 0x004830A0
void CSimpleTexture::Destroy() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    for (int32_t i = 0; i < 2; i++) {
        if (CSimpleTexture::s_imageModePixelShaders[i]) {
            g_theGxDevicePtr->ShaderDestroy(CSimpleTexture::s_imageModePixelShaders[i]);
#ifdef WOWDECOMP_PORTABLE_RUNTIME
            CSimpleTexture::s_imageModePixelShaders[i] = nullptr;
#endif
        }
    }
}

// VERIFY: 3a72f2dc manual
// OFFSET: 0x004830E0
void CSimpleTexture::PostLoadXML(XMLNode* node, CStatus* status) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    CSimpleFrame* parent = this->m_parent;

    if (!parent) {
        return;
    }

    for (uint32_t i = 0; i < FRAMEPOINT_NUMPOINTS; i++) {
        if (this->m_framePoints[i] && !(this->m_framePoints[i]->m_flags & 0x8)) {
            return;
        }
    }

    this->SetAllPoints(parent, 1);
}

// VERIFY: 4323d6db manual
// OFFSET: 0x00483120
void CSimpleTexture::SetRotation(float radians, float centerX, float centerY) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    float angle = radians - 0.78539819f;
    float s = sin(angle);
    float c = cos(angle);

    this->m_texCoords[0].x = s + centerX;
    this->m_texCoords[0].y = centerY - c;
    this->m_texCoords[1].x = centerX - c;
    this->m_texCoords[1].y = centerY - s;
    this->m_texCoords[2].x = c + centerX;
    this->m_texCoords[2].y = s + centerY;
    this->m_texCoords[3].x = centerX - s;
    this->m_texCoords[3].y = centerY + c;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    this->OnRegionChanged();
#endif
}

// VERIFY: 8094d28a auto
// OFFSET: 0x004831D0
void CSimpleTexture::SetTexCoord(const CRect& texRect) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    this->m_texCoords[0].x = texRect.minX;
    this->m_texCoords[0].y = texRect.minY;
    this->m_texCoords[1].x = texRect.minX;
    this->m_texCoords[1].y = texRect.maxY;
    this->m_texCoords[2].x = texRect.maxX;
    this->m_texCoords[2].y = texRect.minY;
    this->m_texCoords[3].x = texRect.maxX;
    this->m_texCoords[3].y = texRect.maxY;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    this->OnRegionChanged();
#endif
}

// VERIFY: 5a6638c2 auto
// OFFSET: 0x00483220
void CSimpleTexture::SetPosition(const CRect& rect, C3Vector* position) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    position[0].x = rect.minX;
    position[0].y = rect.maxY;
    position[0].z = this->m_layoutDepth;
    position[1].x = rect.minX;
    position[1].y = rect.minY;
    position[1].z = this->m_layoutDepth;
    position[2].x = rect.maxX;
    position[2].y = rect.maxY;
    position[2].z = this->m_layoutDepth;
    position[3].x = rect.maxX;
    position[3].y = rect.minY;
    position[3].z = this->m_layoutDepth;
}

// VERIFY: da5e4d9e auto
// OFFSET: 0x00483290
CGxShader* CSimpleTexture::GetImageModePixelShader(TextureImageMode mode) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    uint32_t raw; std::memcpy(&raw, &mode, sizeof(raw));
    if (raw >= 2) throw std::invalid_argument("Invalid native texture image mode");
#endif
    if (CSimpleTexture::s_imageModePixelShaders[mode] && CSimpleTexture::s_imageModePixelShaders[mode]->Valid()) {
        return CSimpleTexture::s_imageModePixelShaders[mode];
    }

    return nullptr;
}

// VERIFY: 99978bcf manual
// OFFSET: 0x004832C0
void CSimpleTexture::OnFrameSizeChanged(const CRect& rect) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    this->CLayoutFrame::OnFrameSizeChanged(rect);

    if (this->m_animating) {
        C3Vector positions[4];
        memcpy(positions, this->m_storedPositions, sizeof(positions));
        this->SetPosition(this->m_rect, this->m_storedPositions);

        this->m_positions[0].x += this->m_storedPositions[0].x - positions[0].x;
        this->m_positions[0].y += this->m_storedPositions[0].y - positions[0].y;
        this->m_positions[0].z += this->m_storedPositions[0].z - positions[0].z;
        this->m_positions[1].x += this->m_storedPositions[1].x - positions[1].x;
        this->m_positions[1].y += this->m_storedPositions[1].y - positions[1].y;
        this->m_positions[1].z += this->m_storedPositions[1].z - positions[1].z;
        this->m_positions[2].x += this->m_storedPositions[2].x - positions[2].x;
        this->m_positions[2].y += this->m_storedPositions[2].y - positions[2].y;
        this->m_positions[2].z += this->m_storedPositions[2].z - positions[2].z;
        this->m_positions[3].x += this->m_storedPositions[3].x - positions[3].x;
        this->m_positions[3].y += this->m_storedPositions[3].y - positions[3].y;
        this->m_positions[3].z += this->m_storedPositions[3].z - positions[3].z;
    } else {
        this->SetPosition(this->m_rect, this->m_positions);
    }

    if ((this->m_textureFlags & 0x4) || (this->m_textureFlags & 0x8)) {
        this->m_textureFlags |= 0x2;
        this->OnRegionChanged();
    }

    if (this->m_parent) {
        this->m_parent->NotifyScrollParent();
    }
}

// VERIFY: a9647663 auto
// OFFSET: 0x00483460
void CSimpleTexture::PreOnAnimUpdate() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
    if (this->m_animating) {
        memcpy(this->m_positions, this->m_storedPositions, sizeof(this->m_positions));

        if (this->m_animAlpha) {
            uint8_t alpha = this->m_animSavedAlpha;
            CImVector color;
            this->GetVertexColor(color);
            color.a = alpha;
            this->SetVertexColor(color);
        }
    }
}

// VERIFY: e87a4320 manual
// OFFSET: 0x00484470
CSimpleTexture::CSimpleTexture(CSimpleFrame* frame, uint32_t drawLayer, int32_t show) : CSimpleRegion(frame, drawLayer, show) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    this->m_textureFlags = 0;
    CSimpleTexture::GetObjectType();
    this->m_positions[0] = this->m_positions[1] = this->m_positions[2] = this->m_positions[3] = C3Vector{};
    this->m_storedPositions[0] = this->m_storedPositions[1] = this->m_storedPositions[2] = this->m_storedPositions[3] = C3Vector{};
    Frame::Native::SetRegionTextureQueue(&QueueTextureRegion);
#endif
    this->m_texture = nullptr;
    this->m_alphaMode = GxBlend_Alpha;
    this->m_shader = CSimpleTexture::s_imageModePixelShaders[ImageMode_Normal];
    this->m_textureFlags &= ~0xFu;
    this->m_shader = CSimpleTexture::GetImageModePixelShader(ImageMode_Normal);
    CSimpleTexture::s_count++;
    this->m_texCoords[0].x = 0.0f;
    this->m_texCoords[0].y = 0.0f;
    this->m_texCoords[1].x = 0.0f;
    this->m_texCoords[1].y = 1.0f;
    this->m_texCoords[2].x = 1.0f;
    this->m_texCoords[2].y = 0.0f;
    this->m_texCoords[3].x = 1.0f;
    this->m_texCoords[3].y = 1.0f;
}

// VERIFY: 9d63630a manual
// OFFSET: 0x004859E0
int32_t CSimpleTexture::SetTexture(const char* fileName, int32_t wrapU, int32_t wrapV, EGxTexFilter filter, TextureImageMode mode) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    GetImageModePixelShader(mode); // Validate before any handle/cache mutation.
    uint32_t rawFilter; std::memcpy(&rawFilter, &filter, sizeof(rawFilter));
    if (rawFilter > GxTex_LinearMipLinear || (wrapU != 0 && wrapU != 1) || (wrapV != 0 && wrapV != 1))
        throw std::invalid_argument("Invalid native Frame texture sampler");
#endif
    if (this->m_texture && fileName && TextureIsSame(this->m_texture, fileName)) {
        return 1;
    }

    CTexture* texture = nullptr;

    if (fileName && *fileName) {
        CStatus status;
        texture = TextureCreate(fileName, CGxTexFlags(filter, wrapU, wrapV, 0, 0, 0, 1), &status, 0x17);

        if (status.m_highestSeverity >= STATUS_ERROR) {
            HandleClose(reinterpret_cast<HOBJECT>(texture));
#ifdef WOWDECOMP_PORTABLE_RUNTIME
            return 0; // Status already records the local image failure; no game SysMessage service.
#else
            SysMsgAdd(&status);
#endif
            return 0;
        }

        this->m_shader = CSimpleTexture::GetImageModePixelShader(mode);
    }

    if (this->m_texture) {
        HandleClose(reinterpret_cast<HOBJECT>(this->m_texture));
    }

    this->m_texture = texture;
    this->OnRegionChanged();

    return 1;
}

// VERIFY: 8453c584 manual
// OFFSET: 0x00485F40
void CSimpleTexture::LoadXML(XMLNode* node, CStatus* status) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegions();
#endif
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    FrameXML::Native::ValidateCreation(node);
    FrameXML::Native::ApplyTemplates(node, [this, status](XMLNode* inherited) { LoadXML(inherited, status); }, false);
#else
    const char* inherits = node->GetAttributeByName("inherits");

    if (inherits && *inherits) {
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

#endif
    this->CScriptRegion::LoadXML(node, status);

    CImVector color;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    color = CImVector(0, 0, 0, 0);
#endif
    int32_t wrapU = 0;
    int32_t wrapV = 0;

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

    const char* horizTile = node->GetAttributeByName("horizTile");

    if (horizTile && *horizTile) {
        this->m_textureFlags ^= (((StringToBOOL(horizTile) != 0) << 2) ^ this->m_textureFlags) & 0x4;
        wrapU = 1;
    }

    const char* vertTile = node->GetAttributeByName("vertTile");

    if (vertTile && *vertTile) {
        this->m_textureFlags ^= (((StringToBOOL(vertTile) != 0) << 3) ^ this->m_textureFlags) & 0x8;
        wrapV = 1;
    }

    for (XMLNode* child = node->m_child; child; child = child->m_next) {
        if (!SStrCmpI(child->m_name.m_str, "TexCoords", STORM_MAX_STR)) {
            float ulx = 0.0f;
            float uly = 0.0f;
            float llx = 0.0f;
            float lly = 1.0f;
            float urx = 1.0f;
            float ury = 0.0f;
            float lrx = 1.0f;
            float lry = 1.0f;
            int32_t valid = 1;

            XMLNode* rectNode = child->GetChildByName("Rect");

            if (rectNode) {
                const char* value = rectNode->GetAttributeByName("ULx");

                if (value && *value) {
                    ulx = SStrToFloat(value);

                    if (ulx < -10000.0f || ulx > 10000.0f) {
                        const char* name = this->GetName();
                        status->Add(STATUS_ERROR, "Texture %s: Invalid rect value (out of range)", name ? name : "<unnamed>");
                        valid = 0;
                    }
                }

                value = rectNode->GetAttributeByName("ULy");

                if (value && *value) {
                    uly = SStrToFloat(value);

                    if (uly < -10000.0f || uly > 10000.0f) {
                        const char* name = this->GetName();
                        status->Add(STATUS_ERROR, "Texture %s: Invalid rect value (out of range)", name ? name : "<unnamed>");
                        valid = 0;
                    }
                }

                value = rectNode->GetAttributeByName("LLx");

                if (value && *value) {
                    llx = SStrToFloat(value);

                    if (llx < -10000.0f || llx > 10000.0f) {
                        const char* name = this->GetName();
                        status->Add(STATUS_ERROR, "Texture %s: Invalid rect value (out of range)", name ? name : "<unnamed>");
                        valid = 0;
                    }
                }

                value = rectNode->GetAttributeByName("LLy");

                if (value && *value) {
                    lly = SStrToFloat(value);

                    if (lly < -10000.0f || lly > 10000.0f) {
                        const char* name = this->GetName();
                        status->Add(STATUS_ERROR, "Texture %s: Invalid rect value (out of range)", name ? name : "<unnamed>");
                        valid = 0;
                    }
                }

                value = rectNode->GetAttributeByName("URx");

                if (value && *value) {
                    urx = SStrToFloat(value);

                    if (urx < -10000.0f || urx > 10000.0f) {
                        const char* name = this->GetName();
                        status->Add(STATUS_ERROR, "Texture %s: Invalid rect value (out of range)", name ? name : "<unnamed>");
                        valid = 0;
                    }
                }

                value = rectNode->GetAttributeByName("URy");

                if (value && *value) {
                    ury = SStrToFloat(value);

                    if (ury < -10000.0f || ury > 10000.0f) {
                        const char* name = this->GetName();
                        status->Add(STATUS_ERROR, "Texture %s: Invalid rect value (out of range)", name ? name : "<unnamed>");
                        valid = 0;
                    }
                }

                value = rectNode->GetAttributeByName("LRx");

                if (value && *value) {
                    lrx = SStrToFloat(value);

                    if (lrx < -10000.0f || lrx > 10000.0f) {
                        const char* name = this->GetName();
                        status->Add(STATUS_ERROR, "Texture %s: Invalid rect value (out of range)", name ? name : "<unnamed>");
                        valid = 0;
                    }
                }

                value = rectNode->GetAttributeByName("LRy");

                if (value && *value) {
                    lry = SStrToFloat(value);

                    if (lry < -10000.0f || lry > 10000.0f) {
                        const char* name = this->GetName();
                        status->Add(STATUS_ERROR, "Texture %s: Invalid rect value (out of range)", name ? name : "<unnamed>");
                        valid = 0;
                    }
                }
            }

            const char* left = child->GetAttributeByName("left");

            if (left && *left) {
                if (this->m_textureFlags & 0x4) {
                    const char* name = this->GetName();
                    status->Add(STATUS_ERROR, "Texture %s: Invalid TexCoords value (horizTile is on)", name ? name : "<unnamed>");
                } else {
                    llx = SStrToFloat(left);
                    ulx = llx;

                    if (llx < -10000.0f || llx > 10000.0f) {
                        const char* name = this->GetName();
                        status->Add(STATUS_ERROR, "Texture %s: Invalid TexCoords value (out of range)", name ? name : "<unnamed>");
                        valid = 0;
                    }
                }
            }

            const char* right = child->GetAttributeByName("right");

            if (right && *right) {
                if (this->m_textureFlags & 0x4) {
                    const char* name = this->GetName();
                    status->Add(STATUS_ERROR, "Texture %s: Invalid TexCoords value (horizTile is on)", name ? name : "<unnamed>");
                } else {
                    lrx = SStrToFloat(right);
                    urx = lrx;

                    if (lrx < -10000.0f || lrx > 10000.0f) {
                        const char* name = this->GetName();
                        status->Add(STATUS_ERROR, "Texture %s: Invalid TexCoords value (out of range)", name ? name : "<unnamed>");
                        valid = 0;
                    }
                }
            }

            const char* top = child->GetAttributeByName("top");

            if (top && *top) {
                if (this->m_textureFlags & 0x8) {
                    const char* name = this->GetName();
                    status->Add(STATUS_ERROR, "Texture %s: Invalid TexCoords value (vertTile is on)", name ? name : "<unnamed>");
                } else {
                    ury = SStrToFloat(top);
                    uly = ury;

                    if (ury < -10000.0f || ury > 10000.0f) {
                        const char* name = this->GetName();
                        status->Add(STATUS_ERROR, "Texture %s: Invalid TexCoords value (out of range)", name ? name : "<unnamed>");
                        valid = 0;
                    }
                }
            }

            const char* bottom = child->GetAttributeByName("bottom");

            if (bottom && *bottom) {
                if (this->m_textureFlags & 0x8) {
                    const char* name = this->GetName();
                    status->Add(STATUS_ERROR, "Texture %s: Invalid TexCoords value (vertTile is on)", name ? name : "<unnamed>");
                } else {
                    lly = SStrToFloat(bottom);
                    lry = lly;

                    if (lly < -10000.0f || lly > 10000.0f) {
                        const char* name = this->GetName();
                        status->Add(STATUS_ERROR, "Texture %s: Invalid TexCoords value (out of range)", name ? name : "<unnamed>");
                        continue;
                    }
                }
            }

            if (valid) {
                this->m_texCoords[0].x = ulx;
                this->m_texCoords[0].y = uly;
                this->m_texCoords[1].x = llx;
                this->m_texCoords[1].y = lly;
                this->m_texCoords[2].x = urx;
                this->m_texCoords[2].y = ury;
                this->m_texCoords[3].x = lrx;
                this->m_texCoords[3].y = lry;

                if (ulx < 0.0f || ulx > 1.0f || llx < 0.0f || llx > 1.0f || urx < 0.0f || urx > 1.0f || lrx < 0.0f || lrx > 1.0f) {
                    wrapU = 1;
                }

                if (uly < 0.0f || uly > 1.0f || lly < 0.0f || lly > 1.0f || ury < 0.0f || ury > 1.0f || lry < 0.0f || lry > 1.0f) {
                    wrapV = 1;
                }
            }
        } else if (!SStrCmpI(child->m_name.m_str, "Color", STORM_MAX_STR)) {
            LoadXML_Color(child, color, status);
            this->SetTexture(color);
        } else if (!SStrCmpI(child->m_name.m_str, "Gradient", STORM_MAX_STR)) {
            ORIENTATION orientation;
            CImVector minColor;
            CImVector maxColor;
            LoadXML_Gradient(child, orientation, minColor, maxColor, status);
            this->SetVertexGradient(orientation, minColor, maxColor);
        }
    }

    const char* file = node->GetAttributeByName("file");

    if (file && *file) {
        if (this->SetTexture(file, wrapU, wrapV, CSimpleTexture::s_textureFilterMode, ImageMode_Normal)) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
            if (color.r || color.g || color.b || color.a) {
#else
            if (*reinterpret_cast<uint32_t*>(&color)) {
#endif
                this->SetVertexColor(color);
            }
        } else {
            const char* name = this->GetName();
            status->Add(STATUS_WARNING, "Texture %s: Unable to load texture file %s", name ? name : "<unnamed>", file);

            CImVector missingColor;
            missingColor.b = 0x00;
            missingColor.g = 0xFF;
            missingColor.r = 0x00;
            missingColor.a = 0xFF;
            this->SetTexture(missingColor);
        }
    }

    const char* alphaMode = node->GetAttributeByName("alphaMode");
    EGxBlend blend;

    if (alphaMode && *alphaMode && StringToBlendMode(alphaMode, blend)) {
        this->SetBlendMode(blend);
    }

    const char* alpha = node->GetAttributeByName("alpha");

    if (alpha && *alpha) {
        float value = SStrToFloat(alpha);
        value = value < 0.0f ? 0.0f : (value >= 1.0f ? 1.0f : value);

        CImVector vertexColor;
        this->GetVertexColor(vertexColor);
        vertexColor.a = static_cast<uint8_t>(value * 255.0f);
        this->SetVertexColor(vertexColor);
    }

    const char* nonBlocking = node->GetAttributeByName("nonBlocking");

    if (nonBlocking && *nonBlocking) {
        this->m_textureFlags ^= ((StringToBOOL(nonBlocking) != 0) ^ this->m_textureFlags) & 0x1;
    }
}
