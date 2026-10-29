#include "GlRenderTargets.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace Gx {
namespace Native {
namespace {
struct Api {
    GlContext &context;
#define BASE(name) decltype(&gl##name) name = reinterpret_cast<decltype(name)>(context.Proc("gl" #name))
    BASE(GetError);
    BASE(GetIntegerv);
    BASE(GenTextures);
    BASE(DeleteTextures);
    BASE(BindTexture);
    BASE(TexImage2D);
    BASE(TexParameteri);
    BASE(TexParameterf);
    BASE(TexSubImage2D);
    BASE(PixelStorei);
    BASE(GetFloatv);
    BASE(DrawBuffer);
    BASE(ReadBuffer);
    BASE(Viewport);
#undef BASE
#define EXT(type, name) type name = reinterpret_cast<type>(context.Proc("gl" #name))
    EXT(PFNGLACTIVETEXTUREARBPROC, ActiveTextureARB);
    EXT(PFNGLCOMPRESSEDTEXIMAGE2DARBPROC, CompressedTexImage2DARB);
    EXT(PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC, CompressedTexSubImage2DARB);
    EXT(PFNGLBINDBUFFERARBPROC, BindBufferARB);
    EXT(PFNGLGENFRAMEBUFFERSEXTPROC, GenFramebuffersEXT);
    EXT(PFNGLBINDFRAMEBUFFEREXTPROC, BindFramebufferEXT);
    EXT(PFNGLDELETEFRAMEBUFFERSEXTPROC, DeleteFramebuffersEXT);
    EXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, FramebufferTexture2DEXT);
    EXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, CheckFramebufferStatusEXT);
    EXT(PFNGLGENRENDERBUFFERSEXTPROC, GenRenderbuffersEXT);
    EXT(PFNGLBINDRENDERBUFFEREXTPROC, BindRenderbufferEXT);
    EXT(PFNGLDELETERENDERBUFFERSEXTPROC, DeleteRenderbuffersEXT);
    EXT(PFNGLRENDERBUFFERSTORAGEEXTPROC, RenderbufferStorageEXT);
    EXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC, FramebufferRenderbufferEXT);
#undef EXT
    explicit Api(GlContext &owner) : context(owner) {}
    GLint Get(GLenum name) const {
        GLint value = 0;
        GetIntegerv(name, &value);
        return value;
    }
    void Check(const char *operation) const {
        GLenum error = GetError();
        if (error != GL_NO_ERROR)
            throw std::runtime_error(std::string(operation) + " GL error=" + std::to_string(error));
    }
};
GLenum Target(uint32_t target) {
    switch (target) {
    case GxTex_2d:
    case GxTex_NonPow2:
        return GL_TEXTURE_2D;
    case GxTex_CubeMap:
        return GL_TEXTURE_CUBE_MAP;
    case GxTex_Rectangle:
        return GL_TEXTURE_RECTANGLE;
    default:
        throw std::invalid_argument("Unsupported Gx render-target texture kind");
    }
}
GLenum Binding(GLenum target) {
    switch (target) {
    case GL_TEXTURE_2D:
        return GL_TEXTURE_BINDING_2D;
    case GL_TEXTURE_CUBE_MAP:
        return GL_TEXTURE_BINDING_CUBE_MAP;
    default:
        return GL_TEXTURE_BINDING_RECTANGLE;
    }
}
// Original storage and upload formats, without native struct overlays. Packed
// 16-bit input is in host uint16 convention (all current native targets are LE).
struct TextureFormat { GLenum internal, format, type; uint32_t bytes, block; };
TextureFormat Format(GlTargetFormat format) {
    switch (format) {
    case GlTargetFormat::Rgba8: return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, 1};
    case GlTargetFormat::Rgba32f: return {GL_RGBA32F, GL_RGBA, GL_FLOAT, 16, 1};
    case GlTargetFormat::Depth24: return {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 4, 1};
    case GlTargetFormat::Rgba4: return {GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV, 2, 1};
    case GlTargetFormat::Rgb5A1: return {GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, 2, 1};
    case GlTargetFormat::Rgb565: return {GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2, 1};
    case GlTargetFormat::Dxt1: return {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0, 8, 4};
    case GlTargetFormat::Dxt3: return {GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, 0, 16, 4};
    case GlTargetFormat::Dxt5: return {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, 0, 16, 4};
    }
    throw std::invalid_argument("Unsupported native texture storage format");
}
TextureFormat DataFormat(uint32_t format) {
    switch (format) {
    case GxTex_Abgr8888: return Format(GlTargetFormat::Rgba8);
    case GxTex_Argb8888: return {GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, 4, 1};
    case GxTex_Argb4444: return Format(GlTargetFormat::Rgba4);
    case GxTex_Argb1555: return Format(GlTargetFormat::Rgb5A1);
    case GxTex_Rgb565: return Format(GlTargetFormat::Rgb565);
    case GxTex_Dxt1: return Format(GlTargetFormat::Dxt1);
    case GxTex_Dxt3: return Format(GlTargetFormat::Dxt3);
    case GxTex_Dxt5: return Format(GlTargetFormat::Dxt5);
    default: throw std::invalid_argument("Unsupported native texture upload format");
    }
}
constexpr GLenum unpackNames[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_PIXELS,
    GL_UNPACK_SKIP_ROWS, GL_UNPACK_IMAGE_HEIGHT, GL_UNPACK_SKIP_IMAGES, GL_UNPACK_SWAP_BYTES, GL_UNPACK_LSB_FIRST};
