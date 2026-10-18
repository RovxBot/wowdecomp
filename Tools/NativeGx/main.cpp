#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include "Engine/Source/Gx/Portable/GlContext.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Services/Portable/AsyncFile.h"
#include "Engine/Source/Storm/H/storm.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <array>
#include <cfenv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using Blizzard::Mopaq::Native::Bytes;
using Gx::Native::GlContext;
namespace {
void Require(bool value, const std::string &message) {
    if (!value)
        throw std::runtime_error(message);
}
struct Core {
    Core() {
        Require(!std::fesetenv(FE_DFL_ENV) && !std::fesetround(FE_TONEAREST),
                "Floating point initialization failed");
        StormRtlInitialize();
        OsTimeStartup(OsTimeManager::BestAvailable);
    }
    ~Core() {
        OsTimeShutdown();
        StormRtlDestroy();
    }
};
struct Options {
    int cycles = 1, frames = 120;
    bool hidden = false, unavailable = false;
    std::string snapshot, data, settings, cache, locale = "enUS";
};
Options Parse(int argc, char **argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--hidden") {
            o.hidden = true;
            continue;
        }
        if (key == "--expect-unavailable") {
            o.unavailable = true;
            continue;
        }
        Require(i + 1 < argc, "Missing value for " + key);
        std::string value = argv[++i];
        if (key == "--cycles" || key == "--frames") {
            size_t used = 0;
            int count = std::stoi(value, &used);
            Require(used == value.size() && count > 0 && count <= (key == "--cycles" ? 20 : 36000),
                    "Invalid cycle/frame count");
            (key == "--cycles" ? o.cycles : o.frames) = count;
        } else if (key == "--snapshot")
            o.snapshot = value;
        else if (key == "--data")
            o.data = value;
        else if (key == "--settings")
            o.settings = value;
        else if (key == "--cache")
            o.cache = value;
        else if (key == "--locale")
            o.locale = value;
        else
            throw std::runtime_error("Unknown option " + key);
    }
    Require(o.data.empty() == (o.settings.empty() && o.cache.empty()),
            "Original-data probe needs --data, --settings and --cache together");
    Require(o.data.empty() || (!o.settings.empty() && !o.cache.empty()),
            "Specify separate settings/cache roots");
    return o;
}
// This fixture exercises the original API family, not a replacement game UI.
// Every procedure is held by the probe and discarded before its context dies.
struct Probe {
    GlContext &context;
#define BASE(name) decltype(&gl##name) name = reinterpret_cast<decltype(name)>(context.Proc("gl" #name))
    BASE(GetError);
    BASE(GetIntegerv);
    BASE(GetString);
    BASE(Viewport);
    BASE(ClearColor);
    BASE(Clear);
    BASE(Enable);
    BASE(Disable);
    BASE(BlendFunc);
    BASE(DepthFunc);
    BASE(Color4f);
    BASE(Begin);
    BASE(End);
    BASE(TexCoord2f);
    BASE(Vertex3f);
    BASE(ReadPixels);
    BASE(PixelStorei);
    BASE(ReadBuffer);
    BASE(GenTextures);
    BASE(DeleteTextures);
    BASE(BindTexture);
    BASE(TexImage2D);
    BASE(TexParameteri);
#undef BASE
#define EXT(type, name) type name = reinterpret_cast<type>(context.Proc("gl" #name))
    EXT(PFNGLGENPROGRAMSARBPROC, GenProgramsARB);
    EXT(PFNGLBINDPROGRAMARBPROC, BindProgramARB);
    EXT(PFNGLPROGRAMSTRINGARBPROC, ProgramStringARB);
    EXT(PFNGLDELETEPROGRAMSARBPROC, DeleteProgramsARB);
    EXT(PFNGLCOMPRESSEDTEXIMAGE2DARBPROC, CompressedTexImage2DARB);
    EXT(PFNGLGENFRAMEBUFFERSEXTPROC, GenFramebuffersEXT);
    EXT(PFNGLBINDFRAMEBUFFEREXTPROC, BindFramebufferEXT);
    EXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, FramebufferTexture2DEXT);
    EXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, CheckFramebufferStatusEXT);
    EXT(PFNGLDELETEFRAMEBUFFERSEXTPROC, DeleteFramebuffersEXT);
    EXT(PFNGLGENRENDERBUFFERSEXTPROC, GenRenderbuffersEXT);
    EXT(PFNGLBINDRENDERBUFFEREXTPROC, BindRenderbufferEXT);
    EXT(PFNGLRENDERBUFFERSTORAGEEXTPROC, RenderbufferStorageEXT);
    EXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC, FramebufferRenderbufferEXT);
    EXT(PFNGLDELETERENDERBUFFERSEXTPROC, DeleteRenderbuffersEXT);
