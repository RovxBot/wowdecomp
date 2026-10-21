#include "Engine/Source/Services/Portable/Texture.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Gx/Portable/LegacyDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include <atomic>
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <thread>
using namespace Gx::Native;
using namespace Services::Native;
using Bytes = std::vector<uint8_t>;
namespace {
void Require(bool yes, const char* why) { if (!yes) throw std::runtime_error(why); }
template<class F> void Reject(F f) {
    bool failed = false; try { f(); } catch (const std::exception&) { failed = true; }
    Require(failed, "Unavailable image operation accepted");
}
void Word(Bytes& out, size_t at, uint32_t value) {
    for (size_t b = 0; b < 4; ++b) out.at(at + b) = uint8_t(value >> (8 * b));
}
Bytes Shader(const char* program, uint32_t count = 1) {
    Bytes out(12); Word(out, 0, 0x47585348); Word(out, 4, 0x10003);
    for (uint32_t i = 0; i < count; ++i) {
        auto base = out.size(); out.resize(base + 16);
        Word(out, base + 12, uint32_t(std::strlen(program) + 1));
        out.insert(out.end(), program, program + std::strlen(program) + 1);
        while (out.size() % 4) out.push_back(0);
    }
    return out;
}
Bytes Blp(uint8_t encoding, uint8_t alpha, uint8_t format, uint32_t w, uint32_t h, Bytes pixels) {
    Bytes out(encoding == 1 ? 1172 : 148);
    Word(out, 0, 0x32504c42); Word(out, 4, 1);
    out[8] = encoding; out[9] = alpha; out[10] = format;
    Word(out, 12, w); Word(out, 16, h);
    Word(out, 20, uint32_t(out.size())); Word(out, 84, uint32_t(pixels.size()));
    out.insert(out.end(), pixels.begin(), pixels.end()); return out;
}
Bytes Raw() {
    // Original Gx requires at least 8x8. BGRA quadrants: red/green, blue/white.
    Bytes pixels;
    for (uint32_t y = 0; y < 8; ++y) for (uint32_t x = 0; x < 8; ++x) {
        const uint8_t bgra[]{uint8_t(y >= 4 ? 255 : 0), uint8_t(x >= 4 ? 255 : 0),
                             uint8_t((y < 4 && x < 4) || (y >= 4 && x >= 4) ? 255 : 0), 255};
        pixels.insert(pixels.end(), bgra, bgra + 4);
    }
    return Blp(3, 8, 0, 8, 8, pixels);
}
Bytes Mipped() {
    // Each authored level has a different color; minification must use it.
    auto out = Blp(3, 8, 0, 8, 8, {});
    const uint8_t bgra[][4]{{0,0,255,255}, {0,255,0,255}, {255,0,0,255}, {0,255,255,128}};
    for (uint32_t level = 0; level < 4; ++level) {
        const auto side = 8u >> level;
        Word(out, 20 + level*4, uint32_t(out.size())); Word(out, 84 + level*4, side*side*4);
        for (uint32_t i = 0; i < side*side; ++i) out.insert(out.end(), bgra[level], bgra[level] + 4);
    }
    out[11] = 1;
    return out;
}
Bytes Palette() {
    Bytes pixels(96); std::fill(pixels.begin() + 64, pixels.end(), 0xFF);
    auto out = Blp(1, 4, 0, 8, 8, pixels);
    out[148] = 23; out[149] = 91; out[150] = 177; return out;
}
Bytes Dxt(uint8_t format) {
    Bytes pixels(format ? 16 : 8);
    if (format == 1) std::fill(pixels.begin(), pixels.begin() + 8, 0x88);
    if (format == 7) { pixels[0] = 128; pixels[1] = 64; }
    size_t offset = format ? 8 : 0;
    pixels[offset] = 0; pixels[offset+1] = 0xF8; // 565 red endpoint, all indices zero.
    pixels[offset+2] = 0xE0; pixels[offset+3] = 7;
    Bytes blocks;
    for (int i = 0; i < 4; ++i) blocks.insert(blocks.end(), pixels.begin(), pixels.end());
    return Blp(2, format ? 8 : 0, format, 8, 8, blocks);
}
struct Handle {
    CTexture* value = nullptr;
    explicit Handle(CTexture* t = nullptr) : value(t) {}
    ~Handle() { Close(); }
    void Close() { if (value) { HandleClose(reinterpret_cast<HOBJECT>(value)); value = nullptr; } }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
struct ShaderOwner {
    LegacyDevice& device;
    CGxShader* vertex[2]{};
    CGxShader* pixel = nullptr;
    ShaderOwner(LegacyDevice& d) : device(d) {
        device.ShaderCreate(vertex, GxSh_Vertex, "Shaders\\Vertex", "UI", 2);
        device.ShaderCreate(&pixel, GxSh_Pixel, "Shaders\\Pixel", "UI", 1);
        GxRsSet(GxRs_VertexShader, vertex[0]); GxRsSet(GxRs_PixelShader, pixel);
    }
    ~ShaderOwner() {
        for (auto& shader : vertex) device.ShaderDestroy(shader);
        device.ShaderDestroy(pixel);
    }
};
struct Target {
    LegacyDevice& device;
    CGxTex* value = nullptr;
    static void Source(EGxTexCommand, uint32_t, uint32_t, uint32_t, uint32_t, void*, uint32_t&, const void*&) {}
    Target(LegacyDevice& d) : device(d) {
        auto flags = CGxTexFlags(GxTex_Nearest); flags.m_renderTarget = 1;
        Require(GxTexCreate(GxTex_2d, 32, 32, 0, GxTex_Argb8888, GxTex_Argb8888,
                           flags, nullptr, Source, "image fixture target", value), "FBO allocation failed");
        device.DeviceSetRenderTarget(GxBuffers_Color, value, 0);
    }
    ~Target() { device.DeviceSetRenderTarget(GxBuffers_Color, nullptr, 0); GxTexDestroy(value); }
};
std::vector<CImVector> Draw(LegacyDevice& device, CTexture* texture, float repeat = 1) {
    GxRsSet(GxRs_Texture0, TextureGetGxTex(texture, 1, nullptr));
    GxRsSet(GxRs_Lighting, 0); GxRsSet(GxRs_Fog, 0); GxRsSet(GxRs_DepthTest, 0);
    GxRsSet(GxRs_Culling, 0); GxRsSet(GxRs_BlendingMode, GxBlend_Opaque);
    C44Matrix matrix; GxXformViewProjNativeTranspose(matrix);
    float constants[] = {matrix.a0,matrix.a1,matrix.a2,matrix.a3,matrix.b0,matrix.b1,matrix.b2,matrix.b3,
                        matrix.c0,matrix.c1,matrix.c2,matrix.c3,matrix.d0,matrix.d1,matrix.d2,matrix.d3};
    GxShaderConstantsSet(GxSh_Vertex, 0, constants, 4);
    struct Vertex { float x,y,z; uint8_t color[4]; float u,v; };
    Vertex quad[] = {{-1,-1,0,{255,255,255,255},0,0}, {1,-1,0,{255,255,255,255},repeat,0},
                           {1,1,0,{255,255,255,255},repeat,repeat}, {-1,1,0,{255,255,255,255},0,repeat}};
    C2Vector offset; float scale;
    if (TextureGetAtlasOffsetAndScale(texture, offset, scale))
        for (auto& vertex : quad) { vertex.u = offset.x + vertex.u * scale; vertex.v = offset.y + vertex.v * scale; }
    auto* buffer = GxBufStream(GxPoolTarget_Vertex, sizeof(Vertex), 4);
    std::memcpy(GxBufLock(buffer), quad, sizeof(quad)); GxBufUnlock(buffer, 0);
    GxPrimVertexPtr(buffer, GxVBF_PCT);
    const uint16_t indices[] = {0,1,2,0,2,3}; GxPrimIndexPtr(6, indices);
    CGxBatch batch{GxPrim_Triangles, 0, 6, 0, 3};
    GxSceneClear(GxBufferBit_Color, CImVector(0,0,0,0)); GxDraw(&batch, 1);
    CiRect rect{}; rect.maxX = rect.maxY = 32;
    TSGrowableArray<CImVector> pixels; device.DeviceReadPixels(rect, pixels);
    return {pixels.Ptr(), pixels.Ptr() + pixels.m_count};
}
void Pixel(const std::vector<CImVector>& pixels, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    const auto& p = pixels.at(y * 32 + x);
    if (p.r != r || p.g != g || p.b != b || p.a != a) {
        std::cerr << "image-pixel=" << x << ',' << y << " actual=" << int(p.r) << ',' << int(p.g) << ','
                  << int(p.b) << ',' << int(p.a) << '\n';
        throw std::runtime_error("Image FBO pixel mismatch");
    }
}
void WaitEntered(std::atomic<bool>& entered) {
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!entered && std::chrono::steady_clock::now() < end) std::this_thread::yield();
    Require(entered, "Worker did not enter controlled read");
}
void Fixture(LegacyDevice& device, FileQueue& queue) {
    const std::map<std::string, Bytes> files{{"INTERFACE\\RAW.BLP", Raw()}, {"INTERFACE\\PALETTE.BLP", Palette()},
        {"INTERFACE\\DXT1.BLP", Dxt(0)}, {"INTERFACE\\DXT3.BLP", Dxt(1)}, {"INTERFACE\\DXT5.BLP", Dxt(7)},
        {"INTERFACE\\MIPPED.BLP", Mipped()},
        {"INTERFACE\\FIRST.BLP", Raw()}, {"INTERFACE\\UI.BLP", Raw()}, {"INTERFACE\\PROMOTED.BLP", Raw()},
        {"INTERFACE\\BROKEN.BLP", Bytes(148)}, {"INTERFACE\\SLOW.BLP", Raw()}};
    std::atomic<unsigned> reads{0};
    std::atomic<bool> entered{false}, release{false};
    std::vector<std::string> readOrder;
    // Always unblock a worker during fixture unwinding, before service teardown.
    TextureService service(queue, [&](const std::string& name, const Cancellation& cancel) {
        ++reads;
        readOrder.push_back(name);
        if (name == "INTERFACE\\SLOW.BLP") {
            entered = true;
            while (!release && !cancel.Requested()) std::this_thread::yield();
        }
        ReadResult result; result.file.bytes = files.at(name); return result;
    });
    struct Unblock { std::atomic<bool>& release; ~Unblock() { release = true; } } unblock{release};
    CStatus status;
    auto create = [&](const char* name, CGxTexFlags flags = CGxTexFlags(GxTex_Nearest), uint32_t options = 1) {
        return TextureCreate(name, flags, &status, options);
    };
    Handle raw(create("Interface/raw"));
    Handle duplicate(create("interface\\RAW.blp"));
    Require(raw.value == duplicate.value && raw.value->m_refcount == 2, "Image cache/refcount identity failed");
    Require(!raw.value->m_asyncObject, "Native ticket leaked into CAsyncObject field");
    uint32_t width = 999, height = 999;
    Require(!TextureGetDimensions(raw.value, &width, &height, 0) && width == 999 && height == 999,
            "Pending dimensions exposed uncompleted data");
    Reject([&] { service.RequireIdle(); });
    bool foreign = false;
    std::thread wrong([&] { try { create("Interface/raw"); } catch (const std::logic_error&) { foreign = true; } });
    wrong.join(); Require(foreign, "Foreign image mutation was accepted");
    Require(TextureGetDimensions(raw.value, &width, &height, 1) && width == 8 && height == 8,
            "File image dimensions failed");
    Require(reads == 1 && TextureIsLoaded(raw.value) && TextureIsOpaque(raw.value), "File image readiness/cache failed");
    auto pixels = Draw(device, raw.value);
    // Original offscreen native projection flips Y. Row zero is at the upper
    // FBO edge; do not cancel that adaptation by changing image upload order.
    Pixel(pixels, 8,24,255,0,0,255); Pixel(pixels,24,24,0,255,0,255);
    Pixel(pixels,8,8,0,0,255,255); Pixel(pixels,24,8,255,255,255,255);
    for (const auto& test : std::vector<std::pair<const char*, CImVector>>{
        // CImVector's original constructor is B,G,R,A.
        {"Interface/palette", CImVector(23,91,177,255)}, {"Interface/dxt1", CImVector(0,0,255,255)},
        {"Interface/dxt3", CImVector(0,0,255,136)}, {"Interface/dxt5", CImVector(0,0,255,128)}}) {
        Handle texture(create(test.first));
        auto rendered = Draw(device, texture.value);
        Pixel(rendered, 16,16,test.second.r,test.second.g,test.second.b,test.second.a);
    }
    Handle wrap(create("Interface/raw", CGxTexFlags(GxTex_Nearest, 1)));
    Handle linear(create("Interface/raw", CGxTexFlags(GxTex_Linear)));
    Require(wrap.value != raw.value && linear.value != raw.value, "Sampler keys aliased file texture cache");
    Require(TextureGetGxTex(wrap.value, 1, nullptr) && TextureGetGxTex(linear.value, 1, nullptr), "Sampler variant load failed");
    for (auto filter : {GxTex_NearestMipNearest, GxTex_LinearMipNearest, GxTex_LinearMipLinear}) {
        Handle mipped(create("Interface/mipped", CGxTexFlags(filter, 1, 1), 0x11));
        Require(TextureGetGxTex(mipped.value, 2, nullptr), "Local-only wait 2 did not finish authored mips");
        Require(!TextureIsOpaque(mipped.value), "Lower-level alpha was discarded from opacity");
        Pixel(Draw(device, mipped.value), 16,16,255,0,0,255);
        Pixel(Draw(device, mipped.value, 8), 16,16,0,255,0,255);
        Pixel(Draw(device, mipped.value, 16), 16,16,0,0,255,255);
        Pixel(Draw(device, mipped.value, 32), 16,16,255,255,0,128);
        if (filter == GxTex_LinearMipLinear) {
            const auto p = Draw(device, mipped.value, 11.3137085f).at(16*32+16);
            Require(p.r <= 2 && std::abs(int(p.g)-128) <= 3 && std::abs(int(p.b)-128) <= 3 && p.a == 255,
                    "Trilinear sampling did not blend authored mip levels");
        }
    }
    {
        Handle incomplete(create("Interface/raw", CGxTexFlags(GxTex_LinearMipLinear)));
        CStatus errors;
        Require(!TextureGetGxTex(incomplete.value, 2, &errors) && !errors.IsEmpty(),
                "Missing authored mip levels fabricated a complete image");
    }
    for (auto name : {"Interface/broken", "Interface/missing", "Interface/percent%s%d%n"}) {
        Handle failed(create(name)); CStatus errors;
        Require(!TextureGetGxTex(failed.value, 1, &errors) && !errors.IsEmpty() && !TextureIsLoaded(failed.value),
                "Failed read/decode fabricated a loaded texture");
        char message[512]; errors.GetErrorStr(message, sizeof(message), STATUS_INFO, " ");
        if (std::strstr(name, "percent"))
            Require(std::strstr(message, "%S%D%N"), "Status copy interpreted filename percent sequences");
        auto entries = errors.GetErrorStrLen(STATUS_INFO, " ");
        errors.Add(errors);
        Require(errors.GetErrorStrLen(STATUS_INFO, " ") == entries, "Status self-copy appended diagnostics");
    }
    Reject([&] { create("../escape"); }); Reject([&] { create("Interface/raw.tga"); });
    Reject([&] { create("Interface/raw", CGxTexFlags(GxTex_Anisotropic)); });
    Reject([&] { create("Interface/raw", CGxTexFlags(), 0x20); });
    Reject([&] { TextureGetGxTex(raw.value, 3, nullptr); });
    Reject([&] { TextureGetGxTex(raw.value, -1, nullptr); });
    {
        Handle slow(create("Interface/slow")); WaitEntered(entered);
        // Close during the actual worker read, then recreate the same cache key.
        slow.Close();
        Handle replacement(create("Interface/slow"));
        release = true;
        Require(TextureGetGxTex(replacement.value, 1, nullptr), "Recreated image lost callback ownership");
        Pixel(Draw(device, replacement.value), 8,24,255,0,0,255);
    }
    {
        std::atomic<bool> blockerEntered{false}, blockerRelease{false};
        auto blocker = queue.Submit([&](const Cancellation& cancel) {
            blockerEntered = true;
            while (!blockerRelease && !cancel.Requested()) std::this_thread::yield();
            return ReadResult{};
        }, [](const ReadResult&) {});
        struct Release { std::atomic<bool>& value; ~Release() { value = true; } } cleanup{blockerRelease};
        WaitEntered(blockerEntered);
        Handle first(create("Interface/first"));
        Handle promoted(create("Interface/promoted"));
        Handle ui(create("Interface/ui", CGxTexFlags(GxTex_Nearest), 0x19));
        Handle priorityDuplicate(create("Interface/promoted", CGxTexFlags(GxTex_Nearest), 0x11));
        Require(priorityDuplicate.value == promoted.value, "Priority request split cache/handle identity");
        Handle queued(create("Interface/palette"));
        auto before = reads.load();
        auto orderStart = readOrder.size(); // Worker remains inside the blocker.
        Require(!TextureGetGxTex(first.value, 0, nullptr), "Query waited for a queued image");
        Require(service.Cancel(queued.value), "Queued image cancellation failed");
        blockerRelease = true;
        CStatus errors;
        Require(!TextureGetGxTex(queued.value, 1, &errors) && !errors.IsEmpty(), "Cancellation fabricated GPU image");
        Require(queue.WaitAll(std::chrono::seconds(5)) && queue.Wait(blocker, std::chrono::seconds(5)) && reads == before+3,
                "Queued priority/cancel reads did not finish once each");
        Require(readOrder.size() == orderStart+3 && readOrder[orderStart] == "INTERFACE\\PROMOTED.BLP" &&
                readOrder[orderStart+1] == "INTERFACE\\UI.BLP" && readOrder[orderStart+2] == "INTERFACE\\FIRST.BLP",
                "Original UI priority or cached priority promotion was lost");
    }
    duplicate.Close(); raw.Close(); wrap.Close(); linear.Close();
    Require(queue.WaitAll(std::chrono::seconds(5)), "Image fixture callbacks did not drain");
    service.Poll(); service.RequireIdle();
    std::cout << "image-cache-read-cancel-failure-pct-fbo=passed reads=" << reads << '\n'
              << "image-authored-mips-filters-local-wait2-priority=passed\n";
}
void SourcesAndAtlas(LegacyDevice& device, FileQueue& queue) {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("wow-native-image-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(fs::create_directory(root), "Temporary loose-image root failed");
    struct Remove { fs::path root; ~Remove() { std::error_code e; fs::remove_all(root, e); } } remove{root};
    fs::create_directory(root / "Interface");
    auto write = [&](const char* name, const Bytes& bytes) {
        std::ofstream out(root / "Interface" / name, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        Require(bool(out), "Loose-image fixture write failed");
    };
    write("Disk.blp", Palette()); write("Broken.blp", Bytes(148));
    write("Case.blp", Raw()); write("case.BLP", Palette());
    fs::create_directory(root / "Interface" / "Directory.blp");
    fs::create_symlink(root.parent_path(), root / "Interface" / "Escape.blp");
    std::atomic<unsigned> packageReads{0};
    TextureService service(queue, [&](const std::string& name, const Cancellation&) {
        ++packageReads;
        ReadResult result;
        auto bytes = name.find("REPLACEMENT") != std::string::npos ? Palette() :
            name.find("MIPPED") != std::string::npos ? Mipped() : Raw();
        if (name.find("ATLAS") != std::string::npos) bytes[11] |= 0x10;
        result.file.bytes = std::move(bytes); return result;
    }, root.u8string());
    CStatus status;
    auto create = [&](const char* name, CGxTexFlags flags = CGxTexFlags(GxTex_Nearest), uint32_t options = 0x17) {
        return TextureCreate(name, flags, &status, options);
    };
    {
        Handle package(create("Interface/Disk", CGxTexFlags(GxTex_Nearest), 1));
        Handle disk(create("Interface/dIsK"));
        Handle duplicate(create("interface/DISK.BLP"));
        Require(package.value != disk.value && duplicate.value == disk.value, "Disk policy aliased cache identity");
        Pixel(Draw(device, disk.value), 16,16,177,91,23,255);
        Require(packageReads == 0, "Loose image unexpectedly read packages");
        Pixel(Draw(device, package.value), 8,24,255,0,0,255);
        Require(packageReads == 1 && TextureIsSame(disk.value, "interface/Disk.blp"), "Image source/name policy failed");
        Handle absent(create("Interface/Absent"));
        Require(TextureGetGxTex(absent.value, 2, &status) && packageReads == 2, "Absent disk image did not use packages");
        for (auto name : {"Interface/Broken", "Interface/Case", "Interface/Directory", "Interface/Escape"}) {
            auto before = packageReads.load();
            Handle failed(create(name)); CStatus errors;
            Require(!TextureGetGxTex(failed.value, 2, &errors) && !errors.IsEmpty() && packageReads == before,
                    "Invalid loose image silently selected package bytes");
        }
    }
    TextureEnableAtlas(1);
    {
        std::vector<std::unique_ptr<Handle>> members;
        for (int i = 0; i < 12; ++i) {
            auto name = "Interface/Atlas" + std::to_string(i);
            members.push_back(std::make_unique<Handle>(create(name.c_str())));
            auto* texture = members.back()->value;
            Require(TextureGetGxTex(texture, 2, &status) && TextureOnAtlas(texture), "Marked BLP did not enter atlas");
            Require(texture->m_atlasBlockIndex == i && texture->m_gxWidth == 8 && texture->m_gxHeight == 8,
                    "Original atlas slot/dimension accounting failed");
            Require(texture->m_atlas == members.front()->value->m_atlas && !texture->m_gxTex,
                    "Eligible images did not share atlas ownership");
            C2Vector offset; float scale = 0;
            Require(TextureGetAtlasOffsetAndScale(texture, offset, scale) && scale == 0.125f &&
                offset.x == float(i & 7) * scale && offset.y == float(i >> 3) * scale, "Atlas UV coordinates changed");
            auto pixels = Draw(device, texture);
            Pixel(pixels,8,24,255,0,0,255); Pixel(pixels,24,8,255,255,255,255);
        }
        auto* sheet = members.front()->value->m_atlas;
        members[3]->Close();
        Require(sheet->m_used == 11, "Closing atlas member destroyed siblings");
        Handle reused(create("Interface/AtlasReplacement"));
        Require(TextureGetGxTex(reused.value, 2, &status) && reused.value->m_atlas == sheet &&
            reused.value->m_atlasBlockIndex == 3, "Freed atlas slot was not reused");
        Pixel(Draw(device, members[9]->value),8,24,255,0,0,255);
        Pixel(Draw(device, reused.value),24,24,177,91,23,255);
        Handle unmarked(create("Interface/Raw"));
        Handle repeat(create("Interface/AtlasRepeat", CGxTexFlags(GxTex_Nearest, 1, 1)));
        Handle mipped(create("Interface/AtlasMipped", CGxTexFlags(GxTex_LinearMipLinear)));
        for (auto* t : {unmarked.value, repeat.value, mipped.value})
            Require(TextureGetGxTex(t, 2, &status) && !TextureOnAtlas(t), "Ineligible image entered atlas");
        Pixel(Draw(device, mipped.value, 8),16,16,0,255,0,255);
        Handle linear(create("Interface/AtlasLinear", CGxTexFlags(GxTex_Linear)));
        Require(TextureGetGxTex(linear.value, 2, &status) && TextureOnAtlas(linear.value) &&
                linear.value->m_atlas != sheet, "Atlas sampler classes aliased");
        auto rendered = Draw(device, linear.value);
        Pixel(rendered,4,28,255,0,0,255); Pixel(rendered,28,4,255,255,255,255);
        TextureEnableAtlas(0);
        Handle disabled(create("Interface/Atlas0"));
        Require(TextureGetGxTex(disabled.value, 2, &status) && !TextureOnAtlas(disabled.value) &&
                disabled.value != members.front()->value, "Atlas toggle changed live cache representation");
        Require(TextureOnAtlas(members.front()->value), "Disabling atlases invalidated existing members");
    }
    {
        Handle small(create("Interface/Raw", CGxTexFlags(GxTex_Anisotropic), 0));
        Require(TextureGetGxTex(small.value, 2, &status) && small.value->m_gxTexFlags.m_filter == GxTex_Nearest,
                "Original small-BLP default policy failed");
        Handle mipped(create("Interface/Mipped", CGxTexFlags(GxTex_Nearest), 0));
        Require(TextureGetGxTex(mipped.value, 2, &status) && mipped.value->m_gxTexFlags.m_filter == GxTex_LinearMipNearest,
                "Original default mip filter failed");
        TextureSetFilterMode(GxTex_Linear);
        Handle changed(create("Interface/Mipped", CGxTexFlags(GxTex_Nearest), 0));
        Require(TextureGetGxTex(changed.value, 2, &status) && changed.value != mipped.value &&
                changed.value->m_gxTexFlags.m_filter == GxTex_Linear, "Default-filter cache policy failed");
        Reject([&] { TextureSetFilterMode(GxTex_Anisotropic); });
    }
    Require(queue.WaitAll(std::chrono::seconds(5)) && status.IsEmpty(), "Image source/atlas callbacks failed");
    service.Poll(); service.RequireIdle();
    std::cout << "image-disk-root-source-cache-failures=passed\n"
              << "image-original-atlas-grid-sharing-reuse-sampling=passed\n"
              << "image-default-filter-small-blp-mips=passed\n";
}
void Snapshot(const std::string& name, const std::vector<CImVector>& pixels) {
    if (name.empty()) return;
    std::ofstream out(name, std::ios::binary); out << "P6\n32 32\n255\n";
    for (int y=31; y>=0; --y) for (int x=0; x<32; ++x) {
        const auto& p = pixels[y*32+x]; const char rgb[]{char(p.r), char(p.g), char(p.b)}; out.write(rgb, 3);
    }
    Require(bool(out), "Image capture failed");
}
}
int main(int argc, char** argv) {
    bool started = false;
    try {
        std::string data, settings, cache, snapshot;
        for (int i = 1; i < argc; ++i) {
            Require(i+1 < argc, "Missing image option value");
            std::string key = argv[i], value = argv[++i];
            if (key == "--data") data = value; else if (key == "--settings") settings = value;
            else if (key == "--cache") cache = value; else if (key == "--snapshot") snapshot = value;
            else throw std::invalid_argument("Unknown image option");
        }
        Require(data.empty() == settings.empty() && data.empty() == cache.empty(), "Original data requires user roots");
        Require(!std::fesetenv(FE_DFL_ENV) && !std::fesetround(FE_TONEAREST), "FP startup failed");
        StormRtlInitialize(); OsTimeStartup(OsTimeManager::BestAvailable); started = true;
        for (int cycle = 0; cycle < 3; ++cycle) {
            {
                GlContext context("File-backed original image Services", 640,480,true);
                std::cout << "gl-renderer=" << context.Info().renderer << "\ngl-version=" << context.Info().version << '\n';
                FileQueue queue;
                std::shared_ptr<Blizzard::Mopaq::Native::Packages> packages;
                std::map<std::string, Bytes> shaders;
                if (data.empty()) {
                    shaders["Shaders\\Vertex\\arbvp1\\UI.bls"] = Shader("!!ARBvp1.0\nDP4 result.position.x,program.env[0],vertex.position;\nDP4 result.position.y,program.env[1],vertex.position;\nDP4 result.position.z,program.env[2],vertex.position;\nDP4 result.position.w,program.env[3],vertex.position;\nMOV result.color,vertex.attrib[4];\nMOV result.texcoord[0],vertex.attrib[6];\nEND\n", 2);
                    shaders["Shaders\\Pixel\\arbfp1\\UI.bls"] = Shader("!!ARBfp1.0\nTEMP t;\nTEX t,fragment.texcoord[0],texture[0],2D;\nMUL result.color,t,fragment.color;\nEND\n");
                } else {
                    Blizzard::File::Native::Paths paths;
                    Require(Blizzard::File::Native::ConfigurePaths(data.c_str(), settings.c_str(), cache.c_str(), paths), "Invalid external roots");
                    auto ticket = queue.Submit([&](const Cancellation&) {
                        packages = Blizzard::Mopaq::Native::Packages::Open12340(paths, "enUS");
                        for (auto name : {"Shaders\\Vertex\\arbvp1\\UI.bls", "Shaders\\Pixel\\arbfp1\\UI.bls"}) {
                            auto file = packages->Read(name);
                            std::cout << "original-member=" << name << " registered-md5=" << file.signedFile << '\n';
                            shaders[name] = std::move(file.bytes);
                        }
                        return ReadResult{};
                    }, [](const ReadResult& r) { if (r.status != ReadStatus::Success) throw std::runtime_error(r.error); });
                    Require(queue.Wait(ticket, std::chrono::seconds(60)), "Original shader read timed out");
                    std::cout << "original-signaturefile=authenticated records=" << packages->SignatureCount() << '\n';
                }
                {
                    LegacyDevice device(context, [&](const std::string& name) { return shaders.at(name); });
                    ShaderOwner programs(device); Target target(device);
                    Fixture(device, queue);
                    SourcesAndAtlas(device, queue);
                    if (packages) {
                        TextureService service(queue, packages);
                        TextureEnableAtlas(1);
                        CStatus status;
                        Handle texture(TextureCreate("Interface\\Glues\\LoadingBar\\Loading-BarFill.blp", CGxTexFlags(GxTex_Linear), &status, 0x17));
                        Require(TextureGetGxTex(texture.value, 1, &status) && status.IsEmpty(), "Original file image failed");
                        auto first = Draw(device, texture.value), second = Draw(device, texture.value);
                        Require(first.size() == second.size() && !std::memcmp(first.data(), second.data(), first.size()*sizeof(CImVector)), "Original repeat pixels changed");
                        Require(std::any_of(first.begin(), first.end(), [](const CImVector& p) { return p.r || p.g || p.b; }), "Original image rendered black");
                        Snapshot(cycle ? "" : snapshot, first);
                        std::cout << "original-services-blp-pct-shader-fbo=passed width=" << texture.value->m_gxWidth
                                  << " height=" << texture.value->m_gxHeight << " atlas=" << TextureOnAtlas(texture.value) << '\n';
                        Handle terrain(TextureCreate("Tileset\\Elwynn\\ElwynnDirtBase.blp",
                            CGxTexFlags(GxTex_LinearMipLinear, 1, 1), &status, 0x19));
                        Require(TextureGetGxTex(terrain.value, 2, &status) && status.IsEmpty(),
                                "Original authored mip-chain image failed");
                        auto minified = Draw(device, terrain.value, 8);
                        Require(std::any_of(minified.begin(), minified.end(), [](const CImVector& p) { return p.r || p.g || p.b; }),
                                "Original minified mip-chain image rendered black");
                        auto repeated = Draw(device, terrain.value, 8);
                        Require(!std::memcmp(minified.data(), repeated.data(), minified.size()*sizeof(CImVector)),
                                "Original minified mip-chain pixels changed");
                        std::cout << "original-services-authored-mips-trilinear-local-wait2=passed width="
                                  << terrain.value->m_gxWidth << " height=" << terrain.value->m_gxHeight << '\n';
                    }
                }
                queue.Shutdown();
            }
            Require(!OsLiveThreadCount(), "Image shutdown left workers");
            std::cout << "image-services-cycle-closed=" << cycle+1 << " workers=0\n";
        }
        OsTimeShutdown(); StormRtlDestroy(); return 0;
    } catch (const std::exception& error) {
        std::cerr << "NativeImageCheck failed: " << error.what() << '\n';
        if (started) { OsTimeShutdown(); StormRtlDestroy(); }
        return 1;
    }
}
