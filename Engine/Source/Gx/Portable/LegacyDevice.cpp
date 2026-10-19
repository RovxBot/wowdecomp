#include "LegacyDevice.h"
#include "Engine/Source/Gx/Buffer.h"
#include "Engine/Source/Gx/CGxPool.h"
#include "Engine/Source/Gx/Gx.h"
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <algorithm>
#include <array>
#include <cstring>
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
    // Bounded first adapter: single-mip 2D UI/cache textures, no base-mip aliasing,
    // automatic mips, compressed conversion or hidden rectangle reinterpretation.
    if (target != GxTex_2d || depth || !width || !height || width > m_caps.m_texMaxSize[0] ||
        height > m_caps.m_texMaxSize[0] || flags.m_filter > GxTex_Linear || flags.m_forceMipTracking ||
        flags.m_generateMipMaps || flags.m_maxAnisotropy != 1 || flags.m_bit8 || flags.m_bit14 ||
        flags.m_bit15 || flags.m_unk16 || flags.m_bit31 || !callback)
        throw std::invalid_argument("Unavailable legacy native texture contract");
    if (uint32_t(dataFormat) > GxTex_Rgb565)
        throw std::invalid_argument("Native legacy compressed callback path unavailable");
    Format(dataFormat);
    if (format != dataFormat)
        throw std::invalid_argument("Native legacy texture conversion unavailable");
    auto resource = state->targets.Create({uint32_t(target), width, height, Format(format),
                                           bool(flags.m_renderTarget), 1, bool(flags.m_renderTarget)});
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
    if (flags.m_filter > GxTex_Linear || flags.m_maxAnisotropy != 1 || flags.m_generateMipMaps ||
        flags.m_forceMipTracking || flags.m_bit8 || flags.m_bit14 || flags.m_bit15 || flags.m_unk16 ||
        flags.m_bit31 || bool(flags.m_renderTarget) != item.resource.Spec().renderable)
        throw std::invalid_argument("Unavailable legacy native sampler flags");
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
            callback(GxTex_Latch, tex->m_width, tex->m_height, 0, 0, user, stride, bytes);
            if (!tex->m_flags.m_renderTarget) {
                const uint32_t pixelBytes = tex->m_dataFormat <= GxTex_Argb8888 ? 4 : 2;
                if (!bytes || tex->m_dataFormat > GxTex_Rgb565 ||
                    uint64_t(stride) < uint64_t(tex->m_width) * pixelBytes)
                    throw std::invalid_argument("Unavailable/invalid legacy texture source stride");
                // Legacy callbacks supply a whole-image origin; native Upload accepts
                // the first texel of the dirty region. Borrowed bytes expire at Unlock.
                size_t offset = size_t(rect.minY) * stride + size_t(rect.minX) * pixelBytes;
                size_t extent =
                    size_t(rect.maxY - rect.minY - 1) * stride + size_t(rect.maxX - rect.minX) * pixelBytes;
                state->targets.Upload(item.resource,
                                      {0, 0, uint32_t(rect.minX), uint32_t(rect.minY),
                                       uint32_t(rect.maxX - rect.minX), uint32_t(rect.maxY - rect.minY)},
                                      tex->m_dataFormat, static_cast<const uint8_t *>(bytes) + offset, extent,
                                      stride);
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
    ShaderPackage package;
    if (missing) {
        if (!state->shaderBytes)
            throw std::logic_error("Authenticated shader provider unavailable");
        package = DecodeShaderPackage(state->shaderBytes(member), count);
        // Decode/compile missing permutations before publishing any cache entry.
        for (uint32_t i = 0; i < count; ++i)
            if (!list.Ptr(keys[i].c_str()))
                programs[i] = std::make_unique<GlProgram>(
                    state->context, target == GxSh_Vertex ? ShaderStage::Vertex : ShaderStage::Fragment,
                    package.permutations[i]);
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
            shader->m_valid = shader->m_loaded = 1;
        }
    } catch (...) {
        for (auto *shader : created) {
            state->programs.erase(shader);
            list.Delete(shader);
        }
        throw;
    }
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = list.Ptr(keys[i].c_str());
        ++out[i]->m_refCount;
    }
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
    case GxRs_Lighting:
    case GxRs_Fog:
        if (integer && MasterEnable(which == GxRs_Lighting ? GxMasterEnable_Lighting : GxMasterEnable_Fog))
            Unavailable("Native fixed-function lighting/fog unavailable");
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
    s.transforms.Sync();
    m_projNative = s.transforms.NativeProjection();
}
void LegacyDevice::Sync() {
    Check();
    IRsSync(0);
    NativeSyncTransforms();
    auto &s = *state;
    for (auto which : {GxRs_VertexShader, GxRs_PixelShader}) {
        auto *shader = static_cast<CGxShader *>(m_appRenderStates[which].m_value.m_data.p);
        if (!shader)
            throw std::logic_error("Legacy batch requires explicit UI shaders");
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
    s.Enable(GL_VERTEX_PROGRAM_ARB);
    s.Enable(GL_FRAGMENT_PROGRAM_ARB);
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
    for (int32_t i = 0; i < count; ++i)
        state->buffers.Draw(uint32_t(batches[i].m_primType), batches[i].m_start, batches[i].m_count,
                            m_primIndexBuf != nullptr);
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
        state->targets.Bind();
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