constexpr GLenum blockNames[] = {GL_UNPACK_COMPRESSED_BLOCK_WIDTH, GL_UNPACK_COMPRESSED_BLOCK_HEIGHT,
    GL_UNPACK_COMPRESSED_BLOCK_DEPTH, GL_UNPACK_COMPRESSED_BLOCK_SIZE};
// Isolate the complete 2D unpack state, including optional compressed block
// storage. This guard restores bindings on allocation/upload exceptions too.
struct TextureState {
    Api& api;
    GLenum target;
    GLint texture, pbo, renderbuffer;
    std::array<GLint, 8> unpack{};
    std::array<GLint, 4> blocks{};
    bool blockStorage;
    TextureState(Api& a, GLenum t) : api(a), target(t), texture(a.Get(Binding(t))),
        pbo(a.Get(GL_PIXEL_UNPACK_BUFFER_BINDING)), renderbuffer(a.Get(GL_RENDERBUFFER_BINDING_EXT)),
        blockStorage(SDL_GL_ExtensionSupported("GL_ARB_compressed_texture_pixel_storage")) {
        for (size_t i = 0; i < unpack.size(); ++i) unpack[i] = a.Get(unpackNames[i]);
        if (blockStorage) for (size_t i = 0; i < blocks.size(); ++i) blocks[i] = a.Get(blockNames[i]);
        a.Check("Query native texture unpack state");
        a.BindBufferARB(GL_PIXEL_UNPACK_BUFFER, 0);
        for (auto name : unpackNames) a.PixelStorei(name, name == GL_UNPACK_ALIGNMENT ? 1 : 0);
        if (blockStorage) for (auto name : blockNames) a.PixelStorei(name, 0);
    }
    ~TextureState() {
        api.BindTexture(target, static_cast<GLuint>(texture));
        api.BindBufferARB(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(pbo));
        api.BindRenderbufferEXT(GL_RENDERBUFFER_EXT, static_cast<GLuint>(renderbuffer));
        for (size_t i = 0; i < unpack.size(); ++i) api.PixelStorei(unpackNames[i], unpack[i]);
        if (blockStorage) for (size_t i = 0; i < blocks.size(); ++i) api.PixelStorei(blockNames[i], blocks[i]);
    }
};
} // namespace

