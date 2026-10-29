#include "LegacyDevice.h"
#include "Engine/Source/Gx/Buffer.h"
#include "Engine/Source/Gx/CGxPool.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/ShaderEffect/Types.h"
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <limits>
#include <map>

namespace Gx {
namespace Native {
namespace {
GlTargetFormat Format(uint32_t value) {
    switch (value) {
    case GxTex_Abgr8888:
    case GxTex_Argb8888:
        return GlTargetFormat::Rgba8;
    case GxTex_Argb4444:
        return GlTargetFormat::Rgba4;
    case GxTex_Argb1555:
        return GlTargetFormat::Rgb5A1;
    case GxTex_Rgb565:
        return GlTargetFormat::Rgb565;
    case GxTex_Dxt1:
        return GlTargetFormat::Dxt1;
    case GxTex_Dxt3:
        return GlTargetFormat::Dxt3;
    case GxTex_Dxt5:
        return GlTargetFormat::Dxt5;
    default:
        throw std::invalid_argument("Unavailable native legacy texture format");
    }
}
uint32_t TextureLevels(uint32_t width, uint32_t height, uint32_t filter) {
    uint32_t levels = 1;
    if (filter >= GxTex_NearestMipNearest)
        for (auto size = std::max(width, height); size > 1; size >>= 1) ++levels;
    return levels;
}
} // namespace
struct LegacyDevice::State {
    GlContext &context;
    ShaderBytes shaderBytes;
    GlRenderTargets targets;
    GlTransforms transforms;
    GlBuffers buffers;
    struct Texture {
        GlTexture resource;
        bool callback = false;
    };
    struct BufferState {
        GlBuffer resource;
        std::unique_ptr<GlBufferWrite> write;
    };
    std::map<CGxTex *, Texture> textures;
    std::map<CGxBuf *, BufferState> storage;
    std::map<CGxShader *, std::unique_ptr<GlProgram>> programs;
    std::map<CGxShader *, uint32_t> filters;
    bool filterDraw = false;
    // One sampler is used in every pass. Scratch stores a single primitive,
    // never a flattened transparent batch; scene depth/blending happen last.
    GlTexture composition;
    std::unique_ptr<GlProgram> compositionTexture;
    std::map<uint32_t, std::unique_ptr<GlProgram>> compositionFirst, compositionResult;
    uint32_t fixedAttributes = 0;
    std::array<CGxStateBom, GxRenderStates_Last> defaults;
#define BASE(name) decltype(&gl##name) name = reinterpret_cast<decltype(name)>(context.Proc("gl" #name))
    BASE(GetError);
    BASE(GetIntegerv);
    BASE(Enable);
    BASE(Disable);
    BASE(BlendFunc);
    BASE(AlphaFunc);
    BASE(DepthFunc);
    BASE(DepthMask);
    BASE(ColorMask);
    BASE(PolygonOffset);
    BASE(ClearColor);
    BASE(Clear);
    BASE(ReadPixels);
    BASE(PixelStorei);
    BASE(GetBooleanv);
    BASE(Color4fv);
    BASE(Materialfv);
    BASE(Materialf);
    BASE(ColorMaterial);
    BASE(Lightfv);
    BASE(Lightf);
    BASE(LightModelfv);
    BASE(LightModeli);
    BASE(Fogf);
    BASE(Fogi);
    BASE(Fogfv);
    BASE(TexEnvi);
    BASE(TexEnvf);
    BASE(TexGeni);
    BASE(MatrixMode);
    BASE(LoadMatrixf);
    BASE(Viewport);
    PFNGLBLENDFUNCSEPARATEPROC BlendFuncSeparate =
        reinterpret_cast<PFNGLBLENDFUNCSEPARATEPROC>(context.Proc("glBlendFuncSeparate"));
    PFNGLGETPROGRAMIVARBPROC GetProgramivARB =
        reinterpret_cast<PFNGLGETPROGRAMIVARBPROC>(context.Proc("glGetProgramivARB"));
    PFNGLBINDPROGRAMARBPROC BindProgramARB =
        reinterpret_cast<PFNGLBINDPROGRAMARBPROC>(context.Proc("glBindProgramARB"));
    PFNGLGETPROGRAMENVPARAMETERFVARBPROC GetProgramEnvParameterfvARB =
        reinterpret_cast<PFNGLGETPROGRAMENVPARAMETERFVARBPROC>(context.Proc("glGetProgramEnvParameterfvARB"));
    PFNGLPROGRAMENVPARAMETER4FVARBPROC ProgramEnvParameter4fvARB =
        reinterpret_cast<PFNGLPROGRAMENVPARAMETER4FVARBPROC>(context.Proc("glProgramEnvParameter4fvARB"));
    PFNGLACTIVETEXTUREARBPROC ActiveTextureARB =
        reinterpret_cast<PFNGLACTIVETEXTUREARBPROC>(context.Proc("glActiveTextureARB"));
    PFNGLBINDBUFFERARBPROC BindBufferARB =
        reinterpret_cast<PFNGLBINDBUFFERARBPROC>(context.Proc("glBindBufferARB"));
#undef BASE
    State(GlContext &owner, ShaderBytes provider)
        : context(owner), shaderBytes(std::move(provider)), targets(owner), transforms(owner, targets),
          buffers(owner) {}
    void Error(const char *operation) {
        if (auto error = GetError())
            throw std::runtime_error(std::string(operation) + " GL error=" + std::to_string(error));
    }
    void Enabled(GLenum cap, bool enabled) {
        if (enabled)
            Enable(cap);
        else
            Disable(cap);
    }
};
void LegacyDevice::Check() const { state->context.CheckCurrent(); }
LegacyDevice::LegacyDevice(GlContext &context, ShaderBytes shaders) {
    context.CheckCurrent();
    if (g_theGxDevicePtr)
        throw std::logic_error("A legacy Gx device is already installed");
    state = std::make_unique<State>(context, std::move(shaders));
    m_api = GxApi_OpenGl;
    // Match 12340 OpenGL ISetCaps. Gxu must not apply the D3D half-pixel
    // offset, which puts nearest atlas samples on unstable texel boundaries.
    m_caps.m_pixelCenterOnEdge = 1;
    m_caps.m_texelCenterOnEdge = 1;
    CapsWindowSize(m_defWindowRect);
    m_curWindowRect = m_defWindowRect;
    m_format = CGxFormat(true, {int32_t(m_defWindowRect.maxX), int32_t(m_defWindowRect.maxY)},
                         CGxFormat::Fmt_Argb8888, CGxFormat::Fmt_Ds248, 0, 0, false, false, false, true, 0);
    m_caps.m_colorFormat = GxCF_rgba;
    m_caps.m_numTmus = 1; // First UI contract; other units fail visibly.
    m_caps.m_numStreams = GxVAs_Last;
    m_caps.m_maxIndex = 65535;
    m_caps.m_shaderTargets[GxSh_Vertex] = 6;
    m_caps.m_shaderTargets[GxSh_Pixel] = 12;
    m_caps.m_shaderConstants[GxSh_Vertex] = m_caps.m_shaderConstants[GxSh_Pixel] = 94;
    GLint maxSize = 0;
    state->GetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    m_caps.m_texMaxSize[GxTex_2d] = uint32_t(maxSize);
    m_caps.m_texTarget[GxTex_2d] = 1;
    for (uint32_t i = GxTex_Abgr8888; i <= GxTex_Rgb565; ++i)
        m_caps.m_texFmt[i] = 1;
    m_caps.m_rttFormat[GxTex_Argb8888] = 1;
    for (uint32_t i = 0; i < GxRenderStates_Last; ++i)
        state->defaults[i] = m_appRenderStates[i].m_value;
    IRsForceUpdate();
    try {
        DeviceCreatePools();
        DeviceCreateStreamBufs();
        // Original Gxu renders directly to the drawable before selecting an FBO.
        // Publish its pixel extent to the native transform owner at installation.
        state->targets.Bind();
    } catch (...) {
        while (auto *pool = m_poolList.Head())
            PoolDestroy(pool);
        m_vertexPool = m_indexPool = nullptr;
        throw;
    }
    m_context = 1;
    m_windowVisible = m_windowFocus = 1;
    // Publish last: CGxTexFlags requires a fully initialized capability owner.
    g_theGxDevicePtr = this;
}
LegacyDevice::~LegacyDevice() {
    Check();
    // Retire native resources while derived virtual dispatch/context still exist.
    state->targets.Bind();
    state->targets.Sample(0);
    state->buffers.VertexFormat(nullptr, 0);
    state->buffers.IndexBuffer();
    while (auto *tex = m_textures.Head())
        TexDestroy(tex);
    while (auto *pool = m_poolList.Head())
        PoolDestroy(pool);
    m_streamBufs[0] = m_streamBufs[1] = nullptr;
    m_vertexPool = m_indexPool = nullptr;
    state->programs.clear();
    for (auto &list : m_shaderList)
        list.Clear();
    state->Disable(GL_VERTEX_PROGRAM_ARB);
    state->Disable(GL_FRAGMENT_PROGRAM_ARB);
    s_uiVertexShader[0] = s_uiVertexShader[1] = s_uiPixelShader = nullptr;
    g_theGxDevicePtr = nullptr;
}
GlBuffer &LegacyDevice::Buffer(CGxBuf *buf) {
    Check();
    if (!buf || !buf->m_pool || !buf->m_size)
        throw std::invalid_argument("Invalid legacy buffer");
    auto &item = state->storage[buf];
    if (item.resource) {
        auto spec = item.resource.Spec();
        if (spec.itemSize != buf->m_itemSize || spec.itemCount != buf->m_itemCount) {
            if (item.write)
                throw std::logic_error("Resizing a locked legacy buffer");
            item.resource = {};
        }
    }
    if (!item.resource)
        item.resource =
            state->buffers.Create({uint32_t(buf->m_pool->m_target), uint32_t(buf->m_pool->m_usage),
                                   buf->m_itemSize, buf->m_itemCount});
    return item.resource;
}
void LegacyDevice::PoolSizeSet(CGxPool *pool, uint32_t size) {
    Check();
    if (!pool || size > uint32_t(INT32_MAX))
        throw std::invalid_argument("Invalid legacy pool size");
    pool->m_size = int32_t(size);
}
void LegacyDevice::NativeBufDestroy(CGxBuf *buf) {
    Check();
    state->buffers.VertexFormat(nullptr, 0);
    state->buffers.IndexBuffer();
    if (m_primVertexBuf == buf)
        m_primVertexBuf = nullptr;
    if (m_primIndexBuf == buf)
        m_primIndexBuf = nullptr;
    for (auto &binding : m_primVertexFormatBuf)
        if (binding == buf)
            binding = nullptr;
    for (auto &locked : m_bufLocked)
        if (locked == buf)
            locked = nullptr;
    for (auto &stream : m_streamBufs)
        if (stream == buf)
            stream = nullptr;
    state->storage.erase(buf); // Aborts any retained staging write first.
}
char *LegacyDevice::BufLock(CGxBuf *buf) {
    auto &resource = Buffer(buf);
    auto &item = state->storage.at(buf);
    auto target = uint32_t(buf->m_pool->m_target);
    if (m_bufLocked[target])
        throw std::logic_error("Legacy buffer target already locked");
    item.write = std::make_unique<GlBufferWrite>(state->buffers.Lock(resource));
    m_bufLocked[target] = buf;
    return static_cast<char *>(item.write->Data());
}
int32_t LegacyDevice::BufUnlock(CGxBuf *buf, uint32_t size) {
    Check();
    auto found = state->storage.find(buf);
    if (found == state->storage.end() || !found->second.write)
        throw std::logic_error("Legacy buffer is not locked");
    found->second.write->Commit(size);
    found->second.write.reset();
    m_bufLocked[buf->m_pool->m_target] = nullptr;
    buf->m_hasData = 1;
    buf->m_dirty = 1;
    return 1;
}
void LegacyDevice::AbortBufferWrite(CGxBuf *buf) {
    Check();
    auto found = state->storage.find(buf);
    if (found == state->storage.end() || !found->second.write)
        throw std::logic_error("Legacy buffer is not locked");
    found->second.write.reset();
    m_bufLocked[buf->m_pool->m_target] = nullptr;
    buf->m_hasData = 0;
}
void LegacyDevice::BufData(CGxBuf *buf, const void *bytes, uint32_t size, uint32_t offset) {
    auto &resource = Buffer(buf);
    state->buffers.Upload(resource, bytes, size, offset);
    buf->m_hasData = 1;
    buf->m_dirty = 1;
}
int32_t LegacyDevice::TexCreate(EGxTexTarget target, uint32_t width, uint32_t height, uint32_t depth,
                                EGxTexFormat format, EGxTexFormat dataFormat, CGxTexFlags flags, void *user,
                                GxTexUserFunc callback, const char *name, CGxTex *&out) {
    Check();
    out = nullptr;
    // Original 2D callback mip order, without base-mip aliasing, automatic mips,
    // compressed conversion or hidden rectangle reinterpretation.
    if (target != GxTex_2d || depth || !width || !height || width > m_caps.m_texMaxSize[0] ||
        height > m_caps.m_texMaxSize[0] || flags.m_filter > GxTex_LinearMipLinear || flags.m_forceMipTracking ||
        flags.m_generateMipMaps || flags.m_maxAnisotropy != 1 || (flags.m_bit8 && !flags.m_renderTarget) || flags.m_bit14 ||
        flags.m_bit15 || flags.m_unk16 || flags.m_bit31 || !callback)
        throw std::invalid_argument("Unavailable legacy native texture contract");
    if (flags.m_renderTarget && flags.m_filter > GxTex_Linear)
        throw std::invalid_argument("Native legacy render targets require a nonmip filter");
    if (uint32_t(dataFormat) > GxTex_Rgb565)
        throw std::invalid_argument("Native legacy compressed callback path unavailable");
    Format(dataFormat);
    if (format != dataFormat)
        throw std::invalid_argument("Native legacy texture conversion unavailable");
    // Original bit 8 requests an implicit depth attachment on a color target.
    // Renderable native storage already retains a depth-24 renderbuffer; reuse
    // that qualified owner without weakening sampled-texture or feedback rules.
    auto resource = state->targets.Create({uint32_t(target), width, height, Format(format),
                                           bool(flags.m_renderTarget), TextureLevels(width, height, flags.m_filter),
                                           bool(flags.m_renderTarget)});
    auto tex = std::make_unique<CGxTex>(target, width, height, depth, format, dataFormat, flags, user,
                                        callback, name);
    auto inserted = state->textures.emplace(tex.get(), State::Texture{std::move(resource)});
    // Stable typed owner address satisfies legacy readiness without aliasing a
    // GLuint through a pointer field. Native storage remains in the owner map.
    tex->m_apiSpecificData = &inserted.first->second;
    tex->m_lastUsedFrame = 0;
    tex->unused_0054 = 0;
    tex->m_char58 = tex->m_char59 = 0;
    tex->m_pad = 0;
    m_textures.LinkToHead(tex.get());
    out = tex.release();
    return 1;
}
void LegacyDevice::ITexMarkAsUpdated(CGxTex *tex) {
    Check();
    auto &item = state->textures.at(tex);
    if (item.callback)
        throw std::logic_error("Reentrant legacy texture callback");
    const auto &flags = tex->m_flags;
    if (flags.m_filter > GxTex_LinearMipLinear || flags.m_maxAnisotropy != 1 || flags.m_generateMipMaps ||
        flags.m_forceMipTracking || (flags.m_bit8 && !item.resource.Spec().implicitDepth) || flags.m_bit14 || flags.m_bit15 || flags.m_unk16 ||
        flags.m_bit31 || bool(flags.m_renderTarget) != item.resource.Spec().renderable)
        throw std::invalid_argument("Unavailable legacy native sampler flags");
    if (TextureLevels(tex->m_width, tex->m_height, flags.m_filter) != item.resource.Spec().mipLevels)
        throw std::invalid_argument("Native legacy sampler change requires a different mip allocation");
    state->targets.Sampling(item.resource,
                            {tex->m_flags.m_filter, tex->m_flags.m_wrapU, tex->m_flags.m_wrapV, 1});
    if (tex->m_needsUpdate) {
        auto rect = tex->m_updateRect;
        if (rect.minX < 0 || rect.minY < 0 || rect.maxX <= rect.minX || rect.maxY <= rect.minY ||
            uint32_t(rect.maxX) > tex->m_width || uint32_t(rect.maxY) > tex->m_height)
            throw std::out_of_range("Legacy texture dirty rectangle");
        uint32_t stride = 0;
        const void *bytes = nullptr;
        auto callback = tex->m_userFunc;
        auto user = tex->m_userArg;
        if (!callback)
            throw std::logic_error("Legacy texture has no source callback");
        item.callback = true;
        bool locked = false;
        try {
            callback(GxTex_Lock, tex->m_width, tex->m_height, 0, 0, user, stride, bytes);
            locked = true;
            for (uint32_t level = 0; level < item.resource.Spec().mipLevels; ++level) {
                const auto width = std::max(1u, tex->m_width >> level);
                const auto height = std::max(1u, tex->m_height >> level);
                callback(GxTex_Latch, width, height, 0, level, user, stride, bytes);
                if (!tex->m_flags.m_renderTarget) {
                    const uint32_t pixelBytes = tex->m_dataFormat <= GxTex_Argb8888 ? 4 : 2;
                    if (!bytes || tex->m_dataFormat > GxTex_Rgb565 ||
                        uint64_t(stride) < uint64_t(width) * pixelBytes)
                        throw std::invalid_argument("Unavailable/invalid legacy texture source stride");
                    // Mipped updates refresh the complete chain, as in original
                    // OpenGL ITexDownload. Single-level caches retain dirty rectangles.
                    const CiRect region = item.resource.Spec().mipLevels == 1 ? rect :
                        CiRect{0, 0, int32_t(height), int32_t(width)};
                    // Legacy callbacks supply a whole-image origin; native Upload accepts
                    // the first texel of the dirty region. Borrowed bytes expire at Unlock.
                    size_t offset = size_t(region.minY) * stride + size_t(region.minX) * pixelBytes;
                    size_t extent =
                        size_t(region.maxY - region.minY - 1) * stride + size_t(region.maxX - region.minX) * pixelBytes;
                    state->targets.Upload(item.resource,
                                          {level, 0, uint32_t(region.minX), uint32_t(region.minY),
                                           uint32_t(region.maxX - region.minX), uint32_t(region.maxY - region.minY)},
                                          tex->m_dataFormat, static_cast<const uint8_t *>(bytes) + offset, extent,
                                          stride);
                }
            }
            locked = false; // Unlock is delivered exactly once even if it throws.
            callback(GxTex_Unlock, tex->m_width, tex->m_height, 0, 0, user, stride, bytes);
            item.callback = false;
        } catch (...) {
            if (locked) {
                try {
                    callback(GxTex_Unlock, tex->m_width, tex->m_height, 0, 0, user, stride, bytes);
                } catch (...) {
                    item.callback = false;
                    throw;
                }
            }
            item.callback = false;
            throw;
        }
    }
    tex->m_needsCreation = tex->m_needsUpdate = tex->m_needsFlagUpdate = 0;
}
void LegacyDevice::TexDestroy(CGxTex *tex) {
    Check();
    auto &item = state->textures.at(tex);
    if (item.callback)
        throw std::logic_error("Destroying a texture from its callback");
    for (auto &target : m_textureTarget)
        if (target.m_texture == tex) {
            DeviceSetRenderTarget(GxBuffers_Color, nullptr, 0);
            break;
        }
    for (uint32_t unit = GxRs_Texture0; unit <= GxRs_Texture15; ++unit) {
        if (m_appRenderStates[unit].m_value.m_data.p == tex || m_hwRenderStates[unit].m_data.p == tex) {
            if (unit == GxRs_Texture0)
                state->targets.Sample(0);
            RsSet(static_cast<EGxRenderState>(unit), nullptr);
            m_hwRenderStates[unit].m_data.p = nullptr;
        }
    }
    if (m_cursorTexture == tex)
        m_cursorTexture = nullptr;
    for (uint32_t i = 0; i < m_pushedStates.Count(); ++i) {
        auto &pushed = m_pushedStates[i];
        if (pushed.m_which >= GxRs_Texture0 && pushed.m_which <= GxRs_Texture15 &&
            pushed.m_value.m_data.p == tex)
            pushed.m_value.m_data.p = nullptr;
    }
    tex->m_apiSpecificData = nullptr;
    state->textures.erase(tex);
    CGxDevice::TexDestroy(tex);
}
int32_t LegacyDevice::TexCopy(CGxTex *dest, const C2iVector &destPos, const C2iVector &sourcePos,
                              const C2iVector &size, uint32_t level, uint32_t plane) {
    Check();
    auto found = state->textures.find(dest);
    if (found == state->textures.end())
        throw std::invalid_argument("Native texture copy destination belongs to another device");
    auto &item = found->second;
    // Original OpenGL TexCopy writes GPU content without invoking a CPU source
    // callback. Admit the reached FFX render textures; copying into an ordinary
    // dirty callback texture needs its own partial-update contract.
    if (item.callback || !item.resource.Spec().renderable || !dest->m_flags.m_renderTarget)
        throw std::logic_error("Native texture copy requires an idle render texture");
    if (destPos.x < 0 || destPos.y < 0 || sourcePos.x < 0 || sourcePos.y < 0 || size.x <= 0 || size.y <= 0)
        throw std::out_of_range("Native texture copy requires positive bounded rectangles");
    state->targets.Copy(item.resource, {level, plane, uint32_t(destPos.x), uint32_t(destPos.y),
                                       uint32_t(size.x), uint32_t(size.y)},
                        uint32_t(sourcePos.x), uint32_t(sourcePos.y));
    dest->m_needsCreation = dest->m_needsUpdate = 0;
    return 1;
}
void LegacyDevice::DeviceSetRenderTarget(EGxBuffer buffer, CGxTex *tex, uint32_t plane) {
    Check();
    if (buffer != GxBuffers_Color || plane)
        throw std::invalid_argument("Native legacy target supports color plane zero");
    if (tex && !tex->m_flags.m_renderTarget)
        throw std::invalid_argument("Legacy texture is not renderable");
    if (tex)
        ITexMarkAsUpdated(tex);
    state->targets.Bind(tex ? GlAttachment{state->textures.at(tex).resource, 0} : GlAttachment{});
    CGxDevice::DeviceSetRenderTarget(buffer, tex, plane);
}
void LegacyDevice::ShaderCreate(CGxShader **out, EGxShTarget target, const char *path, const char *name,
                                uint32_t count) {
    Check();
    if (!out || !path || !name || !count || count > 256 || (target != GxSh_Vertex && target != GxSh_Pixel))
        throw std::invalid_argument("Invalid native legacy shader request");
    std::fill_n(out, count, nullptr);
    std::string member =
        std::string(path) + "\\" + (target == GxSh_Vertex ? "arbvp1" : "arbfp1") + "\\" + name + ".bls";
    std::vector<std::string> keys;
    for (uint32_t i = 0; i < count; ++i)
        keys.push_back(member + ":" + std::to_string(count) + ":" + std::to_string(i));
    auto &list = m_shaderList[target];
    bool missing = false;
    for (auto &key : keys)
        missing |= !list.Ptr(key.c_str());
    std::vector<std::unique_ptr<GlProgram>> programs(count);
    const bool filter = target == GxSh_Pixel && count == 2 &&
        (!std::strcmp(name, "FFXBox4") || !std::strcmp(name, "FFXGauss4"));
    ShaderPackage package;
    if (missing) {
        if (!state->shaderBytes)
            throw std::logic_error("Authenticated shader provider unavailable");
        package = DecodeShaderPackage(state->shaderBytes(member), count);
        // Decode/compile missing permutations before publishing any cache entry.
        for (uint32_t i = 0; i < count; ++i)
            if (!list.Ptr(keys[i].c_str())) {
                auto record = filter ? AliasArbFilterSamplers(package.permutations[i]) : package.permutations[i];
                programs[i] = std::make_unique<GlProgram>(
                    state->context, target == GxSh_Vertex ? ShaderStage::Vertex : ShaderStage::Fragment,
                    record);
            }
    }
    std::vector<CGxShader *> created;
    created.reserve(count);
    try {
        for (uint32_t i = 0; i < count; ++i) {
            auto *shader = list.Ptr(keys[i].c_str());
            if (shader)
                continue;
            shader = list.New(keys[i].c_str(), 0, 0);
            created.push_back(shader);
            shader->m_target = target;
            const auto &record = package.permutations[i];
            shader->m_code.SetCount(uint32_t(record.code.size()));
            std::memcpy(shader->m_code.Ptr(), record.code.data(), record.code.size());
            shader->unk_003C = record.unknown0;
            shader->unk_0040 = record.unknown1;
            shader->unk_0044 = record.unknown2;
            shader->unk_0046 = record.unknown3;
            state->programs.emplace(shader, std::move(programs[i]));
            if (filter) state->filters.emplace(shader, i);
            shader->m_valid = shader->m_loaded = 1;
        }
    } catch (...) {
        for (auto *shader : created) {
            state->programs.erase(shader);
            state->filters.erase(shader);
            list.Delete(shader);
        }
        throw;
    }
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = list.Ptr(keys[i].c_str());
        ++out[i]->m_refCount;
    }
}
void LegacyDevice::RetainShader(CGxShader *shader) {
    Check();
    if (!shader || !state->programs.count(shader))
        throw std::invalid_argument("Foreign legacy shader");
    if (!shader->m_refCount || shader->m_refCount == UINT32_MAX)
        throw std::overflow_error("Legacy shader reference count");
    ++shader->m_refCount;
}
void LegacyDevice::ShaderDestroy(CGxShader *&shader) {
    Check();
    if (!shader)
        return;
    if (!state->programs.count(shader))
        throw std::invalid_argument("Foreign legacy shader");
    if (shader->m_refCount == 1) {
        for (auto which : {GxRs_VertexShader, GxRs_PixelShader}) {
            if (m_appRenderStates[which].m_value.m_data.p == shader)
                RsSet(which, nullptr);
            if (m_hwRenderStates[which].m_data.p == shader)
                m_hwRenderStates[which].m_data.p = nullptr;
        }
        for (uint32_t i = 0; i < m_pushedStates.Count(); ++i) {
            auto &pushed = m_pushedStates[i];
            if ((pushed.m_which == GxRs_VertexShader || pushed.m_which == GxRs_PixelShader) &&
                pushed.m_value.m_data.p == shader)
                pushed.m_value.m_data.p = nullptr;
        }
        state->programs.erase(shader);
        state->filters.erase(shader);
    }
    CGxDevice::ShaderDestroy(shader);
}
void LegacyDevice::IShaderCreate(CGxShader *) {
    Unavailable("Native shader objects require bounded package creation");
}
void LegacyDevice::IShaderReload(CGxShader *, const char *, const char *) {
    Unavailable("Native shader reload unavailable");
}
void LegacyDevice::IRsSendToHw(EGxRenderState which) {
    Check();
    auto &s = *state;
    const auto &value = m_appRenderStates[which].m_value;
    auto integer = value.m_data.i[0];
    switch (which) {
    case GxRs_VertexShader:
    case GxRs_PixelShader:
    case GxRs_Texture0:
    case GxRs_Texture1:
    case GxRs_Texture2:
    case GxRs_Texture3:
    case GxRs_ScissorTest:
        break; // Complete bindings synchronized before each batch.
    case GxRs_DepthTest:
        s.Enabled(GL_DEPTH_TEST, integer && MasterEnable(GxMasterEnable_DepthTest));
        break;
    case GxRs_DepthWrite:
        s.DepthMask(integer && MasterEnable(GxMasterEnable_DepthWrite));
        break;
    case GxRs_ColorWrite: {
        if (uint32_t(integer) > 15)
            throw std::out_of_range("Native color write mask");
        uint32_t mask = MasterEnable(GxMasterEnable_ColorWrite) ? uint32_t(integer) : 0;
        s.ColorMask(mask & 1, mask & 2, mask & 4, mask & 8);
        break;
    }
    case GxRs_DepthFunc: {
        const GLenum funcs[] = {GL_LEQUAL, GL_EQUAL, GL_GEQUAL, GL_LESS};
        if (uint32_t(integer) >= 4)
            throw std::out_of_range("Native depth function");
        s.DepthFunc(funcs[integer]);
        break;
    }
    case GxRs_Culling:
        if (integer < 0 || integer > 1)
            Unavailable("Native culling mode unavailable");
        s.Enabled(GL_CULL_FACE, integer && MasterEnable(GxMasterEnable_Culling));
        break;
    case GxRs_BlendingMode: {
        const GLenum src[] = {
            GL_ONE,       GL_ONE,       GL_SRC_ALPHA,           GL_SRC_ALPHA,           GL_DST_COLOR,
            GL_DST_COLOR, GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA,
            GL_ONE};
        const GLenum dst[] = {
            GL_ZERO, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO, GL_SRC_COLOR, GL_ONE, GL_ONE, GL_ZERO,
            GL_ZERO, GL_ONE};
        if (uint32_t(integer) >= std::size(src))
            throw std::out_of_range("Native blend mode");
        s.Enabled(GL_BLEND, integer > GxBlend_AlphaKey);
        s.BlendFunc(src[integer], dst[integer]);
        break;
    }
    case GxRs_AlphaRef:
        if (integer < 0 || integer > 255)
            throw std::out_of_range("Native alpha reference");
        s.Enabled(GL_ALPHA_TEST, integer > 0);
        s.AlphaFunc(GL_GEQUAL, integer / 255.0f);
        break;
    // Fixed material/light/texture state is delivered as a complete snapshot
    // at submission, including when returning from an ARB UI/FFX pass.
    case GxRs_Lighting:
    case GxRs_Fog:
    case GxRs_FogStart:
    case GxRs_FogEnd:
    case GxRs_FogColor:
    case GxRs_MatDiffuse:
    case GxRs_MatEmissive:
    case GxRs_MatSpecular:
    case GxRs_MatSpecularExp:
    case GxRs_NormalizeNormals:
    case GxRs_ColorMaterial:
    case GxRs_ColorOp0:
    case GxRs_AlphaOp0:
    case GxRs_TexGen0:
    case GxRs_TextureShader0:
    case GxRs_ColorOp1:
    case GxRs_AlphaOp1:
    case GxRs_TexGen1:
    case GxRs_TextureShader1:
        break;
    default:
        if (std::memcmp(&value, &s.defaults[which], sizeof(value)))
            Unavailable("Native legacy render state unavailable");
    }
    s.Error("Legacy render-state delivery");
}
void LegacyDevice::NativeSyncTransforms() {
    Check();
    auto &s = *state;
    // Common legacy matrices/cache are authoritative; the private translator
    // receives a snapshot at submission, with no second caller-visible state.
    for (uint32_t i = 0; i < GxXforms_Last; ++i)
        s.transforms.Set(i, m_xforms[i].m_mtx[m_xforms[i].m_level]);
    s.transforms.Projection(m_projection);
    s.transforms.MasterEnables(m_appMasterEnables);
    s.transforms.Viewport({{m_viewport.m_x.m_l, m_viewport.m_x.m_h, m_viewport.m_y.m_l, m_viewport.m_y.m_h},
                           m_viewport.m_z.m_l,
                           m_viewport.m_z.m_h});
    s.transforms.Scissor(m_appRenderStates[GxRs_ScissorTest].m_value.m_data.i[0] != 0,
                         {m_scissorRect.minX, m_scissorRect.maxX, m_scissorRect.minY, m_scissorRect.maxY});
    const bool fixed = m_appRenderStates[GxRs_VertexShader].m_value.m_data.p == nullptr;
    s.transforms.Sync(fixed, fixed);
    m_projNative = s.transforms.NativeProjection();
}
// Native adaptation of the original OpenGL material/light/combiner submission.
// Uses the existing device state, retained textures and checked buffer owner.
void LegacyDevice::SyncFixed(bool hasColor, uint32_t textureStage) {
    auto &s = *state;
    auto integer = [&](EGxRenderState which) { return m_appRenderStates[which].m_value.m_data.i[0]; };
    auto scalar = [&](EGxRenderState which) {
        float value = m_appRenderStates[which].m_value.m_data.f[0];
        if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite native fixed state");
        return value;
    };
    auto color = [&](EGxRenderState which) {
        const auto &value = reinterpret_cast<const CImVector &>(m_appRenderStates[which].m_value.m_data.u[0]);
        return std::array<float, 4>{{value.r / 255.f, value.g / 255.f, value.b / 255.f, value.a / 255.f}};
    };
    const bool lit = integer(GxRs_Lighting) && MasterEnable(GxMasterEnable_Lighting);
    s.Enabled(GL_LIGHTING, lit);
    s.Enabled(GL_NORMALIZE, integer(GxRs_NormalizeNormals) != 0);
    auto diffuse = color(GxRs_MatDiffuse), emission = color(GxRs_MatEmissive), specular = color(GxRs_MatSpecular);
    const auto exponent = scalar(GxRs_MatSpecularExp);
    if (exponent < 0 || exponent > 128) throw std::out_of_range("Native material specular exponent");
    s.Disable(GL_COLOR_MATERIAL);
    s.Materialfv(GL_FRONT_AND_BACK, GL_AMBIENT, diffuse.data());
    s.Materialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse.data());
    s.Materialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission.data());
    if (exponent == 0) specular = {{0, 0, 0, 0}};
    s.Materialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular.data());
    s.Materialf(GL_FRONT_AND_BACK, GL_SHININESS, exponent);
    if (hasColor && lit) {
        const GLenum modes[] = {GL_AMBIENT_AND_DIFFUSE, GL_SPECULAR, GL_EMISSION};
        auto mode = integer(GxRs_ColorMaterial);
        if (uint32_t(mode) >= std::size(modes)) throw std::out_of_range("Native color material mode");
        s.ColorMaterial(GL_FRONT_AND_BACK, modes[mode]);
        s.Enable(GL_COLOR_MATERIAL);
    } else if (!hasColor) {
        // ShaderEffect folds unlit diffuse into emission. Preserve that color
        // and the diffuse alpha when no vertex color or GL lighting supplies it.
        if (!lit) for (unsigned i = 0; i < 3; ++i) diffuse[i] = std::min(1.f, diffuse[i] + emission[i]);
        s.Color4fv(diffuse.data());
    }
    const float ambient[4] = {0, 0, 0, 0};
    s.LightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    s.LightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);
    s.LightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
    if (lit) {
        // Lights are in view space before the model's world matrix. Apply the
        // original handedness conversion, then restore the submitted modelview.
        GLint mode = 0;
        s.GetIntegerv(GL_MATRIX_MODE, &mode);
        auto view = m_xforms[GxXform_View].m_mtx[m_xforms[GxXform_View].m_level];
        auto modelView = m_xforms[GxXform_World].m_mtx[m_xforms[GxXform_World].m_level] * view;
        auto convert = [](C44Matrix &m) { m.a2 = -m.a2; m.b2 = -m.b2; m.c2 = -m.c2; m.d2 = -m.d2; };
        convert(view); convert(modelView);
        s.MatrixMode(GL_MODELVIEW);
        s.LoadMatrixf(&view.a0);
        for (unsigned i = 0; i < 4; ++i) {
            const auto &light = m_lights[i];
            s.Enabled(GL_LIGHT0 + i, light.m_enable != 0);
            if (!light.m_enable) continue;
            float direction[4] = {light.m_dir.x, light.m_dir.y, light.m_dir.z, light.m_dir.w};
            if (light.m_dir.w == 0) for (float &v : direction) v = -v;
            s.Lightfv(GL_LIGHT0 + i, GL_POSITION, direction);
            auto lightColor = [&](GLenum which, const C3Vector &c) {
                float rgba[4] = {c.x, c.y, c.z, 0};
                s.Lightfv(GL_LIGHT0 + i, which, rgba);
            };
            lightColor(GL_AMBIENT, light.m_ambColor);
            lightColor(GL_DIFFUSE, light.m_dirColor);
            lightColor(GL_SPECULAR, light.m_specColor);
            s.Lightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, light.m_constantAttenuation);
            s.Lightf(GL_LIGHT0 + i, GL_LINEAR_ATTENUATION, light.m_linearAttenuation);
            s.Lightf(GL_LIGHT0 + i, GL_QUADRATIC_ATTENUATION, light.m_quadraticAttenuation);
        }
        s.LoadMatrixf(&modelView.a0);
        s.MatrixMode(static_cast<GLenum>(mode));
    }
    s.Enabled(GL_FOG, integer(GxRs_Fog) && MasterEnable(GxMasterEnable_Fog));
    s.Fogi(GL_FOG_MODE, GL_LINEAR);
    s.Fogf(GL_FOG_START, scalar(GxRs_FogStart));
    s.Fogf(GL_FOG_END, scalar(GxRs_FogEnd));
    const auto fogColor = color(GxRs_FogColor);
    s.Fogfv(GL_FOG_COLOR, fogColor.data());
    s.ActiveTextureARB(GL_TEXTURE0_ARB);
    s.Enabled(GL_TEXTURE_2D, m_appRenderStates[GxRs_Texture0 + textureStage].m_value.m_data.p != nullptr);
    // Same six original s_texEnvOps entries; color and alpha are independent.
    const GLenum combine[] = {GL_MODULATE, GL_MODULATE, GL_ADD, GL_REPLACE, GL_INTERPOLATE, GL_INTERPOLATE};
    s.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    for (bool alpha : {false, true}) {
        const auto op = integer(static_cast<EGxRenderState>((alpha ? GxRs_AlphaOp0 : GxRs_ColorOp0) + textureStage));
        if (uint32_t(op) >= std::size(combine)) throw std::out_of_range("Native texture combiner");
        s.TexEnvi(GL_TEXTURE_ENV, alpha ? GL_COMBINE_ALPHA : GL_COMBINE_RGB, combine[op]);
        s.TexEnvf(GL_TEXTURE_ENV, alpha ? GL_ALPHA_SCALE : GL_RGB_SCALE, op == 1 ? 2.f : 1.f);
        s.TexEnvi(GL_TEXTURE_ENV, alpha ? GL_SOURCE0_ALPHA : GL_SOURCE0_RGB, op == 5 ? GL_TEXTURE : GL_PREVIOUS);
        s.TexEnvi(GL_TEXTURE_ENV, alpha ? GL_SOURCE1_ALPHA : GL_SOURCE1_RGB, op == 5 ? GL_PREVIOUS : GL_TEXTURE);
        s.TexEnvi(GL_TEXTURE_ENV, alpha ? GL_SOURCE2_ALPHA : GL_SOURCE2_RGB, op >= 4 ? GL_PREVIOUS : GL_CONSTANT);
    }
    const auto texgen = integer(static_cast<EGxRenderState>(GxRs_TexGen0 + textureStage));
    if (texgen != GxTexGen_Disabled && texgen != GxTexGen_SphereMap)
        Unavailable("Native fixed texgen mode unavailable");
    const auto textureShader = integer(static_cast<EGxRenderState>(GxRs_TextureShader0 + textureStage));
    if (textureShader < 0 || textureShader > 1)
        Unavailable("Native projected texture matrix unavailable");
    s.Enabled(GL_TEXTURE_GEN_S, texgen != 0);
    s.Enabled(GL_TEXTURE_GEN_T, texgen != 0);
    s.Disable(GL_TEXTURE_GEN_R);
    s.Disable(GL_TEXTURE_GEN_Q);
    if (texgen) {
        s.TexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
        s.TexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
    }
    s.Error("Native CPU model material submission");
}
void LegacyDevice::Sync() {
    Check();
    IRsSync(0);
    NativeSyncTransforms();
    auto &s = *state;
    const bool fixed = m_appRenderStates[GxRs_VertexShader].m_value.m_data.p == nullptr;
    auto *fragment = static_cast<CGxShader *>(m_appRenderStates[GxRs_PixelShader].m_value.m_data.p);
    s.filterDraw = fragment && s.filters.count(fragment);
    if (s.filterDraw) {
        if (!fixed || s.filters.at(fragment) != 0)
            Unavailable("Native FFX filter requires fixed vertices and 2D storage");
        auto *input = static_cast<CGxTex *>(m_appRenderStates[GxRs_Texture0].m_value.m_data.p);
        if (!input || !s.textures.count(input) || s.textures.at(input).resource.Spec().target != GxTex_2d)
            Unavailable("Native FFX filter requires a retained 2D input");
        for (uint32_t i = 0; i < 4; ++i) {
            if (m_appRenderStates[GxRs_Texture0+i].m_value.m_data.p != input)
                Unavailable("Native FFX filter requires four identical logical inputs");
            if (m_appRenderStates[GxRs_TexGen0+i].m_value.m_data.i[0] ||
                m_appRenderStates[GxRs_TextureShader0+i].m_value.m_data.i[0] ||
                m_xforms[GxXform_Tex0+i].m_mtx[m_xforms[GxXform_Tex0+i].m_level] != C44Matrix())
                Unavailable("Native FFX filter requires unmodified original UVs");
        }
    } else if (m_appRenderStates[GxRs_Texture2].m_value.m_data.p ||
               m_appRenderStates[GxRs_Texture3].m_value.m_data.p) {
        Unavailable("Native logical Texture2/3 unavailable outside FFX filters");
    }
    if (!s.filterDraw && fixed != (fragment == nullptr))
        throw std::logic_error("Native mixed fixed/ARB shader pipeline unavailable");
    for (auto which : {GxRs_VertexShader, GxRs_PixelShader}) {
        auto *shader = static_cast<CGxShader *>(m_appRenderStates[which].m_value.m_data.p);
        if (!shader) continue;
        auto &program = *s.programs.at(shader);
        program.Bind();
        auto &shadow = s_shadowConstants[which == GxRs_VertexShader ? 1 : 0];
        if (shadow.dirtyMin <= shadow.dirtyMax) {
            for (uint32_t i = shadow.dirtyMin; i <= shadow.dirtyMax; ++i) {
                float value[4];
                std::copy_n(shadow.constants + i * 4, 4, value);
                program.Constant(i, value);
            }
            shadow.dirtyMin = 255;
            shadow.dirtyMax = 0;
        }
    }
    s.Enabled(GL_VERTEX_PROGRAM_ARB, !fixed);
    s.Enabled(GL_FRAGMENT_PROGRAM_ARB, fragment != nullptr);
    auto *tex = static_cast<CGxTex *>(m_appRenderStates[GxRs_Texture0].m_value.m_data.p);
    if (tex)
        ITexMarkAsUpdated(tex);
    s.targets.Sample(0, tex ? s.textures.at(tex).resource : GlTexture{});
    auto submitted = [&](CGxBuf *buf) -> GlBuffer & {
        if (!buf || !buf->m_hasData)
            throw std::logic_error("Legacy batch buffer has no published data");
        return Buffer(buf);
    };
    std::vector<GlVertexBinding> attributes;
    if (m_primVertexFormat < GxVertexBufferFormats_Last) {
        auto &desc = Buffer::s_vertexBufDesc[m_primVertexFormat];
        for (uint32_t i = 0; i < desc.attribCount; ++i) {
            auto &a = desc.attribs[i];
            attributes.push_back(
                {submitted(m_primVertexBuf), uint32_t(a.m_attrib), a.m_type, a.m_offset, a.m_bufSize});
        }
    } else {
        for (uint32_t i = 0; i < GxVAs_Last; ++i)
            if ((m_primVertexMask & (1u << i)) && m_primVertexFormatBuf[i]) {
                auto &a = m_primVertexFormatAttrib[i];
                attributes.push_back(
                    {submitted(m_primVertexFormatBuf[i]), i, a.m_type, a.m_offset, a.m_bufSize});
            }
    }
    if (fixed && !s.filterDraw) {
        s.fixedAttributes = 0;
        for (const auto &attribute : attributes) s.fixedAttributes |= 1u << attribute.attribute;
        const bool hasColor = std::any_of(attributes.begin(), attributes.end(),
            [](const GlVertexBinding& a) { return a.attribute == GxVA_Color0; });
        SyncFixed(hasColor);
    } else {
        // Fixed texgen and lighting/fog do not execute inside arbitrary ARB
        // programs. Preserve their visible rejection in the shader pipeline.
        if (!s.filterDraw && m_appRenderStates[GxRs_Texture1].m_value.m_data.p)
            Unavailable("Native ARB Texture1 unavailable");
        if (m_appRenderStates[GxRs_TexGen0].m_value.m_data.i[0] ||
            m_appRenderStates[GxRs_TextureShader0].m_value.m_data.i[0])
            Unavailable("Native ARB fixed texgen unavailable");
        if ((m_appRenderStates[GxRs_Lighting].m_value.m_data.i[0] && MasterEnable(GxMasterEnable_Lighting)) ||
            (m_appRenderStates[GxRs_Fog].m_value.m_data.i[0] && MasterEnable(GxMasterEnable_Fog)))
            Unavailable("Native ARB fixed lighting/fog unavailable");
        s.Disable(GL_LIGHTING);
        s.Disable(GL_FOG);
        s.Disable(GL_COLOR_MATERIAL);
        s.Disable(GL_TEXTURE_GEN_S);
        s.Disable(GL_TEXTURE_GEN_T);
        if (s.filterDraw) {
            uint32_t mask = 0;
            for (const auto &attribute : attributes) mask |= 1u << attribute.attribute;
            const uint32_t required = (1u << GxVA_Position) | (1u << GxVA_Color0) |
                (15u << GxVA_TexCoord0);
            if ((mask & required) != required)
                Unavailable("Native FFX filter requires all four original UV streams and color");
            GLint mode = 0;
            s.GetIntegerv(GL_MATRIX_MODE, &mode);
            const C44Matrix identity;
            for (uint32_t unit = 0; unit < 4; ++unit) {
                s.ActiveTextureARB(GL_TEXTURE0_ARB + unit);
                for (auto cap : {GL_TEXTURE_GEN_S, GL_TEXTURE_GEN_T, GL_TEXTURE_GEN_R, GL_TEXTURE_GEN_Q})
                    s.Disable(cap);
                s.MatrixMode(GL_TEXTURE); s.LoadMatrixf(&identity.a0);
            }
            s.ActiveTextureARB(GL_TEXTURE0_ARB);
            s.MatrixMode(GLenum(mode));
        }
    }
    s.buffers.VertexFormat(attributes.data(), attributes.size());
    s.buffers.IndexBuffer(m_primIndexBuf ? submitted(m_primIndexBuf) : GlBuffer{});
}
void LegacyDevice::Draw(CGxBatch *batches, int32_t count) {
    Check();
    if (count < 0 || (count && !batches))
        throw std::invalid_argument("Invalid legacy batch list");
    if (!count)
        return;
    Sync();
    if (!state->filterDraw && m_appRenderStates[GxRs_Texture1].m_value.m_data.p) {
        DrawComposed(batches, count);
        return;
    }
    const bool fixed = m_appRenderStates[GxRs_VertexShader].m_value.m_data.p == nullptr;
    const uint32_t coordinates = state->filterDraw ? 4 : 1;
    // Validate the entire submitted list before the first visible write.
    for (int32_t i = 0; i < count; ++i)
        state->buffers.ValidateDraw(uint32_t(batches[i].m_primType), batches[i].m_start, batches[i].m_count,
                                    m_primIndexBuf != nullptr, fixed, coordinates);
    for (int32_t i = 0; i < count; ++i)
        state->buffers.Draw(uint32_t(batches[i].m_primType), batches[i].m_start, batches[i].m_count,
                            m_primIndexBuf != nullptr, fixed, GxVA_TexCoord0, coordinates);
}
void LegacyDevice::DrawComposed(CGxBatch *batches, int32_t count) {
    auto &s = *state;
    auto integer = [&](EGxRenderState which) { return m_appRenderStates[which].m_value.m_data.i[0]; };
    const bool lit = integer(GxRs_Lighting) && MasterEnable(GxMasterEnable_Lighting);
    const bool fog = integer(GxRs_Fog) && MasterEnable(GxMasterEnable_Fog);
    float fogParameters[4]{};
    if (fog) {
        const float start = m_appRenderStates[GxRs_FogStart].m_value.m_data.f[0];
        const float end = m_appRenderStates[GxRs_FogEnd].m_value.m_data.f[0];
        if (!(end > start)) Unavailable("Native composed fog interval unavailable");
        const double scale = 1.0 / (double(end) - start);
        if (!std::isfinite(scale) || scale > std::numeric_limits<float>::max())
            Unavailable("Native composed fog interval unavailable");
        fogParameters[0] = end; fogParameters[1] = float(scale);
    }
    const auto rgb = integer(GxRs_ColorOp1), alpha = integer(GxRs_AlphaOp1);
    if (uint32_t(rgb) >= GxTexOps_Last || uint32_t(alpha) >= GxTexOps_Last)
        Unavailable("Native composed texture operation unavailable");
    const auto texgen = integer(GxRs_TexGen1);
    if (texgen != GxTexGen_Disabled && texgen != GxTexGen_SphereMap)
        Unavailable("Native composed texgen unavailable");
    if (integer(GxRs_TextureShader1) < 0 || integer(GxRs_TextureShader1) > 1)
        Unavailable("Native composed projected matrix unavailable");
    if (!(s.fixedAttributes & (1u << GxVA_TexCoord0)) ||
        (!texgen && !(s.fixedAttributes & (1u << GxVA_TexCoord1))))
        Unavailable("Native composition requires original texture coordinates");
    const bool hasColor = (s.fixedAttributes & (1u << GxVA_Color0)) != 0;
    const bool indexed = m_primIndexBuf != nullptr;
    for (int32_t i = 0; i < count; ++i) {
        const auto &batch = batches[i];
        if (batch.m_primType != GxPrim_Triangles || batch.m_count % 3)
            Unavailable("Native composition requires complete original triangles");
        s.buffers.ValidateDraw(batch.m_primType, batch.m_start, batch.m_count, indexed, true);
    }
    auto *tex0 = static_cast<CGxTex *>(m_appRenderStates[GxRs_Texture0].m_value.m_data.p);
    auto *tex1 = static_cast<CGxTex *>(m_appRenderStates[GxRs_Texture1].m_value.m_data.p);
    ITexMarkAsUpdated(tex1);
    const auto texture0 = tex0 ? s.textures.at(tex0).resource : GlTexture{};
    const auto texture1 = s.textures.at(tex1).resource;
    if (texture1.Spec().target != GxTex_2d)
        Unavailable("Native composed texture target unavailable");
    // Check feedback against the real scene attachments before changing them.
    s.targets.Sample(0, texture1);
    s.targets.Sample(0, texture0);
    const auto view = s.targets.View();
    if (!s.composition || s.composition.Spec().width != uint32_t(view.width) ||
        s.composition.Spec().height != uint32_t(view.height)) {
        auto scratch = s.targets.Create({GxTex_2d, uint32_t(view.width), uint32_t(view.height), GlTargetFormat::Rgba32f});
        s.targets.Sampling(scratch, {GxTex_Nearest, GxTex_Clamp, GxTex_Clamp, 1});
        s.composition = std::move(scratch);
    }
    auto program = [&](const std::string &source) {
            ShaderRecord record;
            record.code.assign(source.begin(), source.end()); record.code.push_back(0);
            return std::make_unique<GlProgram>(s.context, ShaderStage::Fragment, record);
    };
    if (!s.compositionTexture)
        s.compositionTexture = program("!!ARBfp1.0\nTEX result.color, fragment.texcoord[0], texture[0], 2D;\nEND\n");
    const auto firstRgb = integer(GxRs_ColorOp0), firstAlpha = integer(GxRs_AlphaOp0);
    const uint32_t firstKey = firstRgb + firstAlpha * GxTexOps_Last + (tex0 ? GxTexOps_Last * GxTexOps_Last : 0);
    if (!s.compositionFirst.count(firstKey)) {
        // Fixed vertex processing still supplies original lighting and texgen.
        // An explicit fragment combiner keeps secondary specular and fog out
        // of scratch; both belong after the LAST texture stage.
        std::string source = "!!ARBfp1.0\nTEMP tex, color;\n";
        if (tex0) {
            source += "TEX tex, fragment.texcoord[0], texture[0], 2D;\n";
            auto combine = [&](int op, const char *mask) {
                const std::string output = std::string("color.") + mask;
                // ARB sources permit scalar or four-component swizzles;
                // xyz is a destination mask, not a valid source swizzle.
                const std::string primary = std::string("fragment.color.primary") + (mask[0] == 'w' ? ".w" : "");
                const std::string texture = std::string("tex") + (mask[0] == 'w' ? ".w" : "");
                switch (op) {
                case GxTexOp_Mod: return "MUL_SAT " + output + ", " + primary + ", " + texture + ";\n";
                case GxTexOp_Mod2x: return "MUL " + output + ", " + primary + ", " + texture + ";\nADD_SAT " + output + ", color, color;\n";
                case GxTexOp_Add: return "ADD_SAT " + output + ", " + primary + ", " + texture + ";\n";
                case GxTexOp_PassThru: return "MOV_SAT " + output + ", " + primary + ";\n";
                case GxTexOp_Decal: return "LRP_SAT " + output + ", fragment.color.primary.a, " + primary + ", " + texture + ";\n";
                case GxTexOp_Fade: return "LRP_SAT " + output + ", fragment.color.primary.a, " + texture + ", " + primary + ";\n";
                default: throw std::out_of_range("Native first-stage combiner");
                }
            };
            source += combine(firstRgb, "xyz") + combine(firstAlpha, "w");
            source += "MOV result.color, color;\nEND\n";
        } else source += "MOV_SAT result.color, fragment.color.primary;\nEND\n";
        s.compositionFirst.emplace(firstKey, program(source));
    }
    const uint32_t resultKey = uint32_t(lit) | (uint32_t(fog) << 1);
    if (!s.compositionResult.count(resultKey)) {
        std::string source = "!!ARBfp1.0\nTEMP uv, color, factor;\n"
            "MUL uv, fragment.position, program.env[0];\n"
            "TEX color, uv, texture[0], 2D;\nMOV_SAT color, color;\n";
        if (lit) source += "ADD_SAT color.xyz, color, fragment.color.secondary;\n";
        if (fog) source += "SUB factor.x, program.env[1].x, fragment.fogcoord.x;\n"
            "MUL_SAT factor.x, factor.x, program.env[1].y;\n"
            "LRP color.xyz, factor.x, color, program.env[2];\n";
        source += "MOV result.color, color;\nEND\n";
        s.compositionResult.emplace(resultKey, program(source));
    }
    const auto &target = m_textureTarget[GxBuffers_Color];
    const GlAttachment scene = target.m_texture ? GlAttachment{s.textures.at(target.m_texture).resource, target.m_plane} : GlAttachment{};
    GLint viewport[4]{}, fragmentProgram = 0;
    float constants[3][4]{};
    s.GetIntegerv(GL_VIEWPORT, viewport);
    s.GetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_BINDING_ARB, &fragmentProgram);
    for (unsigned i = 0; i < 3; ++i)
        s.GetProgramEnvParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, i, constants[i]);
    const float inverseExtent[4] = {1.f / view.width, 1.f / view.height, 0, 0};
    const auto &fogColor = reinterpret_cast<const CImVector &>(m_appRenderStates[GxRs_FogColor].m_value.m_data.u[0]);
    const float fogRgba[4] = {fogColor.r / 255.f, fogColor.g / 255.f, fogColor.b / 255.f, fogColor.a / 255.f};
    auto restoreScene = [&] {
        s.targets.Sample(0);
        s.targets.Bind(scene);
        s.Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    };
    auto restore = [&] {
        restoreScene();
        s.targets.Sample(0, texture0);
        s.Disable(GL_FRAGMENT_PROGRAM_ARB);
        s.BindProgramARB(GL_FRAGMENT_PROGRAM_ARB, GLuint(fragmentProgram));
        for (unsigned i = 0; i < 3; ++i)
            s.ProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, i, constants[i]);
        NativeSyncTransforms();
        // Restore the complete fixed material, texgen and color snapshot.
        SyncFixed(hasColor);
        for (auto which : {GxRs_BlendingMode, GxRs_AlphaRef, GxRs_DepthTest, GxRs_DepthWrite, GxRs_ColorWrite})
            IRsSendToHw(which);
        s.Error("Restore composed material state");
    };
    try {
        for (int32_t i = 0; i < count; ++i)
            for (uint32_t offset = 0; offset < batches[i].m_count; offset += 3) {
                const auto start = batches[i].m_start + offset;
                s.targets.Sample(0);
                s.targets.Bind({s.composition, 0});
                // Keep the scene's exact raster coordinates, orientation and
                // culling across all three draws. Bind alone resets viewport.
                s.Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                s.targets.Sample(0, texture0);
                s.Disable(GL_FRAGMENT_PROGRAM_ARB);
                s.Disable(GL_BLEND); s.Disable(GL_ALPHA_TEST); s.Disable(GL_DEPTH_TEST);
                s.DepthMask(GL_FALSE); s.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                // Stage zero retains the original primary color and its clamp.
                // No scratch clear: this primitive overwrites every consumed pixel.
                SyncFixed(hasColor);
                GLint matrixMode = 0;
                s.GetIntegerv(GL_MATRIX_MODE, &matrixMode);
                s.MatrixMode(GL_TEXTURE);
                s.LoadMatrixf(&m_xforms[GxXform_Tex0].m_mtx[m_xforms[GxXform_Tex0].m_level].a0);
                s.MatrixMode(GLenum(matrixMode));
                s.compositionFirst.at(firstKey)->Bind(); s.Enable(GL_FRAGMENT_PROGRAM_ARB);
                s.buffers.Draw(GxPrim_Triangles, start, 3, indexed, true);
                // Stage one samples its own UVs/matrix on the same physical
                // sampler. Multiplication and doubling affect RGB AND alpha.
                // Float scratch prevents an extra RGBA8 quantization; the
                // final fragment program supplies the original stage clamp.
                s.targets.Sample(0, texture1);
                SyncFixed(hasColor, 1);
                s.MatrixMode(GL_TEXTURE);
                s.LoadMatrixf(&m_xforms[GxXform_Tex1].m_mtx[m_xforms[GxXform_Tex1].m_level].a0);
                s.MatrixMode(GLenum(matrixMode));
                s.compositionTexture->Bind(); s.Enable(GL_FRAGMENT_PROGRAM_ARB);
                s.Enable(GL_BLEND);
                // Original six operations, with PREVIOUS alpha retained as
                // the interpolation factor for Decal/Fade in both channels.
                const GLenum source[] = {GL_DST_COLOR, GL_DST_COLOR, GL_ONE, GL_ZERO,
                    GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA};
                const GLenum destination[] = {GL_ZERO, GL_SRC_COLOR, GL_ONE, GL_ONE,
                    GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA};
                const auto alphaSource = alpha < 2 ? GL_DST_ALPHA : source[alpha];
                const auto alphaDestination = alpha == GxTexOp_Mod2x ? GL_SRC_ALPHA : destination[alpha];
                s.BlendFuncSeparate(source[rgb], destination[rgb], alphaSource, alphaDestination);
                s.buffers.Draw(GxPrim_Triangles, start, 3, indexed, true, GxVA_TexCoord1);
                restoreScene();
                s.targets.Sample(0, s.composition);
                auto &resultProgram = *s.compositionResult.at(resultKey);
                resultProgram.Bind(); resultProgram.Constant(0, inverseExtent);
                if (fog) { resultProgram.Constant(1, fogParameters); resultProgram.Constant(2, fogRgba); }
                s.Enable(GL_FRAGMENT_PROGRAM_ARB);
                for (auto which : {GxRs_BlendingMode, GxRs_AlphaRef, GxRs_DepthTest, GxRs_DepthWrite, GxRs_ColorWrite})
                    IRsSendToHw(which);
                s.buffers.Draw(GxPrim_Triangles, start, 3, indexed, true);
            }
    } catch (...) {
        restore();
        throw;
    }
    restore();
}
void LegacyDevice::SceneClear(uint32_t mask, CImVector color) {
    Check();
    if (mask & ~uint32_t(GxBufferBit_Color | GxBufferBit_Depth))
        throw std::invalid_argument("Invalid legacy clear mask");
    NativeSyncTransforms();
    GLboolean depthWrite = 0, colorWrite[4]{};
    state->GetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
    state->GetBooleanv(GL_COLOR_WRITEMASK, colorWrite);
    state->DepthMask(GL_TRUE);
    state->ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state->ClearColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
    state->Clear((mask & GxBufferBit_Color ? GL_COLOR_BUFFER_BIT : 0) |
                 (mask & GxBufferBit_Depth ? GL_DEPTH_BUFFER_BIT : 0));
    state->DepthMask(depthWrite);
    state->ColorMask(colorWrite[0], colorWrite[1], colorWrite[2], colorWrite[3]);
    state->Error("Legacy scene clear");
}
void LegacyDevice::DeviceDestroy() {
    Check();
    CGxDevice::DeviceDestroy(); // SDL/context lifetime remains with the host.
}
void LegacyDevice::ScenePresent() {
    Check();
    state->context.Present();
    CGxDevice::ScenePresent();
}
void LegacyDevice::DeviceReadPixels(CiRect &rect, TSGrowableArray<CImVector> &pixels) {
    Check();
    auto view = state->targets.View();
    if (rect.minX < 0 || rect.minY < 0 || rect.maxX < rect.minX || rect.maxY < rect.minY ||
        rect.maxX > view.width || rect.maxY > view.height)
        throw std::out_of_range("Legacy pixel readback rectangle");
    uint64_t count = uint64_t(rect.maxX - rect.minX) * (rect.maxY - rect.minY);
    if (count > UINT32_MAX)
        throw std::out_of_range("Legacy readback extent");
    pixels.SetCount(uint32_t(count));
    // A CPU pointer must never inherit pack-PBO offset interpretation or hostile
    // row/skip state from a caller. Restore all queried state before diagnostics.
    const GLenum names[] = {GL_PACK_ALIGNMENT,   GL_PACK_ROW_LENGTH,   GL_PACK_SKIP_ROWS,
                            GL_PACK_SKIP_PIXELS, GL_PACK_IMAGE_HEIGHT, GL_PACK_SKIP_IMAGES,
                            GL_PACK_SWAP_BYTES,  GL_PACK_LSB_FIRST};
    std::array<GLint, std::size(names)> saved{};
    GLint pbo = 0;
    state->GetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
    for (size_t i = 0; i < saved.size(); ++i)
        state->GetIntegerv(names[i], &saved[i]);
    state->BindBufferARB(GL_PIXEL_PACK_BUFFER, 0);
    for (auto name : names)
        state->PixelStorei(name, name == GL_PACK_ALIGNMENT ? 1 : 0);
    state->ReadPixels(rect.minX, rect.minY, rect.maxX - rect.minX, rect.maxY - rect.minY, GL_BGRA,
                      GL_UNSIGNED_BYTE, pixels.Ptr());
    for (size_t i = 0; i < saved.size(); ++i)
        state->PixelStorei(names[i], saved[i]);
    state->BindBufferARB(GL_PIXEL_PACK_BUFFER, GLuint(pbo));
    state->Error("Legacy pixel readback");
}
void LegacyDevice::CapsWindowSize(CRect &rect) {
    Check();
    int w, h, pw, ph;
    state->context.Window().Size(w, h, pw, ph);
    rect = {0, 0, float(h), float(w)}; // Logical dimensions; rendering uses target pixels.
}
void LegacyDevice::CapsWindowSizeInScreenCoords(CRect &) {
    Unavailable("Native global screen coordinates unavailable");
}
void *LegacyDevice::DeviceWindow() {
    Check();
    return state->context.Window().Handle();
}
int32_t LegacyDevice::DeviceWM(EGxWM wm, uintptr_t, uintptr_t) {
    Check();
    switch (wm) {
    case GxWM_Size:
        CapsWindowSize(m_defWindowRect);
        if (!m_textureTarget[GxBuffers_Color].m_texture) {
            state->targets.Bind();
            m_curWindowRect = m_defWindowRect;
        }
        m_viewportDirty = 1;
        return 1;
    case GxWM_SetFocus:
        m_windowFocus = 1;
        return 1;
    case GxWM_KillFocus:
        m_windowFocus = 0;
        return 1;
    default:
        Unavailable("Native window message unavailable");
        return 0;
    }
}
} // namespace Native
} // namespace Gx

// No native monitor enumeration/adapter identity is inferred from SDL or PCI.
int32_t CGxDevice::AdapterDesktopMode(CGxMonitorMode &) { return 0; }
int32_t CGxDevice::AdapterMonitorModes(TSGrowableArray<CGxMonitorMode> &) { return 0; }
int32_t CGxDevice::AdapterFormats(EGxApi, TSGrowableArray<CGxFormat> &) { return 0; }
int32_t CGxDevice::DeviceAdapterID(uint16_t &, uint16_t &, uint32_t &, uint32_t &) { return 0; }
int32_t CGxDevice::DeviceAdapterInfer(uint16_t &) { return 0; }

// Native abstract draw-list key functions. The original declarations have no
// definitions in the product tree; provide the empty interface destructors so
// UBSan can retain vptr/typeinfo checks on the actual Gx consumer shutdown route.
GxuDrawListItem::~GxuDrawListItem() = default;
GxuDrawListState::~GxuDrawListState() = default;
