#include "Engine/Source/Gx/Portable/GlRenderTargets.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Storm/H/storm.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace Gx::Native;
namespace {
void Require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Exception, class F> void Reject(F function) {
    bool rejected = false;
    try {
        function();
    } catch (const Exception &) {
        rejected = true;
    }
    Require(rejected, "Invalid render-target operation was accepted");
}
struct Checks {
    GlContext &context;
#define BASE(name) decltype(&gl##name) name = reinterpret_cast<decltype(name)>(context.Proc("gl" #name))
    BASE(GetError);
    BASE(GetIntegerv);
    BASE(ClearColor);
    BASE(ClearDepth);
    BASE(Clear);
    BASE(ReadPixels);
    BASE(Disable);
    BASE(IsTexture);
    BASE(GenTextures);
    BASE(DeleteTextures);
    BASE(BindTexture);
    BASE(TexImage2D);
#undef BASE
#define EXT(type, name) type name = reinterpret_cast<type>(context.Proc("gl" #name))
    EXT(PFNGLACTIVETEXTUREARBPROC, ActiveTextureARB);
    EXT(PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC, GetFramebufferAttachmentParameterivEXT);
    EXT(PFNGLISFRAMEBUFFEREXTPROC, IsFramebufferEXT);
    EXT(PFNGLISRENDERBUFFEREXTPROC, IsRenderbufferEXT);
    EXT(PFNGLGENRENDERBUFFERSEXTPROC, GenRenderbuffersEXT);
    EXT(PFNGLBINDRENDERBUFFEREXTPROC, BindRenderbufferEXT);
    EXT(PFNGLDELETERENDERBUFFERSEXTPROC, DeleteRenderbuffersEXT);
    EXT(PFNGLGENBUFFERSARBPROC, GenBuffersARB);
    EXT(PFNGLBINDBUFFERARBPROC, BindBufferARB);
    EXT(PFNGLBUFFERDATAARBPROC, BufferDataARB);
    EXT(PFNGLDELETEBUFFERSARBPROC, DeleteBuffersARB);
#undef EXT
    explicit Checks(GlContext &owner) : context(owner) {}
    GLint Get(GLenum name) {
        GLint value = 0;
        GetIntegerv(name, &value);
        return value;
    }
    GLint Attachment(GLenum point, GLenum name) {
        GLint value = 0;
        GetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, point, name, &value);
        return value;
    }
    void Clean() { Require(GetError() == GL_NO_ERROR, "Render-target fixture GL error"); }
    void View(GlRenderTargets &targets, int width, int height, bool offscreen) {
        auto view = targets.View();
        Require(view.width == width && view.height == height && view.offscreen == offscreen,
                "Target extent was not committed");
        GLint viewport[4];
        GetIntegerv(GL_VIEWPORT, viewport);
        Require(viewport[0] == 0 && viewport[1] == 0 && viewport[2] == width && viewport[3] == height,
                "Render-target viewport has wrong dimensions");
        Clean();
    }
    void Color(int red, int green, int blue) {
        std::array<uint8_t, 4> value{};
        ReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, value.data());
        Require(std::abs(int(value[0]) - red) <= 1 && std::abs(int(value[1]) - green) <= 1 &&
                    std::abs(int(value[2]) - blue) <= 1,
                "Render-target color pixel mismatch");
        Clean();
    }
    void Depth(float expected) {
        float value = 0;
        ReadPixels(1, 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &value);
        Require(std::abs(value - expected) < 0.00001f, "Render-target depth pixel mismatch");
        Clean();
    }
    void AllocationState(GlRenderTargets &targets) {
        GLuint texture = 0, renderbuffer = 0, buffer = 0;
        GenTextures(1, &texture);
        GenRenderbuffersEXT(1, &renderbuffer);
        GenBuffersARB(1, &buffer);
        ActiveTextureARB(GL_TEXTURE3);
        BindTexture(GL_TEXTURE_2D, texture);
        BindRenderbufferEXT(GL_RENDERBUFFER_EXT, renderbuffer);
        BindBufferARB(GL_PIXEL_UNPACK_BUFFER, buffer);
        const uint8_t byte = 0;
        BufferDataARB(GL_PIXEL_UNPACK_BUFFER, 1, &byte, GL_STATIC_DRAW);
        // A one-byte PBO cannot back the requested texture. Creation must use
        // null storage allocation and then restore all caller bindings.
        auto created = targets.Create({GxTex_2d, 16, 16, GlTargetFormat::Rgba8, true});
        Require(Get(GL_ACTIVE_TEXTURE) == GL_TEXTURE3 &&
                    static_cast<GLuint>(Get(GL_TEXTURE_BINDING_2D)) == texture &&
                    static_cast<GLuint>(Get(GL_RENDERBUFFER_BINDING_EXT)) == renderbuffer &&
                    static_cast<GLuint>(Get(GL_PIXEL_UNPACK_BUFFER_BINDING)) == buffer,
                "Render-target allocation changed caller GL bindings");
        BindBufferARB(GL_PIXEL_UNPACK_BUFFER, 0);
        BindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
        BindTexture(GL_TEXTURE_2D, 0);
        DeleteBuffersARB(1, &buffer);
        DeleteRenderbuffersEXT(1, &renderbuffer);
        DeleteTextures(1, &texture);
        ActiveTextureARB(GL_TEXTURE0);
        Clean();
        std::cout << "target-allocation-binding-and-pbo-restoration=passed\n";
    }
    void Transitions(GlRenderTargets &targets) {
        auto color = targets.Create({GxTex_2d, 32, 32, GlTargetFormat::Rgba8, true});
        auto depth = targets.Create({GxTex_2d, 32, 32, GlTargetFormat::Depth24});
        auto resized = targets.Create({GxTex_NonPow2, 47, 19});
        targets.Bind({color});
        View(targets, 32, 32, true);
        Require(Attachment(GL_DEPTH_ATTACHMENT_EXT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT) ==
                    GL_RENDERBUFFER_EXT,
                "Color target lacks its implicit depth renderbuffer");
        GLuint implicit = Attachment(GL_DEPTH_ATTACHMENT_EXT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT);
        ClearColor(1, 0, 0, 1);
        ClearDepth(0.25);
        Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Color(255, 0, 0);
        Depth(0.25f);
        targets.Bind({color}, {depth});
        Require(Attachment(GL_DEPTH_ATTACHMENT_EXT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT) == GL_TEXTURE,
                "Explicit depth did not replace implicit depth");
        ClearDepth(0.75);
        Clear(GL_DEPTH_BUFFER_BIT);
        Depth(0.75f);
        targets.Bind({color});
        Require(static_cast<GLuint>(Attachment(GL_DEPTH_ATTACHMENT_EXT,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT)) == implicit,
                "Removing explicit depth did not restore implicit depth");
        Depth(0.25f);
        Color(255, 0, 0);
        targets.Bind({resized});
        View(targets, 47, 19, true);
        Require(Attachment(GL_DEPTH_ATTACHMENT_EXT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT) == GL_NONE,
                "Replacement color target retained stale depth");
        targets.Bind({}, {depth});
        View(targets, 32, 32, true);
        Require(Get(GL_DRAW_BUFFER) == GL_NONE && Get(GL_READ_BUFFER) == GL_NONE,
                "Depth-only target has a color read/draw buffer");
        Depth(0.75f);
        ClearDepth(0.5);
        Clear(GL_DEPTH_BUFFER_BIT);
        Depth(0.5f);
        targets.Bind({color}, {depth});
        Depth(0.5f);
        auto rectangle = targets.Create({GxTex_Rectangle, 13, 7});
        targets.Bind({rectangle});
        View(targets, 13, 7, true);
        ClearColor(0, 1, 0, 1);
        Clear(GL_COLOR_BUFFER_BIT);
        Color(0, 255, 0);
        targets.Bind({color});
        Color(255, 0, 0);
        std::cout << "target-color-depth-replacement-depth-only-npot-rectangle=passed\n";
    }
    void Cube(GlRenderTargets &targets) {
        auto color = targets.Create({GxTex_CubeMap, 16, 16});
        auto depth = targets.Create({GxTex_CubeMap, 16, 16, GlTargetFormat::Depth24});
        for (uint32_t plane = 0; plane < 6; ++plane) {
            targets.Bind({color, plane}, {depth, plane});
            Require(
                Attachment(GL_COLOR_ATTACHMENT0_EXT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT) ==
                    static_cast<GLint>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + plane),
                "Wrong cube attachment face");
            ClearColor(float(plane) / 5, 0, 1, 1);
            ClearDepth(float(plane + 1) / 8);
            Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        for (uint32_t plane = 0; plane < 6; ++plane) {
            targets.Bind({color, plane}, {depth, plane});
            Color(int(plane) * 51, 0, 255);
            Depth(float(plane + 1) / 8);
        }
        Reject<std::out_of_range>([&] { targets.Bind({color, 6}); });
        Color(255, 0, 255);
        std::cout << "target-six-cube-color-and-depth-faces=passed\n";
    }
    void FailuresAndLifetime(GlRenderTargets &targets) {
        auto color = targets.Create({GxTex_2d, 32, 32, GlTargetFormat::Rgba8, true});
        auto depth = targets.Create({GxTex_2d, 16, 16, GlTargetFormat::Depth24});
        targets.Bind({color});
        GLuint frame = Get(GL_FRAMEBUFFER_BINDING_EXT);
        GLuint texture = Attachment(GL_COLOR_ATTACHMENT0_EXT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT);
        GLuint implicit = Attachment(GL_DEPTH_ATTACHMENT_EXT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT);
        ClearColor(0, 0, 1, 1);
        Clear(GL_COLOR_BUFFER_BIT);
        Reject<std::invalid_argument>([&] { targets.Bind({color}, {depth}); });
        Reject<std::invalid_argument>([&] { targets.Bind({depth}); });
        Reject<std::out_of_range>([&] { targets.Bind({color, 1}); });
        Reject<std::invalid_argument>([&] { targets.Bind({{}, 1}); });
        Reject<std::invalid_argument>([&] { targets.Create({GxTex_2d, 0, 1}); });
        Reject<std::invalid_argument>([&] { targets.Create({GxTex_CubeMap, 16, 8}); });
        Reject<std::invalid_argument>(
            [&] { targets.Create({GxTex_2d, 16, 16, GlTargetFormat::Depth24, true}); });
        Reject<std::invalid_argument>([&] { targets.Create({GxTexTargets_Last, 16, 16}); });
        Reject<std::out_of_range>([&] { targets.Create({GxTex_2d, UINT32_MAX, 16}); });
        {
            GlRenderTargets other(context);
            auto foreign = other.Create({GxTex_2d, 32, 32});
            Reject<std::invalid_argument>([&] { targets.Bind({foreign}); });
            Reject<std::invalid_argument>([&] { targets.Sample(0, foreign); });
        }
        Reject<std::logic_error>([&] { targets.Sample(0, color); });
        Require(static_cast<GLuint>(Get(GL_FRAMEBUFFER_BINDING_EXT)) == frame,
                "Rejected target transition changed the framebuffer");
        View(targets, 32, 32, true);
        Color(0, 0, 255);
        bool rejected = false;
        std::thread wrong([&] {
            Reject<std::logic_error>([&] { targets.Create({GxTex_2d, 16, 16}); });
            Reject<std::logic_error>([&] { targets.Bind(); });
            Reject<std::logic_error>([&] { targets.Sample(0); });
            Reject<std::logic_error>([&] { targets.View(); });
            rejected = true;
        });
        wrong.join();
        Require(rejected, "Off-thread render-target operations were accepted");
        color = {};
        Require(IsTexture(texture) && IsRenderbufferEXT(implicit),
                "Live attachment lost its retained storage");
        targets.Bind();
        Require(!IsTexture(texture) && !IsRenderbufferEXT(implicit) && !IsFramebufferEXT(frame),
                "Detached target storage/framebuffer was leaked");
        auto sampled = targets.Create({GxTex_2d, 16, 16});
        targets.Sample(2, sampled);
        ActiveTextureARB(GL_TEXTURE2);
        texture = Get(GL_TEXTURE_BINDING_2D);
        ActiveTextureARB(GL_TEXTURE0);
        sampled = {};
        Require(IsTexture(texture), "Sampler failed to retain its texture");
        targets.Sample(2);
        Require(!IsTexture(texture), "Unbound sampler retained its texture");
        Clean();
        std::cout << "target-validation-affinity-attachment-and-sampler-lifetime=passed\n";
    }
    void FeedbackAndRollback(GlRenderTargets &targets) {
        auto color = targets.Create({GxTex_2d, 32, 32});
        auto broken = targets.Create({GxTex_2d, 32, 32});
        targets.Bind();
        ActiveTextureARB(GL_TEXTURE1);
        targets.Sample(3, color);
        Require(Get(GL_ACTIVE_TEXTURE) == GL_TEXTURE1, "Sampling changed the active texture unit");
        Reject<std::logic_error>([&] { targets.Bind({color}); });
        Require(Get(GL_ACTIVE_TEXTURE) == GL_TEXTURE1, "Rejected feedback changed active texture unit");
        targets.Sample(3);
        targets.Sample(0, broken);
        ActiveTextureARB(GL_TEXTURE0);
        GLuint name = Get(GL_TEXTURE_BINDING_2D);
        targets.Sample(0);
        // Fault injection only: redefine this allocation's GL image to zero
        // width behind the owner. This forces a real incomplete FBO after the
        // validated native metadata, exercising driver-failure rollback.
        BindTexture(GL_TEXTURE_2D, name);
        TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        BindTexture(GL_TEXTURE_2D, 0);
        targets.Bind({color});
        GLuint frame = Get(GL_FRAMEBUFFER_BINDING_EXT);
        ClearColor(1, 1, 0, 1);
        Clear(GL_COLOR_BUFFER_BIT);
        Reject<std::runtime_error>([&] { targets.Bind({broken}); });
        Require(static_cast<GLuint>(Get(GL_FRAMEBUFFER_BINDING_EXT)) == frame &&
                    static_cast<GLuint>(Get(GL_READ_FRAMEBUFFER_BINDING)) == frame,
                "Incomplete framebuffer did not restore read/draw bindings");
        View(targets, 32, 32, true);
        Color(255, 255, 0);
        targets.Bind();
        // Inspect the real GL sampler bindings, even if a caller bound a raw
        // name without going through Sample(). This is deliberate misuse.
        BindTexture(GL_TEXTURE_2D, name);
        Reject<std::logic_error>([&] { targets.Bind({broken}); });
        BindTexture(GL_TEXTURE_2D, 0);
        Clean();
        std::cout << "target-feedback-and-injected-incomplete-fbo-rollback=passed\n";
    }
    void Drawable(GlRenderTargets &targets) {
        targets.Bind();
        Require(Get(GL_FRAMEBUFFER_BINDING_EXT) == 0 && Get(GL_READ_FRAMEBUFFER_BINDING) == 0 &&
                    Get(GL_DRAW_BUFFER) == GL_BACK && Get(GL_READ_BUFFER) == GL_BACK,
                "Drawable read/draw state was not restored");
        Require(SDL_SetWindowSize(context.Window().Handle(), 517, 293), "Cannot resize fixture window");
        int w = 0, h = 0, pw = 0, ph = 0;
        for (int i = 0; i < 100; ++i) {
            context.Window().Pump();
            context.Window().Size(w, h, pw, ph);
            if (w == 517 && h == 293)
                break;
            SDL_Delay(10);
        }
        Require(w == 517 && h == 293 && pw > 0 && ph > 0, "Fixture window resize was not observed");
        targets.Bind();
        View(targets, pw, ph, false);
        ClearColor(0, 1, 1, 1);
        Clear(GL_COLOR_BUFFER_BIT);
        Color(0, 255, 255);
        context.Present();
        std::cout << "target-default-drawable-resize-pixel-viewport=passed logical=" << w << ',' << h
                  << " pixels=" << pw << ',' << ph << '\n';
    }
};
} // namespace
int main() {
    StormRtlInitialize();
    OsTimeStartup(OsTimeManager::BestAvailable);
    int result = 0;
    try {
        for (int cycle = 0; cycle < 3; ++cycle) {
            GlContext context("M3 native Gx target fixtures", 320, 240, true);
            const auto &info = context.Info();
            std::cout << "video=" << info.video << "\ngl-vendor=" << info.vendor
                      << "\ngl-renderer=" << info.renderer << "\ngl-version=" << info.version << '\n';
            Checks checks(context);
            checks.Disable(GL_DITHER);
            checks.Disable(GL_SCISSOR_TEST);
            GlRenderTargets targets(context);
            targets.Bind();
            checks.AllocationState(targets);
            checks.Transitions(targets);
            checks.Cube(targets);
            checks.FailuresAndLifetime(targets);
            checks.FeedbackAndRollback(targets);
            checks.Drawable(targets);
        }
        std::cout << "native-render-target-cycles=3 passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    OsTimeShutdown();
    StormRtlDestroy();
    return result;
}