struct GlTarget::Storage {
    std::shared_ptr<Api> api;
    GlTargetSpec spec;
    GlTextureSampling sampling;
    GLuint name = 0, implicitDepth = 0;
    Storage(std::shared_ptr<Api> owner, const GlTargetSpec &value) : api(std::move(owner)), spec(value) {}
    ~Storage() {
        api->context.CheckCurrent();
        if (implicitDepth)
            api->DeleteRenderbuffersEXT(1, &implicitDepth);
        if (name)
            api->DeleteTextures(1, &name);
    }
};
GlTargetSpec GlTarget::Spec() const {
    if (!storage)
        throw std::logic_error("Empty Gx render-target handle");
    return storage->spec;
}

struct GlRenderTargets::State {
    std::shared_ptr<Api> api;
    GLuint framebuffer = 0;
    GlAttachment color, depth;
    GlTargetView view;
    std::vector<GlTarget> samplers;
    explicit State(GlContext &context) : api(std::make_shared<Api>(context)) {
        auto &a = *api;
        a.Check("Before native target initialization");
        GLint units = a.Get(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
        if (units <= 0 || units > 4096)
            throw std::runtime_error("Invalid OpenGL sampler-unit limit");
        samplers.resize(static_cast<size_t>(units));
        a.Check("Query native target limits");
    }
    void Check() const {
        api->context.CheckCurrent();
        if (static_cast<GLuint>(api->Get(GL_FRAMEBUFFER_BINDING_EXT)) != framebuffer ||
            static_cast<GLuint>(api->Get(GL_READ_FRAMEBUFFER_BINDING)) != framebuffer)
            throw std::logic_error("Framebuffer binding changed outside its Gx target owner");
        api->Check("Before native target operation");
    }
    void Validate(const GlAttachment &attachment, GlTargetFormat format) const {
        if (!attachment.texture) {
            if (attachment.plane)
                throw std::invalid_argument("Empty attachment has a cube plane");
            return;
        }
        auto &t = *attachment.texture.storage;
        if (t.api != api)
            throw std::invalid_argument("Render target belongs to another owner/context");
        if (!t.spec.renderable || Format(t.spec.format).block != 1 ||
            ((t.spec.format == GlTargetFormat::Depth24) != (format == GlTargetFormat::Depth24)))
            throw std::invalid_argument("Render-target attachment format/usage mismatch");
        if (attachment.plane >= (t.spec.target == GxTex_CubeMap ? 6u : 1u))
            throw std::out_of_range("Render-target cube plane is out of range");
    }
    void RejectFeedback(const GlTarget &texture) const {
        if (!texture)
            return;
        // Inspect real bindings too: raw GL users cannot bypass the write/read
        // hazard check by omitting a Sample() call. Restore the active unit even
        // when feedback is found before allocating an exception diagnostic.
        auto &a = *api;
        GLint active = a.Get(GL_ACTIVE_TEXTURE);
        bool feedback = false;
        GLenum binding = Binding(Target(texture.storage->spec.target));
        for (size_t i = 0; i < samplers.size(); ++i) {
            a.ActiveTextureARB(GL_TEXTURE0 + static_cast<GLenum>(i));
            if (static_cast<GLuint>(a.Get(binding)) == texture.storage->name)
                feedback = true;
        }
        a.ActiveTextureARB(static_cast<GLenum>(active));
        if (feedback)
            throw std::logic_error("A sampled texture cannot be a render target");
        a.Check("Inspect render-target feedback");
    }
};

GlRenderTargets::GlRenderTargets(GlContext &context) {
    context.CheckCurrent();
    state = std::make_unique<State>(context);
}
GlRenderTargets::~GlRenderTargets() {
    auto &s = *state;
    auto &a = *s.api;
    a.context.CheckCurrent();
    // Owners may coexist for resource construction; only the owner of the
    // current offscreen framebuffer restores it. All retained samples unbind.
    if (s.framebuffer && static_cast<GLuint>(a.Get(GL_FRAMEBUFFER_BINDING_EXT)) == s.framebuffer)
        Bind();
    for (size_t i = 0; i < s.samplers.size(); ++i)
        if (s.samplers[i])
            Sample(static_cast<uint32_t>(i));
    if (s.framebuffer)
        a.DeleteFramebuffersEXT(1, &s.framebuffer);
}

GlTarget GlRenderTargets::Create(const GlTargetSpec &spec) {
    auto &s = *state;
    auto &a = *s.api;
    a.context.CheckCurrent();
    a.Check("Before render-target allocation");
    GLenum target = Target(spec.target);
    auto format = Format(spec.format);
    uint32_t maxLevels = 1;
    for (uint32_t extent = std::max(spec.width, spec.height); extent > 1; extent >>= 1) ++maxLevels;
    if (!spec.width || !spec.height || (spec.target == GxTex_CubeMap && spec.width != spec.height) ||
        !spec.mipLevels || spec.mipLevels > maxLevels ||
        (spec.target == GxTex_Rectangle && (spec.mipLevels != 1 || format.block != 1)) ||
        (spec.format == GlTargetFormat::Depth24 && spec.mipLevels != 1) ||
        (spec.renderable && format.block != 1) ||
        (spec.implicitDepth && (!spec.renderable || spec.format == GlTargetFormat::Depth24)))
        throw std::invalid_argument("Invalid texture dimensions/mips/depth/usage request");
    GLint limit = a.Get(spec.target == GxTex_CubeMap     ? GL_MAX_CUBE_MAP_TEXTURE_SIZE
                        : spec.target == GxTex_Rectangle ? GL_MAX_RECTANGLE_TEXTURE_SIZE
                                                         : GL_MAX_TEXTURE_SIZE);
    if (limit <= 0 || spec.width > static_cast<uint32_t>(limit) ||
        spec.height > static_cast<uint32_t>(limit) ||
        (spec.implicitDepth && (spec.width > static_cast<uint32_t>(a.Get(GL_MAX_RENDERBUFFER_SIZE_EXT)) ||
                                spec.height > static_cast<uint32_t>(a.Get(GL_MAX_RENDERBUFFER_SIZE_EXT)))))
        throw std::out_of_range("Render-target extent exceeds device limits");
    uint64_t total = 0;
    constexpr uint64_t budget = 256 * 1024 * 1024;
    for (uint32_t level = 0; level < spec.mipLevels; ++level) {
        uint64_t w = (std::max(1u, spec.width >> level) + uint64_t(format.block) - 1) / format.block;
        uint64_t h = (std::max(1u, spec.height >> level) + uint64_t(format.block) - 1) / format.block;
        if (w > budget / format.bytes / h)
            throw std::out_of_range("Native texture allocation exceeds 256 MiB budget");
        total += w * h * format.bytes;
        if (total > budget / (spec.target == GxTex_CubeMap ? 6u : 1u))
            throw std::out_of_range("Native texture allocation exceeds 256 MiB budget");
    }
    auto storage = std::make_shared<GlTarget::Storage>(s.api, spec);
    {
        TextureState restore(a, target);
        a.GenTextures(1, &storage->name);
        a.BindTexture(target, storage->name);
        a.TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        a.TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        a.TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        a.TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (target != GL_TEXTURE_RECTANGLE) {
            a.TexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
            a.TexParameteri(target, GL_TEXTURE_MAX_LEVEL, spec.mipLevels - 1);
        }
        if (target == GL_TEXTURE_CUBE_MAP) a.TexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        for (uint32_t plane = 0; plane < (target == GL_TEXTURE_CUBE_MAP ? 6u : 1u); ++plane) {
            auto imageTarget = target == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + plane : target;
            for (uint32_t level = 0; level < spec.mipLevels; ++level) {
                uint32_t w = std::max(1u, spec.width >> level), h = std::max(1u, spec.height >> level);
                uint32_t size = ((w + format.block - 1) / format.block) * ((h + format.block - 1) / format.block) * format.bytes;
                // Sample-only resources start with defined zero bytes at every
                // face/level. Renderable resources retain their clear-before-read contract.
                std::vector<uint8_t> zero;
                if (!spec.renderable) zero.resize(size);
                if (format.block == 4) {
                    a.CompressedTexImage2DARB(imageTarget, level, format.internal, w, h, 0, size, zero.data());
                } else a.TexImage2D(imageTarget, level, format.internal, w, h, 0, format.format, format.type,
                                     zero.empty() ? nullptr : zero.data());
            }
        }
        if (spec.implicitDepth) {
            a.GenRenderbuffersEXT(1, &storage->implicitDepth);
            a.BindRenderbufferEXT(GL_RENDERBUFFER_EXT, storage->implicitDepth);
            a.RenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, spec.width, spec.height);
        }
    }
    a.Check("Allocate native texture storage");
    if (!storage->name || (spec.implicitDepth && !storage->implicitDepth))
        throw std::runtime_error("OpenGL did not allocate a render-target name");
    GlTarget result;
    result.storage = std::move(storage);
    return result;
}

