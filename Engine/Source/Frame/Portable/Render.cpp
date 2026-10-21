#include "Render.h"
#include "Fonts.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/Portable/LegacyDevice.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDevice/CGxShader.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Gxu/GxuFontString.h"
#include "Engine/Source/Services/TextBlock.h"
#include "Engine/Source/Services/Texture.h"
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
Frame::Native::Renderer* renderer = nullptr;
size_t batches = 0;
bool drawing = false;
Frame::Native::RegionTextQueue regionTextQueue = nullptr;
struct Handle {
    HOBJECT value = nullptr;
    Handle() = default;
    explicit Handle(HOBJECT h) : value(HandleDuplicate(h)) {}
    ~Handle() { if (value) HandleClose(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
struct Mesh {
    Handle texture;
    CGxShader* shader = nullptr;
    std::array<C3Vector,4> positions;
    std::array<C2Vector,4> coords;
    std::array<CImVector,4> colors;
    std::array<uint16_t,6> indices;
    explicit Mesh(CTexture* t) : texture(reinterpret_cast<HOBJECT>(t)) {}
    ~Mesh() { if (shader) g_theGxDevicePtr->ShaderDestroy(shader); }
};
struct Text {
    // Reverse member destruction closes the block before its borrowed font.
    Handle font, block;
    Text(HTEXTFONT f, HTEXTBLOCK b) : font(reinterpret_cast<HOBJECT>(f)), block(reinterpret_cast<HOBJECT>(b)) {}
};
}
namespace Frame { namespace Native {
void SetRegionTextQueue(RegionTextQueue queue) { RequireRenderIdle(); regionTextQueue = queue; }
void QueueRegionText(CRenderBatch* batch, CSimpleFontString* string) {
    RequireRenderIdle();
    if (!regionTextQueue) throw std::logic_error("Native Frame font-string regions are not installed");
    regionTextQueue(batch, string);
}
void RequireRenderIdle() {
    Gxu::Native::RequireFontOwner();
    if (drawing) throw std::logic_error("Finish Frame rendering before region mutation");
}
void RequireRenderer() {
    Gxu::Native::RequireFontOwner();
    if (!renderer) throw std::logic_error("Native Frame renderer is not installed");
}
Renderer::Renderer() {
    if (renderer) throw std::logic_error("Only one native Frame renderer may be installed");
    if (!dynamic_cast<Gx::Native::LegacyDevice*>(g_theGxDevicePtr))
        throw std::logic_error("Native Frame renderer requires the qualified Gx adapter");
    renderer = this;
    try { CSimpleRender::Init(); CSimpleRender::s_mergeEnable = 0; }
    catch (...) { CSimpleRender::Destroy(); renderer = nullptr; throw; }
}
void Renderer::CheckCanClose() const {
    RequireRenderer();
    if (renderer != this || batches || drawing)
        throw std::logic_error("Close Frame batches before their renderer");
}
Renderer::~Renderer() {
    try { CheckCanClose(); CSimpleRender::Destroy(); CSimpleRender::s_mergeEnable = 0; renderer = nullptr; }
    catch (...) { std::terminate(); }
}
RenderScope::RenderScope(CRenderBatch* value) : batch(value) {
    if (!batch) throw std::invalid_argument("Null Frame render batch");
    batch->NativeBeginDraw();
}
RenderScope::~RenderScope() { batch->NativeEndDraw(); }
}}
struct CRenderBatch::NativeState {
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<std::unique_ptr<Text>> texts;
};
void CRenderBatch::NativeConstruct() {
    Frame::Native::RequireRenderer();
    if (drawing) throw std::logic_error("Finish Frame rendering before constructing batches");
    m_native = new NativeState;
    ++batches;
}
void CRenderBatch::NativeCheck(bool mutation) const {
    Frame::Native::RequireRenderer();
    if (!m_native) throw std::logic_error("Frame batch is closed");
    if (mutation && drawing) throw std::logic_error("Finish Frame rendering before mutating batches");
}
void CRenderBatch::NativeDestroy() { NativeCheck(); delete m_native; m_native = nullptr; --batches; }
void CRenderBatch::NativeRelease() { m_native->meshes.clear(); m_native->texts.clear(); }
void CRenderBatch::NativeBeginDraw() { NativeCheck(); drawing = true; Frame::Native::SetLayoutRenderActive(true); }
void CRenderBatch::NativeEndDraw() { Frame::Native::SetLayoutRenderActive(false); drawing = false; }
void CRenderBatch::NativeQueue(CTexture* texture, EGxBlend blend, int32_t posCount,
    const C3Vector* positions, const C2Vector* coords, int32_t colorCount, const CImVector* colors,
    int32_t idxCount, const uint16_t* indices, CGxShader* shader) {
    NativeCheck();
    // The first native surface is the original UI quad, with one or four colors.
    if (!texture || !shader || posCount != 4 || idxCount != 6 || !positions || !coords || !indices ||
        (colorCount != 0 && colorCount != 4) || (!colors && colorCount) || uint32_t(blend) >= GxBlend_ConstantAlpha)
        throw std::invalid_argument("Invalid native Frame quad");
    if (m_count >= 32768) throw std::length_error("Native Frame batch limit");
    if (!shader->Valid() || shader->m_target != GxSh_Pixel || !shader->m_refCount || shader->m_refCount == UINT32_MAX)
        throw std::invalid_argument("Frame quad requires a live pixel shader");
    for (size_t i=0; i<4; ++i) {
        if (!std::isfinite(positions[i].x) || !std::isfinite(positions[i].y) || !std::isfinite(positions[i].z) ||
            !std::isfinite(coords[i].x) || !std::isfinite(coords[i].y))
            throw std::invalid_argument("Nonfinite Frame quad");
    }
    for (size_t i=0; i<6; ++i) if (indices[i] >= 4) throw std::out_of_range("Frame quad index");
    auto* gx = TextureGetGxTex(texture, 1, nullptr);
    if (!gx) return;
    auto held = std::make_unique<Mesh>(texture);
    static_cast<Gx::Native::LegacyDevice*>(g_theGxDevicePtr)->RetainShader(shader);
    held->shader = shader;
    for (size_t i=0; i<4; ++i) {
        held->positions[i] = positions[i]; held->coords[i] = coords[i];
        if (colors) held->colors[i] = colors[colorCount ? i : 0];
    }
    for (size_t i=0; i<6; ++i) held->indices[i] = indices[i];
    CSimpleBatchedMesh mesh{};
    mesh.m_texture = texture; mesh.m_textureID = gx; mesh.m_shader = shader; mesh.m_alphaMode = blend;
    mesh.m_posCount = 4; mesh.m_idxCount = 6; mesh.m_colorCount = colorCount;
    mesh.m_position = held->positions.data(); mesh.m_texCoord = held->coords.data();
    mesh.m_indices = held->indices.data(); mesh.m_color = colors ? held->colors.data() : nullptr;
    mesh.m_onAtlas = TextureOnAtlas(texture);
    if (mesh.m_onAtlas) TextureGetAtlasOffsetAndScale(texture, mesh.m_atlasOffset, mesh.m_atlasScale);
    m_native->meshes.push_back(std::move(held));
    try { *m_texturelist.New() = mesh; }
    catch (...) { m_native->meshes.pop_back(); throw; }
    ++m_count;
}
void CRenderBatch::NativeQueueText(HTEXTFONT font, HTEXTBLOCK block) {
    NativeCheck();
    if (!font || !block) throw std::invalid_argument("Frame batch requires font and text handles");
    if (m_count >= 32768) throw std::length_error("Native Frame batch limit");
    auto* string = TextBlockGetStringPtr(block);
    if (!string || string->m_currentFace != TextBlockGetFontPtr(font))
        throw std::invalid_argument("Frame text font does not match its block");
    // One intrusive link cannot belong to two batches, including external Gxu
    // batches. A second enqueue must fail instead of moving another batch's text.
    if (string->m_batchedStringLink.IsLinked())
        throw std::logic_error("Frame text already belongs to a batch");
    auto held = std::make_unique<Text>(font, block);
    m_native->texts.push_back(std::move(held));
    try {
        if (!m_stringbatch) m_stringbatch = GxuFontCreateBatch(false, false);
        if (!m_stringbatch || !GxuFontAddToBatch(m_stringbatch, string))
            throw std::runtime_error("Frame text batch creation failed");
    } catch (...) { m_native->texts.pop_back(); throw; }
    ++m_count;
}
