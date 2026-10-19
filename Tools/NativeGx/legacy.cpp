#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include "Engine/Source/Gx/CGxPool.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/Portable/LegacyDevice.h"
#include "Engine/Source/Gxu/Portable/FontFace.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Services/Portable/AsyncFile.h"
#include "Engine/Source/Storm/H/storm.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cfenv>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
using namespace Gx::Native;
using Bytes = std::vector<uint8_t>;
namespace {
void Require(bool yes, const char *message) {
    if (!yes)
        throw std::runtime_error(message);
}
template <class F> void Reject(F operation, const char *message) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::exception &) {
        rejected = true;
    }
    Require(rejected, message);
}
Bytes Package(const char *program, uint32_t count = 1) {
    Bytes out;
    auto word = [&](uint32_t v) {
        for (int i = 0; i < 4; ++i)
            out.push_back(uint8_t(v >> (8 * i)));
    };
    word(0x47585348);
    word(0x10003);
    word(0); // Same 12340 header as the decoder fixture.
    for (uint32_t i = 0; i < count; ++i) {
        word(0);
        word(0);
        word(0);
        word(uint32_t(std::strlen(program) + 1));
        out.insert(out.end(), program, program + std::strlen(program) + 1);
        while (out.size() % 4)
            out.push_back(0);
    }
    return out;
}
struct Source {
    std::vector<uint16_t> pixels = std::vector<uint16_t>(64, 0xFFFF);
    int locks = 0, latches = 0, unlocks = 0;
    bool bad = false, throwLatch = false;
    static void Callback(EGxTexCommand cmd, uint32_t width, uint32_t, uint32_t, uint32_t, void *user,
                         uint32_t &stride, const void *&bytes) {
        auto &s = *static_cast<Source *>(user);
        if (cmd == GxTex_Lock)
            ++s.locks;
        if (cmd == GxTex_Latch) {
            ++s.latches;
            if (s.throwLatch)
                throw std::runtime_error("Injected latch failure");
        }
        if (cmd == GxTex_Unlock)
            ++s.unlocks;
        stride = s.bad ? 1 : width * 2;
        bytes = s.pixels.data();
    }
};
struct Vertex {
    float x, y, z;
    uint8_t color[4];
    float u, v;
};
static_assert(sizeof(Vertex) == 24, "Original PCT layout");
struct Assets {
    std::map<std::string, Bytes> shaders;
    Bytes font;
    size_t signatures = 0;
    bool original = false;
};
Assets Load(GlContext &context, const std::string &data, const std::string &settings,
            const std::string &cache) {
    Assets assets;
    if (data.empty()) {
        assets.shaders["Shaders\\Vertex\\arbvp1\\UI.bls"] =
            Package("!!ARBvp1.0\nMOV result.position, vertex.position;\nMOV result.color, "
                    "vertex.attrib[4];\nMOV result.texcoord[0], vertex.attrib[6];\nEND\n",
                    2);
        assets.shaders["Shaders\\Pixel\\arbfp1\\UI.bls"] =
            Package("!!ARBfp1.0\nTEMP tex;\nTEX tex, fragment.texcoord[0], texture[0], 2D;\nMUL "
                    "result.color,tex,fragment.color;\nEND\n");
        return assets;
    }
    Blizzard::File::Native::Paths paths;
    Require(Blizzard::File::Native::ConfigurePaths(data.c_str(), settings.c_str(), cache.c_str(), paths),
            "Invalid external data/user roots");
    Services::Native::FileQueue queue;
    bool loaded = false;
    std::string failure;
    auto ticket = queue.Submit(
        [&](const Services::Native::Cancellation &cancel) {
            auto packages = Blizzard::Mopaq::Native::Packages::Open12340(paths, "enUS");
            assets.signatures = packages->SignatureCount();
            for (auto member : {"Shaders\\Vertex\\arbvp1\\UI.bls", "Shaders\\Pixel\\arbfp1\\UI.bls",
                                "Fonts\\FRIZQT__.TTF"}) {
                Require(!cancel.Requested(), "Asset read cancelled");
                auto file = packages->Read(member, 64 * 1024 * 1024);
                std::cout << "original-member=" << member << " registered-md5=" << file.signedFile << '\n';
                if (std::string(member).find("Fonts") == 0)
                    assets.font = std::move(file.bytes);
                else
                    assets.shaders[member] = std::move(file.bytes);
            }
            return Services::Native::ReadResult{};
        },
        [&](const Services::Native::ReadResult &result) {
            loaded = result.status == Services::Native::ReadStatus::Success;
            failure = result.error;
        });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!Services::Native::FileQueue::Complete(ticket) && std::chrono::steady_clock::now() < deadline) {
        context.Window().Pump();
        queue.Wait(ticket, std::chrono::milliseconds(10));
    }
    if (!Services::Native::FileQueue::Complete(ticket))
        queue.Cancel(ticket);
    queue.Shutdown();
    if (!loaded)
        throw std::runtime_error("Original asset read failed: " + failure);
    assets.original = true;
    std::cout << "original-signaturefile=authenticated records=" << assets.signatures << '\n';
    return assets;
}
CImVector Pixel(LegacyDevice &device, int x, int y) {
    CiRect rect{};
    rect.minX = x;
    rect.minY = y;
    rect.maxX = x + 1;
    rect.maxY = y + 1;
    TSGrowableArray<CImVector> pixels;
    device.DeviceReadPixels(rect, pixels);
    return pixels[0];
}
void Constants() {
    C44Matrix matrix;
    GxXformViewProjNativeTranspose(matrix);
    float values[] = {matrix.a0, matrix.a1, matrix.a2, matrix.a3, matrix.b0, matrix.b1, matrix.b2, matrix.b3,
                      matrix.c0, matrix.c1, matrix.c2, matrix.c3, matrix.d0, matrix.d1, matrix.d2, matrix.d3};
    GxShaderConstantsSet(GxSh_Vertex, 0, values, 4);
}
void Cycle(GlContext &context, Assets &assets) {
    int reads = 0;
    LegacyDevice device(context, [&](const std::string &member) {
        ++reads;
        return assets.shaders.at(member);
    });
    Require(g_theGxDevicePtr == &device, "Global Gx device was not installed");
    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_DepthTest, 0);
    GxRsSet(GxRs_Culling, 0);
    GxRsSet(GxRs_BlendingMode, GxBlend_Opaque);
    CGxShader *vertex[2]{};
    CGxShader *fragment = nullptr;
    device.ShaderCreate(vertex, GxSh_Vertex, "Shaders\\Vertex", "UI", 2);
    device.ShaderCreate(&fragment, GxSh_Pixel, "Shaders\\Pixel", "UI", 1);
    CGxShader *cached[2]{};
    device.ShaderCreate(cached, GxSh_Vertex, "Shaders\\Vertex", "UI", 2);
    Require(cached[0] == vertex[0] && reads == 2, "Legacy shader hash/cache lookup failed");
    for (uint32_t i = 0; i < 2u; ++i)
        device.ShaderDestroy(cached[i]);
    device.ShaderDestroy(vertex[1]);
    device.ShaderCreate(cached, GxSh_Vertex, "Shaders\\Vertex", "UI", 2);
    Require(cached[0] == vertex[0] && reads == 3, "Partial shader cache was not repaired");
    vertex[1] = cached[1];
    cached[1] = nullptr;
    device.ShaderDestroy(cached[0]);
    GxRsSet(GxRs_VertexShader, vertex[0]);
    GxRsSet(GxRs_PixelShader, fragment);
    Source source;
    CGxTex *texture = nullptr;
    CGxTex *target = nullptr;
    CGxTexFlags flags(GxTex_Nearest);
    Require(GxTexCreate(GxTex_2d, 8, 8, 0, GxTex_Argb4444, GxTex_Argb4444, flags, &source, Source::Callback,
                        "legacy fixture", texture),
            "Legacy callback texture creation failed");
    Source targetSource;
    auto renderFlags = flags;
    renderFlags.m_renderTarget = 1;
    Require(GxTexCreate(GxTex_2d, 32, 32, 0, GxTex_Argb8888, GxTex_Argb8888, renderFlags, &targetSource,
                        Source::Callback, "legacy target", target),
            "Legacy target creation failed");
    device.DeviceSetRenderTarget(GxBuffers_Color, target, 0);
    GxRsSet(GxRs_Texture0, texture);
    source.bad = true;
    Reject([&] { GxTexUpdate(texture, 0, 0, 8, 8, 1); }, "Invalid stride accepted");
    Require(source.locks == 1 && source.unlocks == 1 && texture->m_needsUpdate,
            "Failed upload lost lock/dirty ownership");
    source.bad = false;
    source.throwLatch = true;
    Reject([&] { GxTexUpdate(texture, 0, 0, 8, 8, 1); }, "Throwing latch accepted");
    Require(source.locks == 2 && source.unlocks == 2, "Throwing latch did not unlock exactly once");
    source.throwLatch = false;
    auto *buffer = GxBufStream(GxPoolTarget_Vertex, sizeof(Vertex), 4);
    const Vertex quad[] = {{-1, -1, 0, {255, 255, 255, 255}, 0, 0},
                           {1, -1, 0, {255, 255, 255, 255}, 1, 0},
                           {1, 1, 0, {255, 255, 255, 255}, 1, 1},
                           {-1, 1, 0, {255, 255, 255, 255}, 0, 1}};
    auto *staging = GxBufLock(buffer);
    std::memcpy(staging, quad, sizeof(quad));
    Reject([&] { GxBufUnlock(buffer, sizeof(quad) + 1); }, "Oversize unlock accepted");
    Require(device.m_bufLocked[0] == buffer, "Rejected unlock lost retained staging");
    GxBufUnlock(buffer, 0);
    GxPrimVertexPtr(buffer, GxVBF_PCT);
    const uint16_t indices[] = {0, 1, 2, 0, 2, 3};
    GxPrimIndexPtr(6, indices);
    CGxBatch batch{GxPrim_Triangles, 0, 6, 0, 3};
    auto draw = [&] {
        Constants();
        GxDraw(&batch, 1);
    };
    GxSceneClear(GxBufferBit_Color | GxBufferBit_Depth, CImVector(0, 0, 0, 255));
    draw();
    auto pixel = Pixel(device, 16, 16);
    Require(pixel.r == 255 && pixel.g == 255 && pixel.b == 255, "Legacy callback/PCT/FBO pixel mismatch");
    Require(source.locks == 3 && source.unlocks == 3, "Upload retry did not complete callback ownership");
    Require(device.TexIsReadyNow(texture), "Native resource never became ready through legacy readiness API");
    bool offThread = false;
    std::thread thread([&] {
        try {
            GxRsSet(GxRs_DepthTest, 1);
        } catch (const std::logic_error &) {
            offThread = true;
        }
    });
    thread.join();
    Require(offThread, "Common legacy state mutation accepted a foreign thread");
    for (size_t y = 2; y < 6; ++y)
        for (size_t x = 2; x < 6; ++x)
            source.pixels[y * 8 + x] = 0xFF00;
    GxTexUpdate(texture, 2, 2, 6, 6, 0);
    draw();
    pixel = Pixel(device, 16, 16);
    Require(pixel.r == 255 && pixel.g == 0 && pixel.b == 0,
            "Legacy dirty rectangle/source-origin pixel mismatch");
    pixel = Pixel(device, 2, 2);
    Require(pixel.r == 255 && pixel.g == 255 && pixel.b == 255, "Dirty rectangle overwrote outside region");
    device.RsPush();
    GxRsSet(GxRs_Texture0, nullptr);
    device.RsPop();
    Require(device.m_appRenderStates[GxRs_Texture0].m_value.m_data.p == texture,
            "Legacy state push/pop lost texture");
    const uint16_t invalid[] = {0, 1, 12};
    GxPrimIndexPtr(3, invalid);
    batch.m_count = 3;
    Reject(draw, "Legacy draw accepted out-of-range actual indices");
    GxPrimIndexPtr(6, indices);
    batch.m_count = 6;
    GxRsSet(GxRs_TexGen0, GxTexGen_ObjectLinear);
    Reject(draw, "Unsupported texgen was silently accepted");
    GxRsSet(GxRs_TexGen0, GxTexGen_Disabled);
    draw();
    device.RsPush();
    GxRsSet(GxRs_Texture0, nullptr);
    GxTexDestroy(texture);
    texture = nullptr;
    device.RsPop();
    Require(!device.m_appRenderStates[GxRs_Texture0].m_value.m_data.p,
            "Destroyed texture survived pushed state");
    std::cout << "legacy-gx-callback-lock-latch-unlock-failure-retry-dirty-rectangle=passed\n"
              << "legacy-gx-cache-pct-u16-bounds-unsupported-state=passed\n";
    if (!assets.font.empty()) {
        Gxu::Native::FontFace face(assets.font);
        auto glyph = face.Raster('A', 24);
        Source atlas;
        atlas.pixels.assign(256 * 256, 0);
        int sampleX = -1, sampleY = -1;
        for (uint32_t y = 0; y < glyph.height; ++y)
            for (uint32_t x = 0; x < glyph.width; ++x) {
                auto coverage = glyph.coverage[y * glyph.width + x];
                atlas.pixels[y * 256 + x] = uint16_t((coverage << 8) | 0xFFF);
                if (coverage >= 240) {
                    sampleX = int(x);
                    sampleY = int(y);
                }
            }
        Require(sampleX >= 0, "Original glyph has no opaque sample");
        CGxTex *fontTexture = nullptr;
        GxTexCreate(GxTex_2d, 256, 256, 0, GxTex_Argb4444, GxTex_Argb4444, flags, &atlas, Source::Callback,
                    "original glyph atlas", fontTexture);
        // Render the atlas's first 32x32 texels across the 32x32 fixture target.
        auto vertices = std::vector<Vertex>(std::begin(quad), std::end(quad));
        for (auto &v : vertices) {
            v.u *= 32.0f / 256;
            v.v *= 32.0f / 256;
        }
        GxBufData(buffer, vertices.data(), sizeof(quad), 0);
        GxRsSet(GxRs_Texture0, fontTexture);
        GxRsSet(GxRs_BlendingMode, GxBlend_Alpha);
        GxSceneClear(GxBufferBit_Color, CImVector(0, 0, 0, 255));
        draw();
        // Original UI projection flips FBO Y; sample in the glyph's top-down row.
        pixel = Pixel(device, sampleX, sampleY);
        Require(pixel.r >= 254 && pixel.g >= 254 && pixel.b >= 254,
                "Original glyph through legacy Gx callback/batch pixel mismatch");
        GxTexDestroy(fontTexture);
        std::cout << "original-font-family=" << face.Family()
                  << "\noriginal-font-through-legacy-gx-callback-pct-ui-shader-pixel=passed\n";
    }
    device.DeviceSetRenderTarget(GxBuffers_Color, nullptr, 0);
    GxTexDestroy(target);
    device.ShaderDestroy(vertex[0]);
    device.ShaderDestroy(vertex[1]);
    device.ShaderDestroy(fragment);
    // Leave streams and a locked pool buffer for derived-before-base teardown.
    auto *pool = device.PoolCreate(GxPoolTarget_Vertex, GxPoolUsage_Stream, 128, GxPoolHintBit_Unk0,
                                   "lifetime fixture");
    auto *retained = device.BufCreate(pool, 24, 4, 0);
    device.BufLock(retained);
    device.PoolDestroy(pool);
    Require(!device.m_bufLocked[0], "Destroyed locked pool retained a binding");
}
} // namespace
int main(int argc, char **argv) {
    bool started = false;
    try {
        std::string data, settings, cache;
        for (int i = 1; i < argc; ++i) {
            Require(i + 1 < argc, "Missing option value");
            std::string key = argv[i], value = argv[++i];
            if (key == "--data")
                data = value;
            else if (key == "--settings")
                settings = value;
            else if (key == "--cache")
                cache = value;
            else
                throw std::invalid_argument("Unknown legacy fixture option");
        }
        Require(data.empty() == settings.empty() && data.empty() == cache.empty(),
                "External data requires settings/cache roots");
        Require(!std::fesetenv(FE_DFL_ENV) && !std::fesetround(FE_TONEAREST), "FP initialization failed");
        StormRtlInitialize();
        OsTimeStartup(OsTimeManager::BestAvailable);
        started = true;
        for (int i = 0; i < 3; ++i) {
            {
                GlContext context("M3 legacy Gx dispatch fixture - no login UI", 320, 240, true);
                std::cout << "gl-renderer=" << context.Info().renderer
                          << "\ngl-version=" << context.Info().version << '\n';
                auto assets = Load(context, data, settings, cache);
                Cycle(context, assets);
                Require(!g_theGxDevicePtr, "Legacy device remained installed after teardown");
            }
            Require(!OsLiveThreadCount(), "Worker remained after legacy device shutdown");
            std::cout << "legacy-gx-cycle-closed=" << i + 1 << " workers=0\n";
        }
        OsTimeShutdown();
        StormRtlDestroy();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "LegacyGxCheck failed: " << error.what() << '\n';
        if (started) {
            OsTimeShutdown();
            StormRtlDestroy();
        }
        return 1;
    }
}