void GlRenderTargets::Upload(const GlTexture& texture, const GlTextureRegion& region, uint32_t dataFormat,
                             const void* data, size_t bytes, uint32_t rowStride) {
    auto& s = *state;
    auto& a = *s.api;
    s.Check();
    if (!texture || texture.storage->api != s.api)
        throw std::invalid_argument("Upload texture belongs to another owner/context");
    auto& t = *texture.storage;
    if (t.spec.format == GlTargetFormat::Depth24)
        throw std::invalid_argument("Native depth texture CPU uploads are unavailable");
    if (texture.storage == s.color.texture.storage || texture.storage == s.depth.texture.storage)
        throw std::logic_error("Cannot update an attached texture");
    auto format = DataFormat(dataFormat), storage = Format(t.spec.format);
    if ((format.block != storage.block) || (format.block == 4 && format.internal != storage.internal))
        throw std::invalid_argument("Compressed texture storage/upload formats must match");
    if (region.level >= t.spec.mipLevels || region.plane >= (t.spec.target == GxTex_CubeMap ? 6u : 1u))
        throw std::out_of_range("Texture upload level/plane exceeds storage");
    uint32_t width = std::max(1u, t.spec.width >> region.level), height = std::max(1u, t.spec.height >> region.level);
    if (!region.width || !region.height || region.x > width || region.width > width - region.x ||
        region.y > height || region.height > height - region.y)
        throw std::out_of_range("Texture update rectangle exceeds storage");
    if (format.block == 4 && ((region.x % 4) || (region.y % 4) ||
        ((region.width % 4) && region.x + region.width != width) ||
        ((region.height % 4) && region.y + region.height != height)))
        throw std::invalid_argument("DXT updates require block-aligned offsets and whole blocks or image edges");
    uint64_t rowBytes = ((uint64_t(region.width) + format.block - 1) / format.block) * format.bytes;
    uint64_t rows = (uint64_t(region.height) + format.block - 1) / format.block;
    uint64_t stride = rowStride ? rowStride : rowBytes;
    if (!data || stride < rowBytes || bytes < (rows - 1) * stride + rowBytes)
        throw std::invalid_argument("Texture upload source span/stride is too small");
    uint64_t size = rowBytes * rows;
    if (size > 256 * 1024 * 1024u || size > uint64_t(std::numeric_limits<GLsizei>::max()))
        throw std::out_of_range("Texture update exceeds native transfer budget");
    // Pack rows before GPU mutation; caller padding never enters GPU storage.
    std::vector<uint8_t> packed;
    const auto* source = static_cast<const uint8_t*>(data);
    if (stride != rowBytes) {
        packed.resize(static_cast<size_t>(size));
        for (uint64_t row = 0; row < rows; ++row)
            std::memcpy(packed.data() + row * rowBytes, source + row * stride, static_cast<size_t>(rowBytes));
        source = packed.data();
    }
    auto target = Target(t.spec.target);
    auto imageTarget = target == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + region.plane : target;
    {
        TextureState restore(a, target);
        a.BindTexture(target, t.name);
        if (format.block == 4)
            a.CompressedTexSubImage2DARB(imageTarget, region.level, region.x, region.y, region.width,
                region.height, format.internal, static_cast<GLsizei>(size), source);
        else a.TexSubImage2D(imageTarget, region.level, region.x, region.y, region.width, region.height,
                             format.format, format.type, source);
    }
    a.Check("Upload native texture rectangle");
}

