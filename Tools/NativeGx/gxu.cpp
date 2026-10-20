#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/Portable/LegacyDevice.h"
#include "Engine/Source/Gxu/Portable/LegacyFonts.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Gxu/GxuFontFace.h"
#include "Engine/Source/Gxu/GxuFontFont.h"
#include "Engine/Source/Gxu/GxuFontMiscClasses.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Services/Portable/AsyncFile.h"
#include "Engine/Source/Services/TextBlock.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/CMath.h"
#include <algorithm>
#include <cfenv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <limits>
#include <thread>
using namespace Gx::Native;
using Bytes = std::vector<uint8_t>;
namespace {
void Require(bool yes, const char* why) { if (!yes) throw std::runtime_error(why); }
template<class F> void Reject(F f) {
    bool rejected=false; try { f(); } catch(const std::exception&) { rejected=true; }
    Require(rejected, "Invalid Gxu operation accepted");
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
struct Assets {
    std::map<std::string, Bytes> shaders;
    Bytes font;
    size_t signatures = 0;
    bool original = false;
};
Assets Load(GlContext &context, const std::string &data, const std::string &settings,
            const std::string &cache, const Bytes& fixture) {
    Assets assets;
    if (data.empty()) {
        assets.font = fixture;
        assets.shaders["Shaders\\Vertex\\arbvp1\\UI.bls"] =
            Package("!!ARBvp1.0\nDP4 result.position.x, program.env[0], vertex.position;\nDP4 result.position.y, program.env[1], vertex.position;\nDP4 result.position.z, program.env[2], vertex.position;\nDP4 result.position.w, program.env[3], vertex.position;\nMOV result.color, "
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
struct FaceHandle {
    HFACE value = nullptr;
    ~FaceHandle() { if (value) FontFaceCloseHandle(value); }
};
struct Font {
    CGxFont* value = nullptr;
    ~Font() { GxuFontDestroyFont(value); }
};
struct String {
    CGxString* value = nullptr;
    ~String() { GxuFontDestroyString(value); }
};
struct Batch {
    CGxStringBatch* value = GxuFontCreateBatch(false, false);
    ~Batch() { GxuFontDestroyBatch(value); }
};
struct Handle {
    HOBJECT value;
    ~Handle() { if (value) HandleClose(value); }
};
std::vector<CImVector> Pixels(LegacyDevice& device) {
    CiRect rect{};
    rect.maxX = 640; rect.maxY = 480;
    TSGrowableArray<CImVector> pixels;
    device.DeviceReadPixels(rect, pixels);
    return {pixels.Ptr(), pixels.Ptr() + pixels.m_count};
}
void SamePixels(const std::vector<CImVector>& before, const std::vector<CImVector>& after, const char* why) {
    Require(before.size() == after.size(), "Gxu readback size changed");
    for (size_t i = 0; i < before.size(); ++i) if (std::memcmp(&before[i], &after[i], 4)) {
        std::cerr << "gxu-pixel-difference x=" << i % 640 << " y=" << i / 640
                  << " before=" << int(before[i].r) << ',' << int(before[i].g) << ',' << int(before[i].b)
                  << " after=" << int(after[i].r) << ',' << int(after[i].g) << ',' << int(after[i].b) << '\n';
        throw std::runtime_error(why);
    }
}
void Snapshot(const std::string& path, const std::vector<CImVector>& pixels) {
    if (path.empty()) return;
    std::ofstream out(path, std::ios::binary);
    out << "P6\n640 480\n255\n";
    for (int y = 479; y >= 0; --y) for (int x = 0; x < 640; ++x) {
        const auto& p = pixels[y * 640 + x];
        const char rgb[] = {char(p.r), char(p.g), char(p.b)};
        out.write(rgb, 3);
    }
    Require(bool(out), "Gxu snapshot write failed");
}
void Cycle(GlContext& context, Assets& assets, const std::string& snapshot) {
    LegacyDevice device(context, [&](const std::string& path) { return assets.shaders.at(path); });
    Require(device.Caps().m_pixelCenterOnEdge == 1 && device.Caps().m_texelCenterOnEdge == 1,
            "Native Gxu requires original OpenGL pixel/texel-center semantics");
    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_Fog, 0);
    int reads = 0;
    Gxu::Native::LegacyFonts fonts([&](const std::string& path) {
        ++reads;
        return path == "bad.ttf" ? Bytes{0,1,0,0,0,0,0,0} : assets.font;
    });
    Require(g_FTLibrary && g_widthPixels == 640 && g_heightPixels == 480,
            "Original Gxu globals/window initialization failed");
    Reject([&] { Gxu::Native::LegacyFonts duplicate([](const std::string&) { return Bytes{}; }); });
    Require(!FontFaceGetHandle("bad.ttf", g_FTLibrary), "Malformed face accepted");
    Require(!s_faceHash.m_fulllist.Head(), "Malformed face left a hash entry");
    const char* name = assets.original ? "Fonts\\FRIZQT__.TTF" : "Fonts\\fixture.ttf";
    {
        FaceHandle one{FontFaceGetHandle(name, g_FTLibrary)};
        Require(one.value, "Original HFACE acquisition failed");
        int before = reads;
        FaceHandle two{FontFaceGetHandle(assets.original ? "FONTS\\frizqt__.ttf" : "FONTS\\FIXTURE.TTF", g_FTLibrary)};
        Require(two.value == one.value && reads == before, "Original face hash did not share handles");
        Require(HandleDereference(reinterpret_cast<HOBJECT>(one.value))->m_refcount == 3,
                "Original self/duplicate handle ownership changed");
        Require(FontFaceGetFace(one.value) && FontFaceGetFontName(one.value), "HFACE lookup failed");
    }
    Require(!s_faceHash.m_fulllist.Head(), "Last external face close did not release self/cache");
    bool rejected = false;
    std::thread wrong([&] {
        try { CGxFont* unused = nullptr; GxuFontCreateFont(name, 0.05f, unused, 0); }
        catch (const std::logic_error&) { rejected = true; }
    });
    wrong.join();
    Require(rejected, "Off-thread Gxu mutation accepted");

    Font face;
    const float height = 32.0f / 480;
    Require(GxuFontCreateFont(name, height, face.value, 0), "Original CGxFont creation failed");
    auto* a = face.value->NewCodeDesc('A');
    auto* v = face.value->NewCodeDesc('V');
    Require(a && v && face.value->NewCodeDesc('A') == a, "Glyph hash/LRU reuse failed");
    rejected = false;
    std::thread wrongCache([&] {
        try { face.value->NewCodeDesc('A'); }
        catch (const std::logic_error&) { rejected = true; }
    });
    wrongCache.join();
    Require(rejected, "Off-thread cached glyph mutation accepted");
    Require(face.value->ComputeStep('A','V') <= a->m_bitmapData.m_glyphAdvance,
            "Original kerning cache increased AV spacing");
    float extent = 0, marked = 0;
    GxuFontGetTextExtent(face.value, "AV", 2, height, &extent, 0, 1, 0, 0);
    GxuFontGetTextExtent(face.value, "|cffff0000AV|r", 14, height, &marked, 0, 1, 0, 0);
    Require(extent > 0 && std::abs(extent - marked) < 0.00001f, "Original rich-text extent changed");
    uint32_t starts[4]{};
    auto lines = GxuFontWrapText(face.value, "AV\nAV", 5, height, 1.0f, starts, 4, 0, 1, 0, 0);
    Require(lines == 2 && starts[0] == 0 && starts[1] == 3, "Original newline wrapping failed");
    String text;
    Require(GxuFontCreateString(face.value, "|cffff0000A|rV", height, C3Vector(0.1f,0.1f,0),
            0.7f, 0.7f, 0, text.value, GxVJ_Bottom, GxHJ_Left, 0,
            CImVector(255,255,255,255), 0, 1), "Original CGxString creation failed");
    Require(text.value->CheckGeometry(), "Original string geometry is empty");
    Batch batch;
    Require(GxuFontAddToBatch(batch.value, text.value), "Original batch hash insertion failed");
    auto render = [&] {
        GxSceneClear(GxBufferBit_Color, CImVector(0,0,0,255));
        GxuFontRenderBatch(batch.value);
        return Pixels(device);
    };
    auto first = render();
    size_t red = 0, white = 0;
    for (const auto& pixel : first) {
        red += pixel.r > 240 && pixel.g < 4 && pixel.b < 4;
        white += pixel.r > 240 && pixel.g > 240 && pixel.b > 240;
    }
    Require(red > 4 && white > 4, "Original Gxu layout/atlas/batch shader colors missing");
    auto second = render();
    SamePixels(first, second, "Cached original Gxu rendering changed pixels");
    Snapshot(snapshot, first);
    Require(!face.value->m_textureCache[0].m_anyDirtyGlyphs, "Atlas upload did not clear dirty state");
    size_t glyphs = 0;
    auto ft = FontFaceGetFace(face.value->m_fontFaceHandle);
    for (uint32_t code = 0x400; code < 0xC00; ++code) {
        if (FT_Get_Char_Index(ft, code) && face.value->NewCodeDesc(code)) ++glyphs;
    }
    if (!assets.original) Require(glyphs == 2048 && text.value->m_textureEvicted,
                                  "Authored eight-page LRU eviction was not exercised");
    auto regenerated = render();
    SamePixels(first, regenerated, "Evicted string cache did not regenerate identical pixels");
    // Clear glyphs while the string remains live, matching resize invalidation.
    face.value->HandleScreenSizeChange();
    auto rebuilt = render();
    SamePixels(first, rebuilt, "Font-size/cache invalidation changed original string pixels");
    // Both original outline paths and mono/gray/space/negative-bearing raster data.
    for (uint32_t flags : {1u, 2u, 3u, 8u, 10u}) {
        Font variant;
        Require(GxuFontCreateFont(name, height, variant.value, flags), "Font style creation failed");
        for (uint32_t code : {'A', 'Q', ' '}) Require(variant.value->NewCodeDesc(code), "Styled glyph failed");
        for (auto& page : variant.value->m_textureCache) page.UpdateDirty();
    }
    {
        // Actual Services handles used by Frame font/string consumers.
        float ddcHeight = NDCToDDCHeight(height);
        Handle serviceFont{reinterpret_cast<HOBJECT>(TextBlockGenerateFont(name, 0, ddcHeight))};
        Handle duplicate{reinterpret_cast<HOBJECT>(TextBlockGenerateFont(name, 0, ddcHeight))};
        Require(serviceFont.value && serviceFont.value == duplicate.value &&
                HandleDereference(serviceFont.value)->m_refcount == 2, "HTEXTFONT cache/refcounts failed");
        rejected = false;
        std::thread wrongHandle([&] {
            try { HandleClose(serviceFont.value); }
            catch (const std::logic_error&) { rejected = true; }
        });
        wrongHandle.join();
        Require(rejected && HandleDereference(serviceFont.value)->m_refcount == 2,
                "Off-thread handle close changed ownership");
        auto hfont = reinterpret_cast<HTEXTFONT>(serviceFont.value);
        Require(TextBlockGetFontPtr(hfont)->m_fontFaceHandle == face.value->m_fontFaceHandle,
                "Services font did not share original HFACE");
        Handle block{reinterpret_cast<HOBJECT>(TextBlockCreate(hfont, "AV", CImVector(255,255,255,255),
            C3Vector(0.1f,0.1f,0), ddcHeight, 0.4f, 0.4f, 0x21, 0, 0, 1))};
        Require(block.value, "HTEXTBLOCK creation failed");
        auto hblock = reinterpret_cast<HTEXTBLOCK>(block.value);
        float serviceExtent = 0;
        TextBlockGetTextExtent(hfont, "AV", 2, ddcHeight, &serviceExtent, 0, 1, 0, 0);
        Require(std::abs(serviceExtent - NDCToDDCWidth(extent)) < 0.00001f,
                "Services text coordinate conversion changed extent");
        Batch serviceBatch;
        GxuFontAddToBatch(serviceBatch.value, TextBlockGetStringPtr(hblock));
        GxSceneClear(GxBufferBit_Color, CImVector(0,0,0,255));
        GxuFontRenderBatch(serviceBatch.value);
        auto servicePixels = Pixels(device);
        Require(std::count_if(servicePixels.begin(), servicePixels.end(), [](const CImVector& p) {
            return p.r > 240 && p.g > 240 && p.b > 240;
        }) > 4, "Services handles did not render through original Gxu");
    }
    GxuFontClearBatch(batch.value);
    GxuFontAddToBatch(batch.value, text.value);
    GxuFontUpdate();
    std::cout << "gxu-hface-layout-glyph-lru-atlas-batch-regeneration=passed red=" << red
              << " white=" << white << " stress-glyphs=" << glyphs
              << " original-data=" << assets.original << '\n';
}
} // namespace
int main(int argc, char** argv) {
    bool started = false;
    try {
        std::string data, settings, cache, font, snapshot;
        for (int i = 1; i < argc; ++i) {
            Require(i + 1 < argc, "Missing option value");
            std::string key = argv[i], value = argv[++i];
            if (key == "--data") data = value;
            else if (key == "--settings") settings = value;
            else if (key == "--cache") cache = value;
            else if (key == "--font") font = value;
            else if (key == "--snapshot") snapshot = value;
            else throw std::invalid_argument("Unknown Gxu fixture option");
        }
        Require(data.empty() == settings.empty() && data.empty() == cache.empty(),
                "External data requires separate settings/cache roots");
        Require(!data.empty() || !font.empty(), "Synthetic Gxu checks require the authored cache font");
        Bytes fixture;
        if (data.empty()) {
            std::ifstream input(font, std::ios::binary);
            fixture.assign(std::istreambuf_iterator<char>(input), {});
        }
        Require(!std::fesetenv(FE_DFL_ENV) && !std::fesetround(FE_TONEAREST), "FP startup failed");
        StormRtlInitialize(); OsTimeStartup(OsTimeManager::BestAvailable); started = true;
        Require(CMath::fint_n(0.5f) == 1 && CMath::fint_n(-0.5f) == -1 &&
                CMath::fuint_pi(0.25f) == 1 && CMath::fuint_pi(-0.25f) == 0 &&
                CeilToInt64(3.0f) == 3 && CeilToInt64(0.00001f) == 0,
                "Original pixel rounding/epsilon changed");
        Reject([] { CMath::fint_n(std::numeric_limits<float>::infinity()); });
        Reject([] { CMath::fuint_pi(std::numeric_limits<float>::quiet_NaN()); });
        for (int cycle = 0; cycle < 3; ++cycle) {
            {
                GlContext context("Original Gxu integration fixture", 640, 480, true);
                std::cout << "gl-renderer=" << context.Info().renderer << "\ngl-version=" << context.Info().version << '\n';
                auto assets = Load(context, data, settings, cache, fixture);
                Cycle(context, assets, cycle ? "" : snapshot);
                Require(!g_FTLibrary && !g_fontPixelShader && !s_faceHash.m_fulllist.Head(),
                        "Original Gxu shutdown left face/library/shader state");
            }
            Require(!OsLiveThreadCount(), "Gxu shutdown left a worker");
            std::cout << "legacy-gxu-cycle-closed=" << cycle + 1 << " workers=0\n";
        }
        OsTimeShutdown(); StormRtlDestroy(); return 0;
    } catch (const std::exception& e) {
        std::cerr << "LegacyGxuCheck failed: " << e.what() << '\n';
        if (started) { OsTimeShutdown(); StormRtlDestroy(); }
        return 1;
    }
}