#undef EXT
    GLuint programs[2]{}, texture = 0, target = 0, framebuffer = 0, depth = 0;
    explicit Probe(GlContext &owner) : context(owner) {}
    ~Probe() {
        BindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        Disable(GL_VERTEX_PROGRAM_ARB);
        Disable(GL_FRAGMENT_PROGRAM_ARB);
        DeleteProgramsARB(2, programs);
        DeleteTextures(1, &texture);
        DeleteTextures(1, &target);
        DeleteRenderbuffersEXT(1, &depth);
        DeleteFramebuffersEXT(1, &framebuffer);
    }
    void Check(const char *operation) {
        GLenum error = GetError();
        Require(error == GL_NO_ERROR, std::string(operation) + " GL error=" + std::to_string(error));
    }
    void Program(GLenum kind, GLuint name, const char *source) {
        BindProgramARB(kind, name);
        ProgramStringARB(kind, GL_PROGRAM_FORMAT_ASCII_ARB, static_cast<GLsizei>(std::strlen(source)),
                         source);
        GLint position = -1;
        GetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &position);
        if (position != -1) {
            auto error = GetString(GL_PROGRAM_ERROR_STRING_ARB);
            throw std::runtime_error("ARB program rejected at " + std::to_string(position) + ": " +
                                     (error ? reinterpret_cast<const char *>(error) : "unknown error"));
        }
        Check("ARB program upload");
    }
    void Image(GLuint name, const Blizzard::Assets::Image &image) {
        BindTexture(GL_TEXTURE_2D, name);
        TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                   image.rgba.data());
        Check("RGBA8 texture upload");
    }
    void Quad(float z, float alpha) {
        Color4f(1, 1, 1, alpha);
        Begin(GL_QUADS);
        TexCoord2f(0, 0);
        Vertex3f(-0.75f, -0.75f, z);
        TexCoord2f(1, 0);
        Vertex3f(0.75f, -0.75f, z);
        TexCoord2f(1, 1);
        Vertex3f(0.75f, 0.75f, z);
        TexCoord2f(0, 1);
        Vertex3f(-0.75f, 0.75f, z);
        End();
    }
    void Draw(int width, int height, bool synthetic) {
        Viewport(0, 0, width, height);
        Disable(GL_DITHER);
        Disable(GL_MULTISAMPLE);
        Disable(GL_FRAMEBUFFER_SRGB);
        Enable(GL_VERTEX_PROGRAM_ARB);
        Enable(GL_FRAGMENT_PROGRAM_ARB);
        Enable(GL_DEPTH_TEST);
        DepthFunc(GL_LESS);
        Enable(GL_BLEND);
        BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        BindTexture(GL_TEXTURE_2D, texture);
        ClearColor(0, 0, 0, 1);
        Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Quad(0, synthetic ? 0.5f : 1);
        // A farther transparent draw would darken the result if depth testing
        // failed.
        if (synthetic)
            Quad(0.5f, 0.5f);
        Check("ARB textured depth/blend draw");
    }
    void Initialize() {
        GenProgramsARB(2, programs);
        Program(GL_VERTEX_PROGRAM_ARB, programs[0],
                "!!ARBvp1.0\nMOV result.position, vertex.position;\n"
                "MOV result.texcoord[0], vertex.texcoord[0];\nMOV result.color, "
                "vertex.color;\nEND\n");
        Program(GL_FRAGMENT_PROGRAM_ARB, programs[1],
                "!!ARBfp1.0\nTEMP tex;\nTEX tex, fragment.texcoord[0], texture[0], "
                "2D;\n"
                "MUL result.color, tex, fragment.color;\nEND\n");
        GenTextures(1, &texture);
        Image(texture, {2, 2, {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255}});
        GenTextures(1, &target);
        BindTexture(GL_TEXTURE_2D, target);
        TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        GenFramebuffersEXT(1, &framebuffer);
        BindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
        FramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, target, 0);
        GenRenderbuffersEXT(1, &depth);
        BindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth);
        RenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, 32, 32);
        FramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depth);
        Require(CheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT,
                "Synthetic FBO is incomplete");
        Draw(32, 32, true);
        ReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
        const std::array<std::array<int, 5>, 5> samples{{{{8, 8, 128, 0, 0}},
                                                         {{24, 8, 0, 128, 0}},
                                                         {{8, 24, 0, 0, 128}},
                                                         {{24, 24, 128, 128, 0}},
                                                         {{1, 1, 0, 0, 0}}}};
        for (const auto &sample : samples) {
            std::array<uint8_t, 4> pixel{};
            ReadPixels(sample[0], sample[1], 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            for (int channel = 0; channel < 3; ++channel)
                Require(std::abs(int(pixel[channel]) - sample[channel + 2]) <= 1,
                        "Synthetic ARB/FBO pixel mismatch at " + std::to_string(sample[0]) + "," +
                            std::to_string(sample[1]) + " channel=" + std::to_string(channel) +
                            " actual=" + std::to_string(pixel[channel]));
        }
        Check("FBO readback");
        const uint8_t dxt1[] = {0, 248, 224, 7, 0, 0, 0, 0};
        BindTexture(GL_TEXTURE_2D, texture);
        CompressedTexImage2DARB(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 4, 4, 0, sizeof(dxt1),
                                dxt1);
        Draw(32, 32, false);
        std::array<uint8_t, 4> compressedPixel{};
        ReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, compressedPixel.data());
        Require(compressedPixel[0] == 255 && compressedPixel[1] == 0 && compressedPixel[2] == 0,
                "S3TC/DXT1 pixel mismatch");
        Check("S3TC readback");
        Image(texture, {2, 2, {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255}});
        BindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        ReadBuffer(GL_BACK);
        std::cout << "synthetic-arb-texture-depth-blend-fbo=passed\n";
        std::cout << "synthetic-s3tc-dxt1=passed\n";
    }
    void Snapshot(const std::string &path, int width, int height) {
        Require(width > 0 && height > 0 && width <= 16384 && height <= 16384, "Invalid snapshot size");
        Bytes pixels(size_t(width) * height * 3);
        PixelStorei(GL_PACK_ALIGNMENT, 1);
        ReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        Check("Drawable readback");
        std::ofstream out(path, std::ios::binary);
        out << "P6\n" << width << ' ' << height << "\n255\n";
        for (int y = height - 1; y >= 0; --y)
            out.write(reinterpret_cast<const char *>(pixels.data() + size_t(y) * width * 3), width * 3);
        Require(bool(out), "Cannot save PPM snapshot");
    }
};
void OriginalImage(const Options &options, GlContext &context, Probe &probe) {
    if (options.data.empty())
        return;
    Blizzard::File::Native::Paths paths;
    Require(Blizzard::File::Native::ConfigurePaths(options.data.c_str(), options.settings.c_str(),
                                                   options.cache.c_str(), paths),
            "Cannot configure separate read-only data and user roots");
    Blizzard::Assets::Image image{};
    size_t signatures = 0;
    std::string failure;
    bool loaded = false;
    Services::Native::FileQueue queue;
    auto ticket = queue.Submit(
        [&](const Services::Native::Cancellation &cancel) {
            auto packages = Blizzard::Mopaq::Native::Packages::Open12340(paths, options.locale);
            signatures = packages->SignatureCount();
            if (cancel.Requested())
                throw std::runtime_error("Original image loading cancelled");
            auto file = packages->Read("Interface\\Glues\\LoadingBar\\Loading-BarFill.blp");
            image = Blizzard::Assets::DecodeBlp(file.bytes).at(0);
            return Services::Native::ReadResult{};
        },
        [&](const Services::Native::ReadResult &result) {
            loaded = result.status == Services::Native::ReadStatus::Success;
            failure = result.error;
        });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    bool closed = false;
    while (!Services::Native::FileQueue::Complete(ticket) && !closed &&
           std::chrono::steady_clock::now() < deadline) {
        context.Window().Pump();
        OSEVENT event{};
        while (OsQueueGet(&event.id, &event.param[0], &event.param[1], &event.param[2], &event.param[3]))
            if (event.id == OS_INPUT_CLOSE || event.id == OS_INPUT_SHUTDOWN)
                closed = true;
        queue.Wait(ticket, std::chrono::milliseconds(10));
    }
    if (!Services::Native::FileQueue::Complete(ticket))
        queue.Cancel(ticket);
    queue.Shutdown();
    Require(loaded && !closed, failure.empty() ? "Original image load cancelled/timed out" : failure);
    probe.Image(probe.texture, image);
    std::cout << "original-signaturefile=authenticated records=" << signatures << '\n'
              << "original-glue-loadingbar=uploaded (asset probe; original login "
                 "UI unavailable)\n";
}
void Cycle(const Options &options) {
    GlContext context("WoW 12340 - M3 OpenGL context probe (no login UI)", 960, 640, options.hidden);
    const auto &info = context.Info();
    std::cout << "video=" << info.video << "\ngl-vendor=" << info.vendor << "\ngl-renderer=" << info.renderer
              << "\ngl-version=" << info.version << "\nprofile=compatibility\nrgba=" << info.red << ','
              << info.green << ',' << info.blue << ',' << info.alpha << " depth=" << info.depth
              << " stencil=" << info.stencil << " double-buffer=" << info.doubleBuffered
              << " swap-interval=" << info.swapInterval << '\n';
    bool rejected = false;
    std::thread wrongThread([&] {
        try {
            context.Present();
        } catch (const std::logic_error &) {
            rejected = true;
        }
    });
    wrongThread.join();
    Require(rejected, "Off-thread presentation was not rejected");
    Probe probe(context);
    probe.Initialize();
    OriginalImage(options, context, probe);
    bool running = true, fullscreen = false;
    for (int frame = 0; running && frame < options.frames; ++frame) {
        context.Window().Pump();
        OSEVENT event{};
        while (OsQueueGet(&event.id, &event.param[0], &event.param[1], &event.param[2], &event.param[3])) {
            if (event.id == OS_INPUT_CLOSE || event.id == OS_INPUT_SHUTDOWN)
                running = false;
            if (event.id == OS_INPUT_KEY_DOWN && event.param[0] == KEY_ESCAPE)
                running = false;
            if (event.id == OS_INPUT_KEY_DOWN && event.param[0] == KEY_F11) {
                fullscreen = !fullscreen;
                Require(context.Window().Fullscreen(fullscreen), SDL_GetError());
            }
        }
        int w, h, pw, ph;
        context.Window().Size(w, h, pw, ph);
        if (pw > 0 && ph > 0) {
            probe.Draw(pw, ph, options.data.empty());
            if (!options.snapshot.empty() && frame == 0)
                probe.Snapshot(options.snapshot, pw, ph);
            context.Present();
        }
        SDL_Delay(16);
    }
}
} // namespace
int main(int argc, char **argv) {
    try {
        auto options = Parse(argc, argv);
        Core core;
        if (options.unavailable) {
            Require(std::string(std::getenv("SDL_VIDEODRIVER") ? std::getenv("SDL_VIDEODRIVER") : "") ==
                        "dummy",
                    "Unavailable fixture requires the SDL dummy driver");
            bool unavailable = false;
            try {
                GlContext context("Unavailable OpenGL fixture", 320, 240, true);
            } catch (const std::runtime_error &error) {
                std::cout << "unavailable-context=reported " << error.what() << '\n';
                unavailable = true;
            }
            Require(unavailable, "Dummy driver unexpectedly created OpenGL");
            OsNative::Window recovery("Context failure rollback", 320, 240, true);
            recovery.Pump();
            std::cout << "failed-context-rollback=passed\n";
            return 0;
        }
        for (int cycle = 0; cycle < options.cycles; ++cycle) {
            Cycle(options);
            Require(OsLiveThreadCount() == 0, "Native worker remains after GL shutdown");
            std::cout << "context-cycle-closed=" << cycle + 1 << " workers=0\n";
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "NativeGlCheck failed: " << error.what() << '\n';
        return 1;
    }
}