void GlRenderTargets::Sampling(const GlTexture& texture, const GlTextureSampling& sampling) {
    auto& s = *state;
    auto& a = *s.api;
    s.Check();
    if (!texture || texture.storage->api != s.api)
        throw std::invalid_argument("Sampling texture belongs to another owner/context");
    auto& t = *texture.storage;
    if (sampling.filter >= GxTexFilters_Last || sampling.wrapU > GxTex_Wrap || sampling.wrapV > GxTex_Wrap ||
        !std::isfinite(sampling.anisotropy) || sampling.anisotropy < 1)
        throw std::invalid_argument("Invalid native texture filter/wrap/anisotropy");
    if (t.spec.target == GxTex_Rectangle &&
        (sampling.filter > GxTex_Linear || sampling.wrapU || sampling.wrapV))
        throw std::invalid_argument("Rectangle textures require nonmip filters and clamp");
    bool anisotropic = SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic");
    GLfloat limit = 1;
    if (anisotropic) a.GetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &limit);
    if (sampling.anisotropy > limit || (!anisotropic && sampling.filter == GxTex_Anisotropic))
        throw std::out_of_range("Requested texture anisotropy is unavailable");
    constexpr GLint minFilters[] = {GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST,
        GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR};
    auto target = Target(t.spec.target);
    auto apply = [&](const GlTextureSampling& value) {
        a.TexParameteri(target, GL_TEXTURE_MIN_FILTER, minFilters[value.filter]);
        a.TexParameteri(target, GL_TEXTURE_MAG_FILTER, value.filter == GxTex_Nearest ? GL_NEAREST : GL_LINEAR);
        a.TexParameteri(target, GL_TEXTURE_WRAP_S, value.wrapU ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        a.TexParameteri(target, GL_TEXTURE_WRAP_T, value.wrapV ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        if (anisotropic) a.TexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, value.anisotropy);
    };
    {
        TextureState restore(a, target);
        a.BindTexture(target, t.name);
        try { apply(sampling); a.Check("Set native texture sampling"); }
        catch (...) { apply(t.sampling); throw; }
    }
    t.sampling = sampling;
    a.Check("Restore native texture sampling bindings");
}

