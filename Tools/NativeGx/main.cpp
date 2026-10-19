#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include "Engine/Source/Gx/Portable/GlContext.h"
#include "Engine/Source/Gx/Portable/GlProgram.h"
#include "Engine/Source/Gx/Portable/GlRenderTargets.h"
#include "Engine/Source/Gx/Portable/GlTransforms.h"
#include "Engine/Source/Gx/Portable/GlBuffers.h"
#include "Engine/Source/Gxu/Portable/FontFace.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Services/Portable/AsyncFile.h"
#include "Engine/Source/Storm/H/storm.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <array>
#include <algorithm>
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
    BASE(ReadPixels);
    BASE(PixelStorei);
    BASE(ReadBuffer);
#undef BASE
#define EXT(type, name) type name = reinterpret_cast<type>(context.Proc("gl" #name))
    EXT(PFNGLGETPROGRAMIVARBPROC, GetProgramivARB);
#undef EXT
    std::unique_ptr<Gx::Native::GlProgram> vertex, fragment;
    Gx::Native::GlRenderTargets targets;
    Gx::Native::GlTransforms transforms;
    Gx::Native::GlBuffers buffers, fontBuffers;
    Gx::Native::GlBuffer quadVertices, quadIndices;
    Gx::Native::GlTarget target;
    Gx::Native::GlTexture texture;
    struct FontVertex { float x, y, z; uint8_t color[4]; float u, v; };
    static_assert(sizeof(FontVertex) == 24, "Original font PCT vertex size");
    std::vector<FontVertex> fontPixels;
    Gx::Native::GlTexture fontAtlas;
    Gx::Native::GlBuffer fontGeometry;
    int fontSampleX = -1, fontSampleY = -1;
    bool originalUi = false;
    explicit Probe(GlContext &owner) : context(owner), targets(owner), transforms(owner, targets), buffers(owner), fontBuffers(owner) {
        quadVertices = buffers.Create({GxPoolTarget_Vertex, GxPoolUsage_Stream, 24, 4});
        quadIndices = buffers.Create({GxPoolTarget_Index, GxPoolUsage_Static, 2, 6});
        const uint16_t indices[] = {0, 1, 2, 0, 2, 3};
        buffers.Upload(quadIndices, indices, sizeof(indices));
        const Gx::Native::GlVertexBinding attributes[] = {
            {quadVertices, GxVA_Position, 4, 0, 24},
            {quadVertices, GxVA_Color0, 0, 12, 24},
            {quadVertices, GxVA_TexCoord0, 3, 16, 24}};
        buffers.VertexFormat(attributes, std::size(attributes));
        buffers.IndexBuffer(quadIndices);
    }
    ~Probe() {
        targets.Bind();
        Disable(GL_VERTEX_PROGRAM_ARB);
        Disable(GL_FRAGMENT_PROGRAM_ARB);
        targets.Sample(0);
    }
    void Check(const char *operation) {
        GLenum error = GetError();
        Require(error == GL_NO_ERROR, std::string(operation) + " GL error=" + std::to_string(error));
    }
    static Gx::Native::ShaderRecord Record(const char* source) {
        Gx::Native::ShaderRecord record;
        record.code.assign(source, source + std::strlen(source) + 1);
        return record;
    }
    void ProgramFailures() {
        vertex->Bind();
        GLint previous = 0, after = 0;
        GetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_BINDING_ARB, &previous);
        bool rejected = false;
        try {
            Gx::Native::GlProgram invalid(context, Gx::Native::ShaderStage::Vertex,
                Record("!!ARBvp1.0\nINVALID instruction;\nEND\n"));
        } catch (const std::runtime_error&) { rejected = true; }
        Require(rejected, "Malformed ARB program was accepted");
        GetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_BINDING_ARB, &after);
        Require(previous == after, "Failed program upload changed the binding");
        const float value[4] = {1, 0, 0, 0};
        rejected = false;
        try { vertex->Constant(UINT32_MAX, value); }
        catch (const std::out_of_range&) { rejected = true; }
        Require(rejected, "Out-of-range ARB constant was accepted");
        rejected = false;
        std::thread wrongThread([&] {
            try { vertex->Bind(); }
            catch (const std::logic_error&) { rejected = true; }
        });
        wrongThread.join();
        Require(rejected, "Off-thread shader bind was accepted");
        Check("Rejected shader operations leave clean GL state");
        std::cout << "native-program-failure-rollback-affinity=passed\n";
    }
    void Image(Gx::Native::GlTexture& name, const Blizzard::Assets::Image& image) {
        name = targets.Create({GxTex_2d, image.width, image.height,
            Gx::Native::GlTargetFormat::Rgba8, false, 1, false});
        targets.Upload(name, {0, 0, 0, 0, image.width, image.height}, GxTex_Abgr8888,
            image.rgba.data(), image.rgba.size());
        Check("Owned RGBA8 texture upload");
    }
    void Quad(float z, float alpha) {
        struct Vertex { float x, y, z; uint8_t color[4]; float u, v; };
        static_assert(sizeof(Vertex) == 24, "12340 PCT vertex size");
        uint8_t a = static_cast<uint8_t>(alpha * 255 + 0.5f);
        const Vertex vertices[] = {
            {-0.75f, -0.75f, z, {255, 255, 255, a}, 0, 0},
            { 0.75f, -0.75f, z, {255, 255, 255, a}, 1, 0},
            { 0.75f,  0.75f, z, {255, 255, 255, a}, 1, 1},
            {-0.75f,  0.75f, z, {255, 255, 255, a}, 0, 1}};
        auto write = buffers.Lock(quadVertices);
        std::memcpy(write.Data(), vertices, sizeof(vertices));
        write.Commit();
        buffers.Draw(GxPrim_Triangles, 0, 6, true);
    }
    void Font(Bytes bytes) {
        Gxu::Native::FontFace face(std::move(bytes));
        const std::string text = "World of Warcraft 3.3.5a - Native Linux";
        std::vector<uint16_t> atlas(256 * 256);
        uint32_t atlasX = 1, atlasY = 1, rowHeight = 0;
        float pen = 16;
        for (size_t i = 0; i < text.size(); ++i) {
            auto glyph = face.Raster(static_cast<uint8_t>(text[i]), 24);
            if (glyph.width && glyph.height) {
                if (atlasX + glyph.width + 1 > 256) { atlasX = 1; atlasY += rowHeight + 1; rowHeight = 0; }
                Require(atlasY + glyph.height + 1 <= 256, "Original font sample atlas exhausted");
                for (uint32_t y = 0; y < glyph.height; ++y) for (uint32_t x = 0; x < glyph.width; ++x) {
                    auto coverage = glyph.coverage[size_t(y) * glyph.width + x];
                    // Original TEXTURECACHE::PasteGlyphNonOutlinedAA nibble encoding.
                    atlas[(atlasY + y) * 256 + atlasX + x] = uint16_t((coverage << 8) | 0xFFF);
                    if (fontSampleX < 0 && coverage >= 240) {
                        fontSampleX = int(pen) + glyph.bearing + int(x);
                        fontSampleY = 40 - glyph.top + int(y);
                    }
                }
                float x0 = pen + glyph.bearing, y0 = 40 - glyph.top;
                float x1 = x0 + glyph.width, y1 = y0 + glyph.height;
                float u0 = atlasX / 256.0f, u1 = (atlasX + glyph.width) / 256.0f;
                float v0 = atlasY / 256.0f, v1 = (atlasY + glyph.height) / 256.0f;
                const FontVertex quad[] = {
                    {x0,y0,0,{255,255,255,255},u0,v0}, {x1,y0,0,{255,255,255,255},u1,v0},
                    {x1,y1,0,{255,255,255,255},u1,v1}, {x0,y0,0,{255,255,255,255},u0,v0},
                    {x1,y1,0,{255,255,255,255},u1,v1}, {x0,y1,0,{255,255,255,255},u0,v1}};
                fontPixels.insert(fontPixels.end(), std::begin(quad), std::end(quad));
                atlasX += glyph.width + 1;
                rowHeight = std::max(rowHeight, glyph.height);
            }
            pen += i + 1 < text.size() ? face.Step(static_cast<uint8_t>(text[i]), static_cast<uint8_t>(text[i + 1]), 24) : glyph.advance;
        }
        Require(!fontPixels.empty() && fontSampleX >= 0, "Original font sample has no opaque glyph pixels");
        fontAtlas = targets.Create({GxTex_2d, 256, 256, Gx::Native::GlTargetFormat::Rgba4, false, 1, false});
        targets.Upload(fontAtlas, {0,0,0,0,256,256}, GxTex_Argb4444, atlas.data(), atlas.size() * sizeof(uint16_t), 512);
        fontGeometry = fontBuffers.Create({GxPoolTarget_Vertex, GxPoolUsage_Stream, sizeof(FontVertex), uint32_t(fontPixels.size())});
        const Gx::Native::GlVertexBinding fields[] = {{fontGeometry,0,4,0,sizeof(FontVertex)},
            {fontGeometry,4,0,12,sizeof(FontVertex)}, {fontGeometry,6,3,16,sizeof(FontVertex)}};
        fontBuffers.VertexFormat(fields, std::size(fields));
        auto checkTarget = targets.Create({GxTex_2d,512,64});
        targets.Bind({checkTarget});
        ClearColor(0,0,0,1); Clear(GL_COLOR_BUFFER_BIT);
        DrawFont(512,64);
        std::array<uint8_t,4> pixel{};
        // Original offscreen projection flips Y: top-down sample pixels map
        // directly to FBO readback rows here, as in original UI pixel fixtures.
        ReadPixels(fontSampleX,fontSampleY,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        Require(pixel[0] >= 254 && pixel[1] >= 254 && pixel[2] >= 254, "Original font/UI shader glyph pixel mismatch");
        Check("Original font/UI shader pixel readback");
        targets.Bind();
        std::cout << "original-font-family=" << face.Family() << '\n'
                  << "original-font-raster-argb4444-buffered-ui-shader-pixel=passed\n";
    }
    void DrawFont(int width, int height) {
        transforms.Sync();
        auto constants = transforms.UiConstants();
        for (uint32_t i = 0; i < 4; ++i) {
            float value[4]; std::copy(constants[i].begin(), constants[i].end(), value);
            vertex->Constant(i, value);
        }
        vertex->Bind(); fragment->Bind();
        Enable(GL_VERTEX_PROGRAM_ARB); Enable(GL_FRAGMENT_PROGRAM_ARB);
        Disable(GL_DEPTH_TEST); Disable(GL_CULL_FACE); Enable(GL_BLEND); BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        targets.Sample(0,fontAtlas);
        auto vertices = fontPixels;
        for (auto& v : vertices) { v.x = 2 * v.x / width - 1; v.y = 1 - 2 * v.y / height; }
        fontBuffers.Upload(fontGeometry,vertices.data(),uint32_t(vertices.size() * sizeof(FontVertex)));
        fontBuffers.Draw(GxPrim_Triangles,0,uint32_t(vertices.size()));
    }
    void Draw(int width, int height, bool synthetic) {
        auto view = targets.View();
        Require(view.width == width && view.height == height, "Draw extent differs from bound target");
        transforms.Sync();
        if (originalUi) {
            auto constants = transforms.UiConstants();
            for (uint32_t i = 0; i < 4; ++i) {
                float value[4];
                std::copy(constants[i].begin(), constants[i].end(), value);
                vertex->Constant(i, value);
            }
        }
        vertex->Bind();
        fragment->Bind();
        Disable(GL_DITHER);
        Disable(GL_MULTISAMPLE);
        Disable(GL_FRAMEBUFFER_SRGB);
        Enable(GL_VERTEX_PROGRAM_ARB);
        Enable(GL_FRAGMENT_PROGRAM_ARB);
        Enable(GL_DEPTH_TEST);
        DepthFunc(GL_LESS);
        Enable(GL_BLEND);
        BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        targets.Sample(0, texture);
        ClearColor(0, 0, 0, 1);
        Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Quad(0, synthetic ? 0.5f : 1);
        // A farther transparent draw would darken the result if depth testing
        // failed.
        if (synthetic)
            Quad(0.5f, 0.5f);
        if (!fontPixels.empty()) DrawFont(width, height);
        Check("ARB textured depth/blend/font draw");
    }
    void Initialize() {
        vertex = std::make_unique<Gx::Native::GlProgram>(context, Gx::Native::ShaderStage::Vertex, Record(
                "!!ARBvp1.0\nMOV result.position, vertex.position;\n"
                "MOV result.texcoord[0], vertex.attrib[6];\nMOV result.color, "
                "vertex.attrib[4];\nEND\n"));
        fragment = std::make_unique<Gx::Native::GlProgram>(context, Gx::Native::ShaderStage::Fragment, Record(
                "!!ARBfp1.0\nTEMP tex;\nTEX tex, fragment.texcoord[0], texture[0], "
                "2D;\n"
                "MUL result.color, tex, fragment.color;\nEND\n"));
        ProgramFailures();
        Image(texture, {2, 2, {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255}});
        target = targets.Create({GxTex_2d, 32, 32, Gx::Native::GlTargetFormat::Rgba8, true});
        targets.Bind({target, 0});
        Draw(32, 32, true);
        ReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
        CheckPixels();
        const uint8_t dxt1[] = {0, 248, 224, 7, 0, 0, 0, 0};
        texture = targets.Create({GxTex_2d, 4, 4, Gx::Native::GlTargetFormat::Dxt1, false, 1, false});
        targets.Upload(texture, {0, 0, 0, 0, 4, 4}, GxTex_Dxt1, dxt1, sizeof(dxt1));
        Draw(32, 32, false);
        std::array<uint8_t, 4> compressedPixel{};
        ReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, compressedPixel.data());
        Require(compressedPixel[0] == 255 && compressedPixel[1] == 0 && compressedPixel[2] == 0,
                "S3TC/DXT1 pixel mismatch");
        Check("S3TC readback");
        Image(texture, {2, 2, {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255}});
        targets.Bind();
        std::cout << "synthetic-arb-texture-depth-blend-fbo=passed\n";
        std::cout << "native-buffered-pct-u16-submission=passed\n";
        std::cout << "native-owned-texture-rgba-dxt1-sampling=passed\n";
        std::cout << "synthetic-s3tc-dxt1=passed\n";
    }
    void CheckPixels(bool flipY = false, bool scaled = false) {
        const std::array<std::array<int, 5>, 5> samples{{{{8, 8, 128, 0, 0}},
                                                         {{24, 8, 0, 128, 0}},
                                                         {{8, 24, 0, 0, 128}},
                                                         {{24, 24, 128, 128, 0}},
                                                         {{1, 1, 0, 0, 0}}}};
        for (const auto &sample : samples) {
            std::array<uint8_t, 4> pixel{};
            int x = sample[0], y = sample[1];
            if (scaled) {
                x = int(0.5f * x + 10.25f);
                y = int(21.75f - 0.5f * y);
            } else if (flipY) y = 31 - y;
            ReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            for (int channel = 0; channel < 3; ++channel)
                Require(std::abs(int(pixel[channel]) - sample[channel + 2]) <= 1,
                        "Synthetic ARB/FBO pixel mismatch at " + std::to_string(sample[0]) + "," +
                            std::to_string(sample[1]) + " channel=" + std::to_string(channel) +
                            " actual=" + std::to_string(pixel[channel]));
        }
        Check("FBO readback");
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
    Gx::Native::ShaderPackage vertexShaders, fragmentShaders;
    Bytes fontBytes;
    bool fontSigned = false;
    bool vertexSigned = false, fragmentSigned = false;
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
            auto vs = packages->Read("Shaders\\Vertex\\arbvp1\\UI.bls", 64 * 1024 * 1024);
            auto fs = packages->Read("Shaders\\Pixel\\arbfp1\\UI.bls", 64 * 1024 * 1024);
            vertexSigned = vs.signedFile;
            fragmentSigned = fs.signedFile;
            auto font = packages->Read("Fonts\\FRIZQT__.TTF", 32 * 1024 * 1024);
            fontSigned = font.signedFile;
            fontBytes = std::move(font.bytes);
            vertexShaders = Gx::Native::DecodeShaderPackage(vs.bytes, 2);
            fragmentShaders = Gx::Native::DecodeShaderPackage(fs.bytes, 1);
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
    // Both original vertex permutations must upload. The normal UI draw uses
    // permutation zero; stereo constants/rendering are not qualified here.
    Gx::Native::GlProgram stereo(context, Gx::Native::ShaderStage::Vertex, vertexShaders.permutations.at(1));
    probe.vertex = std::make_unique<Gx::Native::GlProgram>(context, Gx::Native::ShaderStage::Vertex,
                                                         vertexShaders.permutations.at(0));
    probe.fragment = std::make_unique<Gx::Native::GlProgram>(context, Gx::Native::ShaderStage::Fragment,
                                                           fragmentShaders.permutations.at(0));
    probe.originalUi = true;
    // Reuse known synthetic pixels with the original programs before displaying
    // the original texture. Shader upload alone is not rendered-output evidence.
    probe.targets.Bind({probe.target, 0});
    probe.Draw(32, 32, true);
    probe.ReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    probe.CheckPixels(true);
    probe.Check("Original UI shader readback");
    probe.transforms.Scale(GxXform_View, {0.5f, 0.5f, 1});
    probe.transforms.Translate(GxXform_View, {0.25f, 0.25f, 0});
    probe.Draw(32, 32, true);
    probe.CheckPixels(true, true);
    probe.transforms.Set(GxXform_View, C44Matrix());
    std::cout << "original-ui-shader-transformed-fbo-pixel=passed\n";
    probe.targets.Bind();
    probe.Font(std::move(fontBytes));
    std::cout << "original-font=Fonts/FRIZQT__.TTF registered-md5=" << fontSigned << '\n';
    probe.Image(probe.texture, image);
    std::cout << "original-ui-shaders=0x10003 vertex-permutations=2 fragment-permutations=1"
              << " vertex-md5=" << vertexSigned << " fragment-md5=" << fragmentSigned << '\n'
              << "original-ui-shader-fbo-pixel=passed\n";
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
            probe.targets.Bind();
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
