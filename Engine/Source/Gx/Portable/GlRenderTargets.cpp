#include "GlRenderTargets.h"
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
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
    BASE(DrawBuffer);
    BASE(ReadBuffer);
    BASE(Viewport);
#undef BASE
#define EXT(type, name) type name = reinterpret_cast<type>(context.Proc("gl" #name))
    EXT(PFNGLACTIVETEXTUREARBPROC, ActiveTextureARB);
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
GLenum Target(EGxTexTarget target) {
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
} // namespace

struct GlTarget::Storage {
    std::shared_ptr<Api> api;
    GlTargetSpec spec;
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
        if (t.spec.format != format)
            throw std::invalid_argument("Render-target attachment format mismatch");
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
    if (spec.format != GlTargetFormat::Rgba8 && spec.format != GlTargetFormat::Depth24)
        throw std::invalid_argument("Unsupported Gx render-target format");
    if (!spec.width || !spec.height || (spec.target == GxTex_CubeMap && spec.width != spec.height) ||
        (spec.implicitDepth && spec.format != GlTargetFormat::Rgba8))
        throw std::invalid_argument("Invalid render-target dimensions/depth request");
    GLint limit = a.Get(spec.target == GxTex_CubeMap     ? GL_MAX_CUBE_MAP_TEXTURE_SIZE
                        : spec.target == GxTex_Rectangle ? GL_MAX_RECTANGLE_TEXTURE_SIZE
                                                         : GL_MAX_TEXTURE_SIZE);
    if (limit <= 0 || spec.width > static_cast<uint32_t>(limit) ||
        spec.height > static_cast<uint32_t>(limit) ||
        (spec.implicitDepth && (spec.width > static_cast<uint32_t>(a.Get(GL_MAX_RENDERBUFFER_SIZE_EXT)) ||
                                spec.height > static_cast<uint32_t>(a.Get(GL_MAX_RENDERBUFFER_SIZE_EXT)))))
        throw std::out_of_range("Render-target extent exceeds device limits");
    auto storage = std::make_shared<GlTarget::Storage>(s.api, spec);
    GLint previous = a.Get(Binding(target));
    GLint renderbuffer = a.Get(GL_RENDERBUFFER_BINDING_EXT);
    GLint unpack = a.Get(GL_PIXEL_UNPACK_BUFFER_BINDING);
    a.Check("Query render-target allocation state");
    // Null allocation data must not be interpreted as an offset into a PBO.
    a.BindBufferARB(GL_PIXEL_UNPACK_BUFFER, 0);
    a.GenTextures(1, &storage->name);
    a.BindTexture(target, storage->name);
    a.TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    a.TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    a.TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    a.TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (target != GL_TEXTURE_RECTANGLE) {
        a.TexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
        a.TexParameteri(target, GL_TEXTURE_MAX_LEVEL, 0);
    }
    if (target == GL_TEXTURE_CUBE_MAP)
        a.TexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    bool depth = spec.format == GlTargetFormat::Depth24;
    for (uint32_t plane = 0; plane < (target == GL_TEXTURE_CUBE_MAP ? 6u : 1u); ++plane)
        a.TexImage2D(target == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + plane : target, 0,
                     depth ? GL_DEPTH_COMPONENT24 : GL_RGBA8, spec.width, spec.height, 0,
                     depth ? GL_DEPTH_COMPONENT : GL_RGBA, depth ? GL_UNSIGNED_INT : GL_UNSIGNED_BYTE,
                     nullptr);
    if (spec.implicitDepth) {
        a.GenRenderbuffersEXT(1, &storage->implicitDepth);
        a.BindRenderbufferEXT(GL_RENDERBUFFER_EXT, storage->implicitDepth);
        a.RenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, spec.width, spec.height);
    }
    a.BindTexture(target, static_cast<GLuint>(previous));
    a.BindRenderbufferEXT(GL_RENDERBUFFER_EXT, static_cast<GLuint>(renderbuffer));
    a.BindBufferARB(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpack));
    a.Check("Allocate native render-target storage");
    if (!storage->name || (spec.implicitDepth && !storage->implicitDepth))
        throw std::runtime_error("OpenGL did not allocate a render-target name");
    GlTarget result;
    result.storage = std::move(storage);
    return result;
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