void GlRenderTargets::Bind(const GlAttachment &color, const GlAttachment &depth) {
    auto &s = *state;
    auto &a = *s.api;
    s.Check();
    s.Validate(color, GlTargetFormat::Rgba8);
    s.Validate(depth, GlTargetFormat::Depth24);
    if (color.texture && depth.texture &&
        (color.texture.Spec().width != depth.texture.Spec().width ||
         color.texture.Spec().height != depth.texture.Spec().height))
        throw std::invalid_argument("Color and depth render-target dimensions differ");
    s.RejectFeedback(color.texture);
    s.RejectFeedback(depth.texture);
    GlAttachment nextColor = color, nextDepth = depth;
    GlTargetView nextView;
    nextView.offscreen = bool(color.texture) || bool(depth.texture);
    if (nextView.offscreen) {
        auto spec = color.texture ? color.texture.Spec() : depth.texture.Spec();
        nextView.width = static_cast<int>(spec.width);
        nextView.height = static_cast<int>(spec.height);
    } else {
        int w, h;
        a.context.Window().Size(w, h, nextView.width, nextView.height);
    }
    GLint viewport[4];
    a.GetIntegerv(GL_VIEWPORT, viewport);
    GLint previousDraw = a.Get(GL_DRAW_BUFFER), previousRead = a.Get(GL_READ_BUFFER);
    GLuint candidate = 0;
    try {
        if (nextView.offscreen) {
            a.GenFramebuffersEXT(1, &candidate);
            if (!candidate)
                throw std::runtime_error("OpenGL did not allocate a framebuffer name");
            a.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, candidate);
            auto attach = [&](GLenum point, const GlAttachment &attachment) {
                if (!attachment.texture)
                    return;
                auto &t = *attachment.texture.storage;
                GLenum target = Target(t.spec.target);
                a.FramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, point,
                                          target == GL_TEXTURE_CUBE_MAP
                                              ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + attachment.plane
                                              : target,
                                          t.name, 0);
            };
            attach(GL_COLOR_ATTACHMENT0_EXT, color);
            attach(GL_DEPTH_ATTACHMENT_EXT, depth);
            if (!depth.texture && color.texture && color.texture.storage->implicitDepth)
                a.FramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT,
                                             color.texture.storage->implicitDepth);
            a.DrawBuffer(color.texture ? GL_COLOR_ATTACHMENT0_EXT : GL_NONE);
            a.ReadBuffer(color.texture ? GL_COLOR_ATTACHMENT0_EXT : GL_NONE);
            a.Check("Attach native render-target storage");
            GLenum status = a.CheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
            if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
                throw std::runtime_error("Native render-target framebuffer incomplete: " +
                                         std::to_string(status));
        } else {
            a.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            a.DrawBuffer(GL_BACK);
            a.ReadBuffer(GL_BACK);
        }
        a.Viewport(0, 0, nextView.width, nextView.height);
        a.Check("Commit native render-target transition");
    } catch (...) {
        a.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, s.framebuffer);
        a.DrawBuffer(static_cast<GLenum>(previousDraw));
        a.ReadBuffer(static_cast<GLenum>(previousRead));
        a.Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        if (candidate)
            a.DeleteFramebuffersEXT(1, &candidate);
        throw;
    }
    GLuint previous = s.framebuffer;
    s.framebuffer = candidate;
    s.color = std::move(nextColor);
    s.depth = std::move(nextDepth);
    s.view = nextView;
    if (previous)
        a.DeleteFramebuffersEXT(1, &previous);
}

void GlRenderTargets::Sample(uint32_t unit, const GlTarget &texture) {
    auto &s = *state;
    auto &a = *s.api;
    s.Check();
    if (unit >= s.samplers.size())
        throw std::out_of_range("Render-target sampler unit exceeds device limit");
    if (texture && texture.storage->api != s.api)
        throw std::invalid_argument("Sampled render target belongs to another owner/context");
    if (texture && (texture.storage == s.color.texture.storage || texture.storage == s.depth.texture.storage))
        throw std::logic_error("An attached texture cannot be sampled");
    GlTarget retained = texture;
    GLint active = a.Get(GL_ACTIVE_TEXTURE);
    a.ActiveTextureARB(GL_TEXTURE0 + unit);
    const auto &previous = s.samplers[unit];
    if (previous)
        a.BindTexture(Target(previous.Spec().target), 0);
    if (texture)
        a.BindTexture(Target(texture.Spec().target), texture.storage->name);
    a.ActiveTextureARB(static_cast<GLenum>(active));
    a.Check("Bind native render-target sampler");
    s.samplers[unit] = std::move(retained);
}
GlTargetView GlRenderTargets::View() const {
    state->Check();
    return state->view;
}
} // namespace Native
} // namespace Gx
